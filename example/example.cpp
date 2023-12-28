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

        zmq::message_t response("NOT YET REALIZED");
        socket.send(response);
    }

    virtual void handle_push(zmq::socket_t& socket)
    {
        zmq::message_t msg;
        socket.recv(msg);

        std::cout << "PULL: " << msg.to_string() << std::endl;
    }

    virtual void handle_finished(child_ptr child) {
        std::cout << "FINISHED: " << child->uid << std::endl;
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

    auto root = util::path_find_parent(__FILE__);
    auto file = util::path_append(root, "pyscripts.py");

    auto cmd  = boost::str(boost::format(
        R"(python.exe "%s" --uid "{uid}" --req "{req}" --sub "{sub}" --push "{push}")")
        % util::conv::easy::_2utf8(file));

    std::error_code ecode;
    subprocesses children;
    for (int i = 0; i < 1; ++i)
        children.launch(cmd, boost::uuids::to_string(boost::uuids::random_generator()()), ecode);

    while (gFlags == kFlagRunning)
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

    children.publish(zmq::message_t("stop"));
    children.shutdown();
    children.wait_for();

    ::system("pause");
}