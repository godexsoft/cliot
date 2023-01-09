#pragma once

#include <reporting/events.hpp>

#include <string>
#include <vector>

struct DefaultReportRenderer {
    uint16_t verbose;
    DefaultReportRenderer(uint16_t verbose);

    void operator()(SimpleEvent const &ev) const;
    void operator()(SuccessEvent const &ev) const;
    void operator()(FailureEvent const &ev) const;
    void operator()(RequestEvent const &ev) const;
    void operator()(ResponseEvent const &ev) const;

    std::string operator()(FailureEvent::Data::Type type) const;
    std::string operator()(FailureEvent::Data const &failure) const;
};
