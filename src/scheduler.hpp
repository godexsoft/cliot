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

template <typename FlowType, typename CrawlerType>
class Scheduler {
    using flow_t = FlowType;

    // all services needed for the scheduler:
    using env_t       = typename flow_t::env_t;
    using store_t     = typename flow_t::store_t;
    using con_man_t   = typename flow_t::con_man_t;
    using reporting_t = typename flow_t::reporting_t;
    using crawler_t   = CrawlerType;
    using services_t  = di::Deps<env_t, store_t, con_man_t, reporting_t, crawler_t>;

    services_t services_;
    using flow_runner_t = FlowRunner<flow_t>;

public:
    Scheduler(services_t services)
        : services_{ services } { }

    void run() {
        auto const &reporting = services_.template get<reporting_t>();
        auto flows            = services_.template get<crawler_t>().get().crawl();
        for(auto const &[name, flow] : flows) {
            try {
                auto runner = flow_runner_t{
                    services_, name, flow
                };
                runner.run();
                reporting.get().record(SuccessEvent{ name });

            } catch(FlowException const &e) {
                reporting.get().record(FailureEvent{ name, e.path, e.issues, e.response });
            }
        }
    }
};
