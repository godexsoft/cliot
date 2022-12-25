#pragma once

#include <crab/crab.hpp>
#include <fmt/color.h>
#include <fmt/compile.h>
#include <inja/inja.hpp>

#include <condition_variable>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <queue>
#include <sstream>
#include <string_view>
#include <vector>

class WebSocketConnection {
    crab::http::ClientConnection ws_;
    const std::string host_;
    uint16_t port_;
    crab::Timer reconnect_timer_;
    crab::Timer send_timer_;
    std::function<void(std::string)> callback_;
    std::queue<std::pair<std::string, std::function<void(std::string)>>> queue_;

public:
    WebSocketConnection(std::string const &host, uint16_t port)
        : ws_{ [this] { on_ws_data(); } }
        , host_{ host }
        , port_{ port }
        , reconnect_timer_{ [this] { connect(); } }
        , send_timer_{ [this] { send_next(); } } {
        connect();
    }

    template <typename CallbackType>
    void send(std::string const &data, CallbackType cb) {
        queue_.emplace(data, cb);
        send_timer_.once(1);
    }

private:
    void on_ws_data() {
        if(!ws_.is_open())
            return on_ws_closed();

        crab::http::WebMessage wm;
        bool had_response = false;
        while(ws_.read_next(wm)) {
            if(wm.is_close()) {
                ws_.write(crab::http::WebMessage{ crab::http::WebMessageOpcode::CLOSE });
            }
            if(wm.is_text()) {
                callback_(wm.body);
                had_response = true;
            }
        }

        if(had_response)
            send_next();
    }

    void on_ws_closed() {
        reconnect_timer_.once(1);
    }

    void connect() {
        crab::http::RequestHeader req;
        req.path = "/";
        ws_.connect(host_, port_);
        ws_.web_socket_upgrade(req);
    }

    void send_next() {
        if(queue_.empty())
            return;

        auto &[data, cb] = queue_.front();
        callback_        = cb;

        ws_.write(crab::http::WebMessage{ crab::http::WebMessageOpcode::TEXT, data });
        queue_.pop();
    }
};

/**
 * @brief A strategy that creates connections on demand. Not very efficient. 
 * 
 * This strategy for @ref ConnectionManager creates a thread with a @ref crab::RunLoop 
 * and executes the request over a @ref WebSocketConnection.
 * Data is awaited for so that the call site does not have to bother with any async code.
 */
struct OnDemandConnection {
    std::string host;
    uint16_t port;

    struct Runner {
        crab::RunLoop run_loop{};
        WebSocketConnection connection;
        std::string data;

        Runner(std::string const &host, uint16_t port, std::string const &data)
            : connection{ host, port }
            , data{ data } { }

        std::string run(std::binary_semaphore &sem) {
            std::string result;

            connection.send(data, [this, &result, &sem](std::string data) {
                result = std::move(data);
                sem.release();
                run_loop.cancel();
            });

            run_loop.run();
            return result;
        }
    };

    std::string request(std::string &&data) {
        std::string result;
        std::binary_semaphore sem{ 0 };
        auto r = std::thread{ [this, &result, &sem, data = std::move(data)] {
            result = Runner{ host, port, std::move(data) }.run(sem);
        } };

        sem.acquire();
        r.join();
        return result;
    }
};

// clang-format off
template <typename T>
concept ConnectionHandler = requires(T a, std::string s) {
    { a.request(std::move(s)) } -> std::convertible_to<std::string>;
};
// clang-format on

template <ConnectionHandler Handler>
class ConnectionManager {
    std::string host_;
    uint16_t port_;

public:
    ConnectionManager(std::string const &host, uint16_t port)
        : host_{ host }
        , port_{ port } { }

    // blocks, performs websocket on a connection
    std::string send(std::string &&data) {
        return Handler{ host_, port_ }.request(std::move(data));
    }
};
