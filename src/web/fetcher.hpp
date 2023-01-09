#pragma once

#include <boost/beast/http.hpp>

#include <string>

/**
 * @brief A synchronous implementation of GET and POST HTTP requests 
 */
class OnDemandFetcher {
public:
    /**
     * @brief 
     * 
     * @param url 
     * @return std::string 
     */
    std::string get(std::string const &url) const;

    /**
     * @brief 
     * 
     * @param url 
     * @return std::string 
     */
    std::string post(std::string const &url) const;

private:
    std::string fetch(std::string const &url, boost::beast::http::verb method) const;
};
