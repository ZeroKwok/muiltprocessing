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

#ifndef muiltprocessing_h__
#define muiltprocessing_h__

#include <set>
#include <string>
#include <vector>
#include <chrono>
#include <memory>
#include <zmq.hpp>
#include <boost/format.hpp>
#include <boost/thread.hpp>
#include <boost/process.hpp>
#include <boost/process/windows.hpp>
#include <boost/algorithm/string.hpp>

#include "platform/platform_util.h"

class muiltprocessing
{
protected:
    struct child {
        std::string uid;
        std::shared_ptr<boost::process::child> process;
    };
    typedef std::shared_ptr<child> child_ptr;

    unsigned short                  m_preferredPort;
    std::mutex                      m_mutex;
    std::atomic_bool                m_intrrupted;
    std::set<child_ptr>             m_children;
    std::vector<std::string>        m_addresses;
    std::shared_ptr<zmq::socket_t>  m_pull;
    std::shared_ptr<zmq::socket_t>  m_publish;
    std::shared_ptr<zmq::socket_t>  m_response;
    std::shared_ptr<zmq::context_t> m_iocontext;
    std::shared_ptr<boost::thread>  m_iothread;
public:

    explicit muiltprocessing(uint16_t preferredPort = 5555)
    {
        try
        {
            m_preferredPort = preferredPort;
            m_iocontext = std::make_shared<zmq::context_t>(1);

            m_pull = std::make_shared<zmq::socket_t>(*m_iocontext, ZMQ_PULL);
            m_publish = std::make_shared<zmq::socket_t>(*m_iocontext, ZMQ_PUB);
            m_response = std::make_shared<zmq::socket_t>(*m_iocontext, ZMQ_REP);

            m_addresses.resize(3);
            m_addresses[0] = bind(*m_response, m_preferredPort);
            m_addresses[1] = bind(*m_publish, m_preferredPort + 1);
            m_addresses[2] = bind(*m_pull, m_preferredPort + 2);

            m_intrrupted = false;
            m_iothread = std::make_shared<boost::thread>([=] {
                iothread();
            });
        }
        catch (const std::exception& e)
        {
            util::output_debug_string("*** Warning ***");
            util::output_debug_string("muiltprocessing() failed, error: %s", e.what());
        }
    }

    ~muiltprocessing()
    {
        shutdown();
    }

    muiltprocessing(muiltprocessing&) = delete;
    muiltprocessing& operator=(muiltprocessing&) = delete;

    void shutdown()
    {
        if (m_iocontext)
        {
            m_intrrupted = true;
            m_pull.reset();
            m_publish.reset();
            m_response.reset();
            m_iocontext->close();
            m_iothread->interrupt();

            m_iocontext.reset();
        }
    }

    bool wait_children(int milliseconds = -1, const std::function<void()>& ticker = {})
    {
        auto timeout = std::chrono::steady_clock::now() + 
                       std::chrono::milliseconds(milliseconds);
        while (true)
        {
            size_t size = 0;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                size = m_children.size();
            }

            if (size == 0)
                return true;

            // 如果I/O线程已经先一步退出, 那么这里应该主动轮询所有子进程状态
            if (!m_iothread->joinable() || m_iothread->try_join_for(boost::chrono::milliseconds(0)))
                poll_child();

            if (milliseconds != -1 && timeout < std::chrono::steady_clock::now())
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            if (ticker)
                ticker();
        }

        return false;
    }

    void join()
    {
        if (wait_children())
        {
            if (m_iothread->joinable())
                m_iothread->join();
        }
    }

    bool launch(
        const std::string& cmd,
        const std::string& uid,
        std::error_code& error)
    {
        error.clear();

        auto command = cmd;
        boost::replace_all(command, "{uid}",  uid);
        boost::replace_all(command, "{req}",  m_addresses[0]);
        boost::replace_all(command, "{sub}",  m_addresses[1]);
        boost::replace_all(command, "{push}", m_addresses[2]);

#if IS_DEBUG
        auto show = boost::process::windows::show_normal;
#else
        auto show = boost::process::windows::hide;
#endif
        auto child = std::make_shared<muiltprocessing::child>(
            muiltprocessing::child{
                uid,
                std::make_shared<boost::process::child>(command, show)
            });

        std::unique_lock<std::mutex> lock(m_mutex);
        m_children.insert(child);

        return !error;
    }

    void publish(zmq::message_t& msg)
    {
        if (m_publish)
            m_publish->send(msg);
    }

protected:
    virtual void handle_request(zmq::socket_t& socket) 
    {
        zmq::message_t request;
        socket.recv(request);
        socket.send(zmq::message_t(std::string("NOT YET REALIZED")), 0);
    }

    virtual void handle_push(zmq::socket_t& socket)
    {}

    virtual void handle_finished(child_ptr child)
    {}

protected:
    void iothread()
    {
        try
        {
            zmq::pollitem_t items[] = {
                { *m_response,  0, ZMQ_POLLIN, 0 },
                { *m_pull, 0, ZMQ_POLLIN, 0 }
            };

            auto start    = std::chrono::steady_clock::now();
            auto interval = std::chrono::milliseconds(500);

            while (!m_intrrupted)
            {
                zmq::poll(items, 2, std::chrono::milliseconds(10));
                if (items[0].revents & ZMQ_POLLIN)
                    handle_request(*m_response);
                if (items[1].revents & ZMQ_POLLIN)
                    handle_push(*m_pull);

                auto now = std::chrono::steady_clock::now();
                if (now - start > interval)
                {
                    poll_child();
                    start = now;
                }
            }
        }
        catch (const zmq::error_t& e)
        {
            if (e.num() == ETERM || m_intrrupted)
                return;

            util::output_debug_string("*** Warning ***");
            util::output_debug_string("iothread() interrupted, error_t: 0x%08x, %s", e.num(), e.what());
        }
        catch (const std::exception& e)
        {
            util::output_debug_string("*** Warning ***");
            util::output_debug_string("iothread() interrupted, exception: %s", e.what());
        }
    }

    void poll_child()
    {
        std::set<child_ptr> finished;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            for (auto it = m_children.begin(); it != m_children.end();)
            {
                std::error_code ecode;
                if (!(*it)->process->running(ecode))
                {
                    finished.insert(*it);
                    it = m_children.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }

        for (auto const& child : finished)
            handle_finished(child);
    }

    std::string bind(zmq::socket_t& socket, uint16_t port)
    {
        do
        {
            auto address = boost::str(boost::format("tcp://localhost:%d") % port++);
            try
            {
                socket.bind(address);
                socket.setsockopt(ZMQ_LINGER, int(500));
                return address;
            }
            catch (zmq::error_t e)
            {
                if (e.num() != EADDRINUSE)
                    throw;
            }
        } while (true);

        return {};
    }
};

#endif // muiltprocessing_h__
