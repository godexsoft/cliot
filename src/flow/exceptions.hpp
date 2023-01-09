#pragma once

#include <reporting/events.hpp>

#include <exception>
#include <string>
#include <vector>

struct FlowException : public std::runtime_error {
    FlowException(
        std::string const &path,
        std::vector<FailureEvent::Data> const &issues,
        std::string const &response)
        : std::runtime_error{ "FlowException" }
        , path{ path }
        , issues{ issues }
        , response{ response } { }

    std::string path;
    std::vector<FailureEvent::Data> issues;
    std::string response;
};
