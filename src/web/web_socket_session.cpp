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

#include <exception>
#include <memory>
#include <string>
#include <vector>

namespace beast     = boost::beast;         // from <boost/beast.hpp>
namespace http      = beast::http;          // from <boost/beast/http.hpp>
namespace websocket = beast::websocket;     // from <boost/beast/websocket.hpp>
namespace net       = boost::asio;          // from <boost/asio.hpp>
using tcp           = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>

// Report a failure
void fail([[maybe_unused]] beast::error_code ec, [[maybe_unused]] std::string_view what) {
    // fmt::print("{}: {}\n", what, ec.message());
    // todo: use reporting for this instead?
}

WebSocketSession::WebSocketSession(net::io_context &ioc, std::string const &host, std::string const &port)
    : resolver_(net::make_strand(ioc))
    , ws_(net::make_strand(ioc))
    , host_{ host }
    , port_{ port } {
    connect();
}

void WebSocketSession::ensure_connection_established() {
    is_connected_.wait(false);
}

void WebSocketSession::write(std::string &&data) {
    ensure_connection_established();
    ws_.async_write(net::buffer(data), beast::bind_front_handler(&WebSocketSession::on_write, this));
}

std::string WebSocketSession::read_one() {
    ensure_connection_established();

    beast::flat_buffer buffer;
    ws_.read(buffer);
    return beast::buffers_to_string(buffer.data());
}

void WebSocketSession::close() {
    ws_.async_close(websocket::close_code::normal, beast::bind_front_handler(&WebSocketSession::on_close, this));
}

void WebSocketSession::connect() {
    is_connected_ = false;
    resolver_.async_resolve(host_, port_, beast::bind_front_handler(&WebSocketSession::on_resolve, this));
}

void WebSocketSession::on_resolve(beast::error_code ec, tcp::resolver::results_type results) {
    if(ec)
        return fail(ec, "resolve");

    beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(30));
    beast::get_lowest_layer(ws_).async_connect(results, beast::bind_front_handler(&WebSocketSession::on_connect, this));
}

void WebSocketSession::on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type ep) {
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

void WebSocketSession::on_handshake(beast::error_code ec) {
    if(ec)
        return fail(ec, "handshake");
    is_connected_ = true;
    is_connected_.notify_all();
}

void WebSocketSession::on_write(beast::error_code ec, [[maybe_unused]] std::size_t bytes_transferred) {
    if(ec)
        return fail(ec, "write");
}

void WebSocketSession::on_close(beast::error_code ec) {
    if(ec)
        return fail(ec, "close");
}
