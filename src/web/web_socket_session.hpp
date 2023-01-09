#pragma once

#include <util/async_queue.hpp>
#include <web/web_socket_session.hpp>

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

#include <atomic>
#include <exception>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

void fail([[maybe_unused]] boost::beast::error_code ec, [[maybe_unused]] std::string_view what);

class WebSocketSession {
    boost::asio::ip::tcp::resolver resolver_;
    boost::beast::websocket::stream<boost::beast::tcp_stream> ws_;

    std::string host_;
    std::string port_;

    std::atomic_bool is_connected_ = false;

public:
    explicit WebSocketSession(boost::asio::io_context &ioc, std::string const &host, std::string const &port);

    /**
     * @brief Blocks until connection is actually established
     */
    void ensure_connection_established();

    /**
     * @brief 
     * 
     * @param data 
     */
    void write(std::string &&data);

    /**
     * @brief 
     * 
     * @return std::string 
     */
    std::string read_one();

    /**
     * @brief 
     * 
     */
    void close();

private:
    void connect();
    void on_resolve(boost::beast::error_code ec, boost::asio::ip::tcp::resolver::results_type results);
    void on_connect(boost::beast::error_code ec, boost::asio::ip::tcp::resolver::results_type::endpoint_type ep);
    void on_handshake(boost::beast::error_code ec);
    void on_write(boost::beast::error_code ec, [[maybe_unused]] std::size_t bytes_transferred);
    void on_close(boost::beast::error_code ec);
};
