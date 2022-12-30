#pragma once

#include <di.hpp>
#include <fmt/color.h>
#include <fmt/compile.h>
#include <inja/inja.hpp>

#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <events.hpp>
#include <flow.hpp>
#include <reporting.hpp>
#include <web.hpp>

template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

template <typename FlowType>
class FlowRunner {
    using flow_t = FlowType;

    // all services needed for the flow runner:
    using env_t       = typename flow_t::env_t;
    using store_t     = typename flow_t::store_t;
    using con_man_t   = typename flow_t::con_man_t;
    using reporting_t = typename flow_t::reporting_t;
    using services_t  = typename flow_t::services_t;

    services_t services_;
    std::string name_;
    flow_t flow_;
    std::string last_response_;

public:
    FlowRunner(
        services_t services,
        std::string_view name,
        flow_t const &flow)
        : services_{ services }
        , name_{ name }
        , flow_{ flow } { }

    void run() {
        auto store  = services_.template get<store_t>();
        store.get() = {}; // clean for each run

        report("RUNNING", name_);

        for(auto steps = flow_.steps(); auto &step : steps) {
            // clang-format off
            std::visit( overloaded {
                [this](typename flow_t::request_step_t& req) mutable {
                    last_response_ = req.perform();
                },
                [this](typename flow_t::response_step_t& resp) {
                    resp.validate(inja::json::parse(last_response_));
                },
                [](typename flow_t::run_flow_step_t& subflow) {
                    subflow.run();
                }},
            step);
            // clang-format on
        }
    }

private:
    void
    report(std::string const &label, std::string const &message) {
        auto const &reporting = services_.template get<reporting_t>();
        reporting.get().record(SimpleEvent{ label, message });
    }

    void
    report(auto &&ev) {
        auto const &reporting = services_.template get<reporting_t>();
        reporting.get().record(std::move(ev));
    }
};
