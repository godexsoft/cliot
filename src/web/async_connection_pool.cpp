#include <util/async_queue.hpp>
#include <web/async_connection_pool.hpp>
#include <web/web_socket_session.hpp>

#include <boost/asio/io_context.hpp>
#include <fmt/compile.h>

#include <exception>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

AsyncConnectionPool::AsyncConnectionPool(std::string const &host, std::string const &port)
    : work_{ ctx_.get_executor() }
    , available_pool_{ 4, [](std::shared_ptr<WebSocketSession> &ses) { ses->close(); } } {
    for(auto i = 0; i < 4; ++i)
        workers_.emplace_back(std::bind_front(&AsyncConnectionPool::worker_loop, this));
    for(auto i = 0; i < 4; ++i)
        available_pool_.enqueue(std::make_shared<WebSocketSession>(ctx_, host, port));
}

AsyncConnectionPool::~AsyncConnectionPool() {
    stop();
    for(auto &worker : workers_)
        worker.join();
}

// potentially blocks
AsyncConnectionPool::shared_link_t AsyncConnectionPool::borrow() {
    auto ws = available_pool_.dequeue();
    if(!ws)
        throw std::runtime_error("Could not borrow ws connection");

    return std::make_shared<ConnectionLink>(*ws,
        [this, ws = *ws]() {
            available_pool_.enqueue(ws);
        });
}

void AsyncConnectionPool::worker_loop() {
    try {
        ctx_.run();
    } catch(std::exception const &e) {
        fmt::print("Exception on asio worker thread: {}\n", e.what());
    }
}

void AsyncConnectionPool::stop() {
    available_pool_.stop();
    work_.reset();
}
