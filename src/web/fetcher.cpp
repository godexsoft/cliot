#include <util/parse_uri.hpp>
#include <web/fetcher.hpp>

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

namespace beast = boost::beast;
namespace http  = beast::http;
namespace net   = boost::asio;
namespace ssl   = net::ssl;
using tcp       = net::ip::tcp;

std::string OnDemandFetcher::get(std::string const &url) const {
    return fetch(url, http::verb::get);
}

std::string OnDemandFetcher::post(std::string const &url) const {
    return fetch(url, http::verb::post);
}

// basically taken as is from the sync ssl example
std::string OnDemandFetcher::fetch(std::string const &url, http::verb method) const {
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
