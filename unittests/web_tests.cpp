#include <gtest/gtest.h>

#include <web.hpp>

#include <fmt/color.h>
#include <fmt/compile.h>

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

#include <semaphore>

struct MockHandler {
    std::string host;
    std::string port;

    std::string request(std::string &&) {
        return "{data}";
    }
};

struct MockFetcher {
    std::string get(std::string const &) const {
        return "{data}";
    }
    std::string post(std::string const &) const {
        return "{data}";
    }
};

TEST(Web, ConnectionManagerTest) {
    MockFetcher fetcher;
    ConnectionManager<MockHandler, MockFetcher> man{ "127.0.0.1", "1234", std::cref(fetcher) };
    EXPECT_EQ(man.send(R"({"method":"server_info"})"), "{data}");
    EXPECT_EQ(man.get("http://test.com"), "{data}");
    EXPECT_EQ(man.post("https://another.test.com/something"), "{data}");
}

namespace beast     = boost::beast;         // from <boost/beast.hpp>
namespace http      = beast::http;          // from <boost/beast/http.hpp>
namespace websocket = beast::websocket;     // from <boost/beast/websocket.hpp>
namespace net       = boost::asio;          // from <boost/asio.hpp>
using tcp           = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>

//------------------------------------------------------------------------------

// Report a failure
void fail(beast::error_code ec, char const *what) {
    std::cerr << what << ": " << ec.message() << "\n";
}

class WebSocketSession {
    tcp::resolver resolver_;
    websocket::stream<beast::tcp_stream> ws_;

    std::string host_;
    std::string port_;
    std::string request_data_;

    std::function<void(std::string &&)> on_read_cb_;
    beast::flat_buffer buffer_;

public:
    explicit WebSocketSession(net::io_context &ioc, std::string const &host, std::string const &port)
        : resolver_(net::make_strand(ioc))
        , ws_(net::make_strand(ioc))
        , host_{ host }
        , port_{ port } { connect(); }

    template <typename Fn>
    void send(std::string &&data, Fn callback) {
        if(not ws_.is_open())
            throw std::runtime_error("Not connected");

        request_data_ = std::move(data);
        on_read_cb_   = callback;

        ws_.async_write(net::buffer(request_data_), beast::bind_front_handler(&WebSocketSession::on_write, this));
    }

    void close() {
        ws_.async_close(websocket::close_code::normal, beast::bind_front_handler(&WebSocketSession::on_close, this));
    }

private:
    void connect() {
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

        // we are connected seems like
    }

    void on_write(beast::error_code ec, [[maybe_unused]] std::size_t bytes_transferred) {
        if(ec)
            return fail(ec, "write");

        ws_.async_read(buffer_, beast::bind_front_handler(&WebSocketSession::on_read, this));
    }

    void on_read(beast::error_code ec, [[maybe_unused]] std::size_t bytes_transferred) {
        if(ec)
            return fail(ec, "read");

        on_read_cb_(beast::buffers_to_string(buffer_.data()));
        ws_.async_read(buffer_, beast::bind_front_handler(&WebSocketSession::on_read, this));
    }

    void on_close(beast::error_code ec) {
        if(ec)
            return fail(ec, "close");

        // If we get here then the connection is closed gracefully

        // The make_printable() function helps print a ConstBufferSequence
        // std::cout << beast::make_printable(buffer_.data()) << std::endl;
    }
};

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
            fmt::print("close whatever front in the queue..\n");
            deleter_(q_.front());
            q_.pop();
        }
        cv_.notify_all();
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

class AsyncConnectionPool {
    boost::asio::io_context ctx_;
    boost::asio::executor_work_guard<decltype(ctx_.get_executor())> work_{ ctx_.get_executor() };

    AsyncQueue<std::shared_ptr<WebSocketSession>> available_pool_;
    std::vector<std::thread> workers_;

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

    // note: this is potentially hit from multiple threads
    template <typename Fn>
    void request(std::string &&data, Fn callback) {
        // we should take a free connection, transfer it to in_use pool and then execute on it
        // passing it the callback as completion.

        auto con = available_pool_.dequeue();
        if(not con) {
            fmt::print("- unhandled connection on stop request");
            return;
        }

        fmt::print("got connection to operate on\n");

        // note that con.value() is shared_ptr, we capture it with the lambda to have it survive thru the request
        con.value()->send(std::move(data), [this, callback, con = con.value()](std::string &&resp) mutable {
            fmt::print("finished request, got response: '{}'\n", resp);
            available_pool_.enqueue(con);
            callback(std::move(resp));
        });
    }

private:
    void worker_loop() {
        try {
            fmt::print("running worker\n");
            ctx_.run();
            fmt::print("stopping worker\n");
        } catch(std::exception const &e) {
            fmt::print("oops from thread: {}\n", e.what());
        }
    }

    void stop() {
        fmt::print("stopping pool..\n");
        available_pool_.stop();
        work_.reset();
    }
};

class PooledConnection {
    AsyncConnectionPool pool_;

public:
    PooledConnection(std::string const &host, std::string const &port)
        : pool_{ host, port } { }

    // this makes async pooled connection into a simple sync function
    std::string request(std::string &&data) {
        std::string response;
        std::binary_semaphore sem{ 0 };

        pool_.request(std::move(data), [&response, &sem](std::string &&resp) {
            fmt::print("Got async response: {}\n", resp);
            response = std::move(resp);
            sem.release();
        });
        fmt::print("after request\n");
        sem.acquire();
        fmt::print("after acquire done\n");
        return response;
    }
};

TEST(Web, PoolConnections) {
    try {
        auto con = PooledConnection{ "127.0.0.1", "51233" };
        std::this_thread::sleep_for(std::chrono::seconds{ 1 });
        auto resp = con.request("{}");
        fmt::print("resp: {}\n", resp);
    } catch(std::exception const &e) {
        fmt::print("oops: {}\n", e.what());
    }
}
