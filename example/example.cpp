// This file is part of the muiltprocessing distribution.
// Copyright (c) 2023 zero.kwok@foxmail.com
// 
// This is free software; you can redistribute it and/or modify it
// under the terms of the GNU Lesser General Public License as
// published by the Free Software Foundation; either version 3 of
// the License, or (at your option) any later version.
// 
// This software is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public
// License along with this software; 
// If not, see <http://www.gnu.org/licenses/>.

#include "muiltprocessing.hpp"

#include "common/time_util.h"
#include "string/string_util.h"
#include "string/string_conv_easy.hpp"
#include "filesystem/path_util.h"

#include <signal.h>
#include <iostream>
#include <boost/format.hpp>
#include <boost/uuid/uuid_io.hpp>  
#include <boost/uuid/uuid_generators.hpp>

class subprocesses : public muiltprocessing
{
public:
    subprocesses()
        : muiltprocessing(5555)
    {}

    virtual void handle_request(zmq::socket_t& socket)
    {
        zmq::message_t request;
        socket.recv(request);

        std::cout << "REQUEST: " << request.to_string() << std::endl;

        zmq::message_t response(std::string("NOT YET REALIZED"));
        socket.send(response);
    }

    virtual void handle_push(zmq::socket_t& socket)
    {
        zmq::message_t msg;
        socket.recv(msg);

        std::cout << "PULL: " << msg.to_string() << std::endl;
    }

    virtual void handle_finished(child_ptr child) {
        using namespace std::chrono;
        auto elapse = duration_cast<seconds>(steady_clock::now() - child->start).count();
        std::cout << "FINISHED: " << child->uid << ", Elapse: " << elapse << std::endl;
    }
};

enum {
    kFlagRunning = 0x0,
    kFlagInterrupted = 0x1,
};
static std::atomic_int gFlags(kFlagRunning);

void handle_signal(int signum)
{
    switch (signum)
    {
    case SIGBREAK:
        gFlags |= kFlagInterrupted;
        break;
    }
    signal(signum, handle_signal);
}

int main()
{
    signal(SIGBREAK, handle_signal);

    std::cout << "Press [CTRL + PAUSE/BREAK] to interrupt the operation!" << std::endl;

    auto root = util::path_find_parent(__FILE__);
    auto file = util::path_append(root, "pyscripts.py");

#if 0
    auto cmd  = boost::str(boost::format(
        R"(python.exe "%s" --uid "{uid}" --req "{req}" --sub "{sub}" --push "{push}")")
        % file);
#else
    auto cmd = boost::str(boost::wformat(
        LR"(L:\中文路径٩( 'ω' )و get！\python36\python.exe "%s" --uid "{uid}" --req "{req}" --sub "{sub}" --push "{push}")")
        % util::conv::easy::_2wstr(file));
#endif

    std::error_code ecode;
    subprocesses children;
    for (int i = 0; i < 8; ++i)
        children.launch(cmd, boost::uuids::to_string(boost::uuids::random_generator()()), ecode);

    while (gFlags == kFlagRunning)
    {
        children.publish(zmq::message_t(std::string("TICKTACK")));
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    // 等待所有子进程结束并循环发送终止信号
    while (!children.wait_children(1000))
        children.publish(zmq::message_t(std::string("STOP")));

    children.shutdown();                      // 尝试关闭所有活动
    children.join();                          // 等待所有活动结束

    ::system("pause");
}