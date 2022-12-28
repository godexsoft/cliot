#pragma once

#include <di.hpp>
#include <fmt/compile.h>
#include <inja/inja.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
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
#include <sstream>
#include <string_view>
#include <vector>

// Currently OnDemandConnection is sync and creates a io_context for each request..
// Ideally:
// 1) there is a connection pool to clio, either ssl or plain websocket or even normal http
// 2) websocket connections should be able to receive multiple messages and generally should stay alive.
// 3) one websocket connection can serve multiple requests, however subscriptions and their testing should be considered too.

// inefficient, sync version
struct OnDemandConnection {
    std::string host;
    std::string port;

    OnDemandConnection(std::string const &host, std::string const &port)
        : host{ host }
        , port{ port } { }

    //
    // ssl implementation if needed later
    //

    // std::string request(std::string &&data) {
    //     namespace beast     = boost::beast;
    //     namespace websocket = beast::websocket;
    //     namespace http      = beast::http;
    //     namespace net       = boost::asio;
    //     namespace ssl       = net::ssl;
    //     using tcp           = net::ip::tcp;

    //     try {
    //         net::io_context ioc;
    //         ssl::context ctx{ ssl::context::tlsv12_client };
    //         ctx.set_verify_mode(ssl::verify_none);

    //         tcp::resolver resolver{ ioc };
    //         websocket::stream<beast::ssl_stream<tcp::socket>> ws{ ioc, ctx };

    //         auto const results = resolver.resolve(host, port);
    //         auto ep            = net::connect(get_lowest_layer(ws), results);

    //         if(!SSL_set_tlsext_host_name(ws.next_layer().native_handle(), host.c_str()))
    //             throw beast::system_error(
    //                 beast::error_code(
    //                     static_cast<int>(::ERR_get_error()),
    //                     net::error::get_ssl_category()),
    //                 "Failed to set SNI Hostname");

    //         host += ':' + std::to_string(ep.port());
    //         ws.next_layer().handshake(ssl::stream_base::client);
    //         ws.set_option(websocket::stream_base::decorator(
    //             [](websocket::request_type &req) {
    //                 req.set(http::field::user_agent, "cliot");
    //             }));

    //         ws.handshake(host, "/");
    //         ws.write(net::buffer(std::move(data)));

    //         beast::flat_buffer buffer;
    //         ws.read(buffer);

    //         ws.close(websocket::close_code::normal);

    //         return beast::buffers_to_string(buffer.data());
    //     } catch(std::exception const &e) {
    //         return fmt::format(R"({{"error" : "{}"}})", e.what());
    //     }
    // }

    std::string request(std::string &&data) {
        namespace beast     = boost::beast;
        namespace websocket = beast::websocket;
        namespace http      = beast::http;
        namespace net       = boost::asio;
        using tcp           = net::ip::tcp;

        try {
            net::io_context ioc;

            tcp::resolver resolver{ ioc };
            websocket::stream<tcp::socket> ws{ ioc };

            auto const results = resolver.resolve(host, port);
            auto ep            = net::connect(ws.next_layer(), results);

            host += ':' + std::to_string(ep.port());
            ws.set_option(websocket::stream_base::decorator(
                [](websocket::request_type &req) {
                    req.set(http::field::user_agent, "cliot");
                }));

            ws.handshake(host, "/");
            ws.write(net::buffer(std::move(data)));

            beast::flat_buffer buffer;
            ws.read(buffer);
            ws.close(websocket::close_code::normal);

            return beast::buffers_to_string(buffer.data());
        } catch(std::exception const &e) {
            return fmt::format(R"({{"error" : "{}"}})", e.what());
        }
    }
};

// taken from https://github.com/boostorg/beast/issues/787 as a workaround of not having boost.url
struct ParsedURI {
    std::string protocol;
    std::string domain; // only domain must be present
    std::string port;
    std::string resource;
    std::string query; // everything after '?', possibly nothing
};

ParsedURI parse_uri(const std::string &url) {
    ParsedURI result;
    auto value_or = [](const std::string &value, std::string &&deflt) -> std::string {
        return (value.empty() ? deflt : value);
    };
    static const std::regex PARSE_URL{ R"((([httpsw]{2,5})://)?([^/ :]+)(:(\d+))?(/([^ ?]+)?)?/?\??([^/ ]+\=[^/ ]+)?)",
        std::regex_constants::ECMAScript | std::regex_constants::icase };
    std::smatch match;
    if(std::regex_match(url, match, PARSE_URL) && match.size() == 9) {
        result.protocol               = value_or(boost::algorithm::to_lower_copy(std::string(match[2])), "http");
        result.domain                 = match[3];
        const bool is_sequre_protocol = (result.protocol == "https" || result.protocol == "wss");
        result.port                   = value_or(match[5], (is_sequre_protocol) ? "443" : "80");
        result.resource               = value_or(match[6], "/");
        result.query                  = match[8];
    }
    return result;
}

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
            auto uri          = parse_uri(url);
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

// clang-format off
template <typename T>
concept ConnectionHandler = requires(T a, std::string s) {
    { a.request(std::move(s)) } -> std::convertible_to<std::string>;
};

template <typename T>
concept SimpleRequestProvider = requires(T a, std::string s) {
    { a.get(s) } -> std::convertible_to<std::string>;
    { a.post(s) } -> std::convertible_to<std::string>;
};
// clang-format on

template <ConnectionHandler Handler, SimpleRequestProvider FetchProvider>
class ConnectionManager {
    std::string host_;
    std::string port_;

    std::reference_wrapper<const FetchProvider> fetcher_;

public:
    ConnectionManager(std::string const &host, std::string const &port, FetchProvider const &fetcher)
        : host_{ host }
        , port_{ port }
        , fetcher_{ std::cref(fetcher) } { }

    // blocks, performs websocket on a connection
    std::string send(std::string &&data) {
        return Handler{ host_, port_ }.request(std::move(data));
    }

    // blocks, performs http connection GET and returns data or throws
    std::string get(std::string const &url) {
        return fetcher_.get().get(url);
    }

    // blocks, performs http connection POST and returns data or throws
    std::string post(std::string const &url) {
        return fetcher_.get().post(url);
    }
};
