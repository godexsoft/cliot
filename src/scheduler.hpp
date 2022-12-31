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

template <typename FlowFactoryType, typename ConnectionManagerType, typename ReportEngineType, typename CrawlerType>
class Scheduler {
    using flow_factory_t = FlowFactoryType;
    using con_man_t      = ConnectionManagerType;
    using reporting_t    = ReportEngineType;
    using crawler_t      = CrawlerType;
    using services_t     = di::Deps<flow_factory_t, reporting_t, con_man_t, crawler_t>;

    services_t services_;
    using flow_runner_t = FlowRunner<flow_factory_t>;

public:
    Scheduler(services_t services)
        : services_{ services } { }

    int run() {
        auto const &reporting = services_.template get<reporting_t>();
        auto flow_dirs        = services_.template get<crawler_t>().get().crawl();
        for(auto const &[name, dir] : flow_dirs) {
            try {
                // the scheduler could run a thread pool and execute multiple runners concurrently.
                // this is not really needed yet so leaving it as single threaded for now.
                auto runner = flow_runner_t{
                    services_, name, dir
                };
                runner.run();
                reporting.get().record(SuccessEvent{ name });

            } catch(FlowException const &e) {
                reporting.get().record(FailureEvent{ name, e.path, e.issues, e.response });
            }
        }

        return EXIT_SUCCESS;
    }
};
