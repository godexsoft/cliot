#pragma once

#include <web/concepts.hpp>

#include <type_traits>

/**
 * @brief A simple connection manager
 * 
 * Connections are handled by the injected strategies.
 * 
 * @tparam Handler 
 * @tparam FetchProvider 
 */
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
