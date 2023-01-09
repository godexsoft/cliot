#pragma once

#include <util/async_queue.hpp>
#include <web/web_socket_session.hpp>

#include <boost/asio/io_context.hpp>

#include <exception>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

class AsyncConnectionPool {
    using ws_ptr_t = std::shared_ptr<WebSocketSession>;

    boost::asio::io_context ctx_;
    boost::asio::executor_work_guard<decltype(ctx_.get_executor())> work_;
    util::AsyncQueue<ws_ptr_t> available_pool_;
    std::vector<std::thread> workers_;

public:
    class ConnectionLink {
        ws_ptr_t ws_; // connection that is borrowed by the client
        std::function<void()> cleanup_;

    public:
        template <typename Fn>
        ConnectionLink(ws_ptr_t const &ws, Fn cleanup)
            : ws_{ ws }
            , cleanup_{ cleanup } {
            ws_->ensure_connection_established();
        }
        ~ConnectionLink() {
            cleanup_();
        }

        void write(std::string &&data) {
            ws_->write(std::move(data));
        }

        std::string read_one() {
            // note: blocks until message is received
            return ws_->read_one();
        }
    };

    using shared_link_t = std::shared_ptr<ConnectionLink>;

public:
    explicit AsyncConnectionPool(std::string const &host, std::string const &port);
    ~AsyncConnectionPool();

    /**
     * @brief 
     * 
     * @return shared_link_t 
     */
    shared_link_t borrow();

private:
    void worker_loop();
    void stop();
};
