#pragma once

#include <di.hpp>
#include <fmt/compile.h>
#include <inja/inja.hpp>
#include <util/parse_uri.hpp>

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>

#include <condition_variable>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <queue>
#include <regex>
#include <semaphore>
#include <sstream>
#include <string_view>
#include <vector>

// clang-format off

template <typename T>
concept ConnectionChannel = requires(T a, std::string s) {
    { a->write(std::move(s)) };
    { a->read_one() } -> std::convertible_to<std::string>;
};

template <typename T>
concept ConnectionHandler = requires(T a) {
    { a.borrow() } -> ConnectionChannel;
};

template <typename T>
concept SimpleRequestProvider = requires(T a, std::string s) {
    { a.get(s) } -> std::convertible_to<std::string>;
    { a.post(s) } -> std::convertible_to<std::string>;
};
// clang-format on

template <typename T>
class AsyncQueue {
public:
    template <typename Fn>
    AsyncQueue(
        std::size_t const capacity, Fn deleter = [](T &) {})
        : capacity_{ capacity }
        , deleter_{ deleter } { }

    void enqueue(T const &element) {
        std::unique_lock l{ mtx_ };
        cv_.wait(l, [this] { return q_.size() < capacity_ || stop_requested_; });

        if(stop_requested_)
            return;

        q_.push(element);
        cv_.notify_all();
    }

    void stop() {
        std::unique_lock l{ mtx_ };
        stop_requested_ = true;
        while(not q_.empty()) {
            deleter_(q_.front());
            q_.pop();
        }
        cv_.notify_all();
    }

    [[nodiscard]] std::size_t size() const {
        std::scoped_lock l{ mtx_ };
        return q_.size();
    }

    [[nodiscard]] std::optional<T> dequeue() {
        std::unique_lock l{ mtx_ };
        cv_.wait(l, [this] { return !q_.empty() || stop_requested_; });

        if(stop_requested_)
            return {};

        auto value = q_.front();
        q_.pop();

        l.unlock();
        cv_.notify_all();
        return std::make_optional<T>(value);
    }

private:
    std::size_t capacity_;
    std::function<void(T &)> deleter_;
    std::queue<T> q_;

    mutable std::mutex mtx_;
    std::condition_variable cv_;
    bool stop_requested_ = false;
};

namespace beast     = boost::beast;         // from <boost/beast.hpp>
namespace http      = beast::http;          // from <boost/beast/http.hpp>
namespace websocket = beast::websocket;     // from <boost/beast/websocket.hpp>
namespace net       = boost::asio;          // from <boost/asio.hpp>
using tcp           = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>

//------------------------------------------------------------------------------

// Report a failure
inline void fail([[maybe_unused]] beast::error_code ec, [[maybe_unused]] std::string_view what) {
    // fmt::print("{}: {}\n", what, ec.message());
    // todo: use reporting for this instead?
}

class WebSocketSession {
    tcp::resolver resolver_;
    websocket::stream<beast::tcp_stream> ws_;

    std::string host_;
    std::string port_;

    std::atomic_bool is_connected_ = false;

public:
    explicit WebSocketSession(net::io_context &ioc, std::string const &host, std::string const &port)
        : resolver_(net::make_strand(ioc))
        , ws_(net::make_strand(ioc))
        , host_{ host }
        , port_{ port } { connect(); }

    /**
     * @brief Blocks until connection is actually established
     */
    void ensure_connection_established() {
        is_connected_.wait(false);
    }

    void write(std::string &&data) {
        ensure_connection_established();
        ws_.async_write(net::buffer(data), beast::bind_front_handler(&WebSocketSession::on_write, this));
    }

    std::string read_one() {
        ensure_connection_established();

        beast::flat_buffer buffer;
        ws_.read(buffer);
        return beast::buffers_to_string(buffer.data());
    }

    void close() {
        ws_.async_close(websocket::close_code::normal, beast::bind_front_handler(&WebSocketSession::on_close, this));
    }

private:
    void connect() {
        is_connected_ = false;
        resolver_.async_resolve(host_, port_, beast::bind_front_handler(&WebSocketSession::on_resolve, this));
    }

    void on_resolve(beast::error_code ec, tcp::resolver::results_type results) {
        if(ec)
            return fail(ec, "resolve");

        beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(30));
        beast::get_lowest_layer(ws_).async_connect(results, beast::bind_front_handler(&WebSocketSession::on_connect, this));
    }

    void on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type ep) {
        if(ec)
            return fail(ec, "connect");

        beast::get_lowest_layer(ws_).expires_never();
        ws_.set_option(
            websocket::stream_base::timeout::suggested(
                beast::role_type::client));

        ws_.set_option(websocket::stream_base::decorator(
            [](websocket::request_type &req) {
                req.set(http::field::user_agent, "cliot");
            }));

        // See https://tools.ietf.org/html/rfc7230#section-5.4
        host_ += ':' + std::to_string(ep.port());

        ws_.async_handshake(host_, "/", beast::bind_front_handler(&WebSocketSession::on_handshake, this));
    }

    void on_handshake(beast::error_code ec) {
        if(ec)
            return fail(ec, "handshake");
        is_connected_ = true;
        is_connected_.notify_all();
    }

    void on_write(beast::error_code ec, [[maybe_unused]] std::size_t bytes_transferred) {
        if(ec)
            return fail(ec, "write");
    }

    void on_close(beast::error_code ec) {
        if(ec)
            return fail(ec, "close");
    }
};

class AsyncConnectionPool {
    boost::asio::io_context ctx_;
    boost::asio::executor_work_guard<decltype(ctx_.get_executor())> work_{ ctx_.get_executor() };

    using ws_ptr_t = std::shared_ptr<WebSocketSession>;
    AsyncQueue<ws_ptr_t> available_pool_;
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
    AsyncConnectionPool(std::string const &host, std::string const &port)
        : available_pool_{ 4, [](std::shared_ptr<WebSocketSession> &ses) { ses->close(); } } {
        for(auto i = 0; i < 4; ++i)
            workers_.emplace_back(std::bind_front(&AsyncConnectionPool::worker_loop, this));
        for(auto i = 0; i < 4; ++i)
            available_pool_.enqueue(std::make_shared<WebSocketSession>(ctx_, host, port));
    }

    ~AsyncConnectionPool() {
        stop();
        for(auto &worker : workers_)
            worker.join();
    }

    // potentially blocks
    shared_link_t borrow() {
        auto ws = available_pool_.dequeue();
        if(!ws)
            throw std::runtime_error("Could not borrow ws connection");

        return std::make_shared<ConnectionLink>(*ws,
            [this, ws = *ws]() {
                available_pool_.enqueue(ws);
            });
    }

private:
    void worker_loop() {
        try {
            ctx_.run();
        } catch(std::exception const &e) {
            fmt::print("Exception on asio worker thread: {}\n", e.what());
        }
    }

    void stop() {
        available_pool_.stop();
        work_.reset();
    }
};

// a very inefficient, sync implementation of get and post http requests
class OnDemandFetcher {
public:
    std::string get(std::string const &url) const {
        return fetch(url, boost::beast::http::verb::get);
    }

    std::string post(std::string const &url) const {
        return fetch(url, boost::beast::http::verb::post);
    }

private:
    // basically taken as is from the sync ssl example
    std::string fetch(std::string const &url, boost::beast::http::verb method) const {
        namespace beast = boost::beast;
        namespace http  = beast::http;
        namespace net   = boost::asio;
        namespace ssl   = net::ssl;
        using tcp       = net::ip::tcp;

        try {
            auto uri          = util::parse_uri(url);
            auto const host   = uri.domain;
            auto const port   = uri.port;
            auto const target = uri.resource;
            auto const query  = uri.query;
            int version       = 10;

            net::io_context ioc;
            ssl::context ctx(ssl::context::tlsv12_client);
            ctx.set_verify_mode(ssl::verify_none);

            tcp::resolver resolver(ioc);
            beast::ssl_stream<beast::tcp_stream> stream(ioc, ctx);

            if(!SSL_set_tlsext_host_name(stream.native_handle(), host.data())) {
                beast::error_code ec{ static_cast<int>(::ERR_get_error()), net::error::get_ssl_category() };
                throw beast::system_error{ ec };
            }

            auto const results = resolver.resolve(host, port);
            beast::get_lowest_layer(stream).connect(results);
            stream.handshake(ssl::stream_base::client);

            http::request<http::string_body> req{ method, target, version };
            req.set(http::field::protocol_query, query);
            req.set(http::field::host, host);
            req.set(http::field::user_agent, "cliot");

            http::write(stream, req);

            beast::flat_buffer buffer;
            http::response<http::dynamic_body> res;
            http::read(stream, buffer, res);

            beast::error_code ec;
            stream.shutdown(ec);
            if(ec == net::error::eof) {
                ec = {};
            }
            if(ec)
                throw beast::system_error{ ec };

            return beast::buffers_to_string(res.body().data());
        } catch(std::exception const &e) {
            return e.what();
        }
    }
};

template <ConnectionHandler Handler, SimpleRequestProvider FetchProvider>
class ConnectionManager {
    Handler handler_;
    std::reference_wrapper<const FetchProvider> fetcher_;

public:
    using link_ptr_t = typename Handler::shared_link_t;

    ConnectionManager(std::string const &host, std::string const &port, FetchProvider const &fetcher)
        : handler_{ host, port }
        , fetcher_{ std::cref(fetcher) } {
    }

    [[nodiscard]] ConnectionChannel auto request(std::string &&data) {
        auto link = handler_.borrow(); // potentially blocks until ws is available to borrow
        link->write(std::move(data));
        return link;
    }

    // blocks, performs http connection GET and returns data or throws
    [[nodiscard]] std::string get(std::string const &url) {
        return fetcher_.get().get(url);
    }

    // blocks, performs http connection POST and returns data or throws
    [[nodiscard]] std::string post(std::string const &url) {
        return fetcher_.get().post(url);
    }
};
