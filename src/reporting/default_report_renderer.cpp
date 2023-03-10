#include <reporting/default_report_renderer.hpp>
#include <reporting/events.hpp>

#include <fmt/color.h>
#include <fmt/compile.h>

#include <algorithm>
#include <string>
#include <vector>

DefaultReportRenderer::DefaultReportRenderer(uint16_t verbose)
    : verbose{ verbose } {
}

void DefaultReportRenderer::operator()(SimpleEvent const &ev) const {
    if(verbose < 1)
        return;
    fmt::print(fg(fmt::color::ghost_white), "? | ");
    fmt::print(fg(fmt::color::pale_green) | fmt::emphasis::bold, "{} ", ev.label);
    fmt::print(fg(fmt::color::sky_blue) | fmt::emphasis::bold, "{}\n", ev.message);
}

void DefaultReportRenderer::operator()(SuccessEvent const &ev) const {
    if(verbose < 1)
        return;
    fmt::print(fg(fmt::color::ghost_white), "+ | ");
    fmt::print(fg(fmt::color::pale_green) | fmt::emphasis::bold, "SUCCESS ");
    fmt::print(fg(fmt::color::sky_blue) | fmt::emphasis::bold, "{}\n", ev.flow_name);
}

void DefaultReportRenderer::operator()(FailureEvent const &ev) const {
    if(verbose < 1)
        return;

    std::vector<std::string> issues;
    std::transform(std::begin(ev.issues), std::end(ev.issues),
        std::back_inserter(issues),
        [this](FailureEvent::Data const &issue) -> std::string {
            return this->operator()(issue);
        });

    std::string flat_issues = fmt::format("{}", fmt::join(issues, "\n"));
    auto title              = fmt::format("Failed '{}':", fmt::format(fg(fmt::color::pale_violet_red) | fmt::emphasis::bold, "{}", ev.path));

    auto all_issues = fmt::format(fg(fmt::color::dark_red) | fmt::emphasis::bold, "{}", flat_issues);
    auto response   = fmt::format(fg(fmt::color::medium_violet_red) | fmt::emphasis::italic, "{}", ev.response);
    auto message    = fmt::format("\n [-] {}\n\nIssues:\n{}\n\nLive response:\n---\n{}\n---", title, all_issues, response);

    fmt::print(fg(fmt::color::ghost_white), "- | ");
    fmt::print(fg(fmt::color::red) | fmt::emphasis::bold, "FAIL ");
    fmt::print("'{}': {}\n",
        fmt::format(fg(fmt::color::sky_blue) | fmt::emphasis::bold, "{}", ev.flow_name),
        message);
}

void DefaultReportRenderer::operator()(RequestEvent const &ev) const {
    if(verbose < 2)
        return;

    fmt::print(fg(fmt::color::ghost_white), "? | ");
    fmt::print(fg(fmt::color::pale_green) | fmt::emphasis::bold, "REQUEST ");
    fmt::print(fg(fmt::color::sky_blue) | fmt::emphasis::bold, "{}\n", ev.path);

    fmt::print("Request data:\n---\n{}\n---\nStore state:\n---\n{}\n---\n",
        fmt::format(fg(fmt::color::sky_blue) | fmt::emphasis::italic, "{}", ev.data),
        fmt::format(fg(fmt::color::blue_violet) | fmt::emphasis::italic, "{}", ev.store.dump(4)));
}

void DefaultReportRenderer::operator()(ResponseEvent const &ev) const {
    if(verbose < 2)
        return;

    fmt::print(fg(fmt::color::ghost_white), "? | ");
    fmt::print(fg(fmt::color::pale_green) | fmt::emphasis::bold, "RESPONSE ");
    fmt::print(fg(fmt::color::sky_blue) | fmt::emphasis::bold, "{}\n", ev.path);

    fmt::print("Response:\n---\n{}\n---\nExpectations:\n---\n{}\n---\n",
        fmt::format(fg(fmt::color::sky_blue) | fmt::emphasis::italic, "{}", ev.response),
        fmt::format(fg(fmt::color::blue_violet) | fmt::emphasis::italic, "{}", ev.expectations));
}

std::string DefaultReportRenderer::operator()(FailureEvent::Data::Type type) const {
    switch(type) {
    case FailureEvent::Data::Type::LOGIC_ERROR:
        return fmt::format(fg(fmt::color::orange_red) | fmt::emphasis::bold, "LOGIC");
    case FailureEvent::Data::Type::NO_MATCH:
        return fmt::format(fg(fmt::color::indian_red) | fmt::emphasis::bold, "NO MATCH");
    case FailureEvent::Data::Type::NOT_EQUAL:
        return fmt::format(fg(fmt::color::indian_red) | fmt::emphasis::bold, "NOT EQUAL");
    case FailureEvent::Data::Type::TYPE_CHECK:
        return fmt::format(fg(fmt::color::indian_red) | fmt::emphasis::bold, "WRONG TYPE");
    }
}

std::string DefaultReportRenderer::operator()(FailureEvent::Data const &failure) const {
    if(verbose < 1)
        return "";
    auto path   = fmt::format(fg(fmt::color::sky_blue) | fmt::emphasis::bold, "{}", failure.path);
    auto detail = [&failure]() -> std::string {
        if(not failure.detail.empty())
            return fmt::format("\n\nExtra detail:\n---\n{}\n---\n", failure.detail);
        return "";
    }();
    return fmt::format("  {} {} [{}]: {}{}",
        fmt::format(fg(fmt::color::red) | fmt::emphasis::bold, "-"),
        this->operator()(failure.type), path, failure.message, detail);
}
