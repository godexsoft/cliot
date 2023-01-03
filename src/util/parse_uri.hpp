#pragma once

#include <string>

namespace util {

// taken from https://github.com/boostorg/beast/issues/787 as a workaround of not having boost.url
struct ParsedURI {
    std::string protocol;
    std::string domain; // only domain must be present
    std::string port;
    std::string resource;
    std::string query; // everything after '?', possibly nothing
};

ParsedURI parse_uri(const std::string &url);

}; // namespace util
