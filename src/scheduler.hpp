#pragma once

#include <crawler.hpp>
#include <reporting.hpp>
#include <runner.hpp>

#include <di.hpp>
#include <fmt/color.h>
#include <fmt/compile.h>
#include <inja/inja.hpp>

#include <filesystem>
#include <iostream>
#include <sstream>
#include <string_view>
#include <vector>

template <
    typename ConnectionManagerType,
    typename RequestType,
    typename ResponseType,
    typename ReportRendererType>
class Scheduler {
    // all services needed for the scheduler:
    using env_t             = inja::Environment;
    using store_t           = inja::json;
    using con_man_t         = ConnectionManagerType;
    using reporting_t       = ReportEngine;
    using report_renderer_t = ReportRendererType;
    using crawler_t         = Crawler<RequestType, ResponseType>;
    using services_t        = di::Deps<env_t, store_t, con_man_t, reporting_t, report_renderer_t, crawler_t>;

    services_t services_;

public:
    Scheduler(services_t services)
        : services_{ services } { }

    void run() {
        auto const &reporting = services_.template get<reporting_t>();
        auto const &renderer  = services_.template get<report_renderer_t>();
        auto flows            = services_.template get<crawler_t>().get().crawl();
        for(auto const &[name, flow] : flows) {
            try {
                auto runner = FlowRunner<ConnectionManagerType, RequestType, ResponseType>{
                    services_, name, flow.first, flow.second
                };
                runner.run();

                report_success(name);
                reporting.get().record(SimpleEvent{ "SUCCESS", name }, renderer);

            } catch(std::exception const &e) {
                // we need custom exception here i reckon.. to hold FailureEvent basically
                report_failure(name, e.what());
                // reporting.get().record(FailureEvent{ name, }, renderer);
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
