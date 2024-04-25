// This file is part of the muiltprocessing distribution.
// Copyright (c) 2023-2024 Zero <zero.kwok@foxmail.com>
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

#if defined(_MSC_VER)
#   pragma warning( push )
#   pragma warning( disable : 4996 )
#   pragma warning( disable : 4834 )
#endif

class muiltprocessing
{
public:
    struct child {
        std::string uid;                                //!< 子进程唯一id
        std::chrono::steady_clock::time_point start;    //!< 子进程开始运行的时间
        std::shared_ptr<boost::process::child> process; //!< 子进程对象
    };
    typedef std::shared_ptr<child> child_ptr;

protected:

    friend class poller;
    class poller
    {
        zmq::pollitem_t  _items[2] = {};
        muiltprocessing* _ptr = nullptr;

    public:
        poller(muiltprocessing* ptr) : _ptr(ptr)
        {
            _items[0] = { *_ptr->m_response, 0, ZMQ_POLLIN, 0 };
            _items[1] = { *_ptr->m_pull, 0, ZMQ_POLLIN, 0 };
        }

        bool poll(int msec = 10)
        {
            try
            {
                while(zmq::poll(_items, 2, std::chrono::milliseconds(msec)) > 0)
                {
                    if (_items[0].revents & ZMQ_POLLIN)
                        _ptr->handle_request(*_ptr->m_response);
                    if (_items[1].revents & ZMQ_POLLIN)
                        _ptr->handle_push(*_ptr->m_pull);
                }
            }
            catch (const zmq::error_t& e)
            {
                if (e.num() != ETERM && !_ptr->m_intrrupted)
                {
                    util::output_debug_string("*** Warning ***");
                    util::output_debug_string("poller.poll() interrupted, error_t: 0x%08x, %s", e.num(), e.what());
                }
                return false;
            }
            return true;
        }
    };

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
    std::shared_ptr<poller>         m_poller;

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
            m_poller = std::make_shared<poller>(this);

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
            m_iothread->interrupt();

            m_pull.reset();
            m_publish.reset();
            m_response.reset();
            m_iocontext->close();
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

    template<typename StdTString>
    child_ptr launch(
        const StdTString& cmd,
        const std::string& uid,
        std::error_code& error)
    {
        error.clear();

        auto command = cmd;
        boost::replace_all(command, "{uid}",  uid);
        boost::replace_all(command, "{req}",  m_addresses[0]);
        boost::replace_all(command, "{sub}",  m_addresses[1]);
        boost::replace_all(command, "{push}", m_addresses[2]);

        try
        {
#if defined(MUILTPROCESSING_SHOW_WINDOWS)
            auto show = boost::process::windows::show_normal;
#else
            auto show = boost::process::windows::hide;
#endif
            auto child = std::make_shared<muiltprocessing::child>(
                muiltprocessing::child{
                    uid,
                    std::chrono::steady_clock::now(),
                    std::make_shared<boost::process::child>(command, show)
                });

            std::unique_lock<std::mutex> lock(m_mutex);
            m_children.insert(child);

            return child;
        }
        catch (const boost::process::process_error& e)
        {
            error = e.code();
        }

        return {};
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
    __RESUME:
        try
        {
            auto start    = std::chrono::steady_clock::now();
            auto interval = std::chrono::milliseconds(500);

            while (!m_intrrupted)
            {
                if (!m_poller->poll())
                    break;

                auto now = std::chrono::steady_clock::now();
                if (now - start > interval) {
                    poll_child();
                    start = now;
                }
            }
        }
        catch (const std::bad_alloc&)
        {
            util::output_debug_string("*** Warning ***");
            util::output_debug_string("iothread() throws a bad allocation and try to continue execution.");

            // 这里出现 bad_alloc 主要是因为进程在其他地方耗尽了内存, 波及到了这里。
            // 为了增强程序的健壮性，我们将在一段时间后继续尝试执行。
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            goto __RESUME;
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
                    // 再次轮询处理子进程的消息, 确保handle_finished()是最后响应的事件.
                    m_poller->poll(10);
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

#if defined(_MSC_VER)
#   pragma warning( pop )
#endif

#endif // muiltprocessing_h__
