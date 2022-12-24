#pragma once

#include <crawler.hpp>
#include <runner.hpp>

#include <fmt/color.h>
#include <fmt/compile.h>
#include <inja/inja.hpp>

#include <filesystem>
#include <iostream>
#include <sstream>
#include <string_view>
#include <vector>

class Scheduler {
    std::reference_wrapper<inja::Environment> env_;
    Crawler crawler_;

public:
    Scheduler(inja::Environment &env, Crawler &&crawler)
        : env_{ std::ref(env) }
        , crawler_{ std::move(crawler) } {
    }

    void run() {
        auto flows = crawler_.crawl();
        for(auto const &[name, flow] : flows) {
            try {
                auto runner = FlowRunner{ env_.get(), name, flow.first, flow.second };
                runner.run();

                report_success(name);
            } catch(std::exception const &e) {
                report_failure(name, e.what());
            }
        }
    }

private:
    void report_success(std::string_view name) {
        fmt::print(fg(fmt::color::ghost_white), "+ | ");
        fmt::print(fg(fmt::color::green) | fmt::emphasis::bold, "SUCCESS ");
        fmt::print(fg(fmt::color::sky_blue) | fmt::emphasis::bold, "{}\n", name);
    }

    void report_failure(std::string_view name, std::string_view msg) {
        fmt::print(fg(fmt::color::ghost_white), "- | ");
        fmt::print(fg(fmt::color::red) | fmt::emphasis::bold, "FAIL ");
        fmt::print("'{}': {}\n",
            fmt::format(fg(fmt::color::sky_blue) | fmt::emphasis::bold, "{}", name),
            fmt::format(fg(fmt::color::red) | fmt::emphasis::italic, "{}", msg));
    }
};
