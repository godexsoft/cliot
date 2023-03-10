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

#include <flow/flow.hpp>
#include <reporting/events.hpp>
#include <reporting/report_engine.hpp>
#include <util/overloaded.hpp>

template <typename FlowFactoryType>
class FlowRunner {
    using flow_factory_t = FlowFactoryType;
    using flow_t         = typename flow_factory_t::flow_t;
    using env_t          = typename flow_factory_t::env_t;
    using store_t        = typename flow_factory_t::store_t;
    using con_man_t      = typename flow_factory_t::con_man_t;
    using reporting_t    = typename flow_factory_t::reporting_t;
    using link_ptr_t     = typename con_man_t::link_ptr_t;
    using services_t     = di::Deps<flow_factory_t, reporting_t, con_man_t>;

    services_t services_;
    std::string name_;
    std::string path_;

public:
    FlowRunner(
        services_t services,
        std::string_view name,
        std::string_view path)
        : services_{ services }
        , name_{ name }
        , path_{ path } { }

    void run() {
        env_t env;
        store_t store;

        auto env_json_path = (std::filesystem::path{ path_ } / "env.json").string();
        if(std::filesystem::exists(env_json_path))
            store = env.load_json(env_json_path);

        register_extensions(env, store);
        run(env, store);
    }

    void run(env_t &env, store_t &store) {
        auto const &factory  = services_.template get<flow_factory_t>();
        auto flow            = factory.get().make(path_, env, store);
        auto connection_link = link_ptr_t{};
        run(env, store, flow);
    }

    void run(env_t &env, store_t &store, auto const &flow) {
        report("RUNNING", name_);
        auto connection_link = link_ptr_t{};

        for(auto steps = flow.steps(); auto &step : steps) {
            // clang-format off
            std::visit( overloaded {
                [&connection_link](typename flow_t::request_step_t& req) mutable {
                    connection_link = req.perform(); // round robin ws on each new request
                },
                [&connection_link](typename flow_t::response_step_t& resp) {
                    if(not connection_link)
                        throw std::logic_error{ "Response can't come before Request step" };
                    resp.validate(inja::json::parse(connection_link->read_one()));
                },
                [](typename flow_t::run_flow_step_t& subflow) {
                    subflow.run();
                },
                [](typename flow_t::repeat_block_step_t& block) {
                    block.run();
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

    void report(auto &&ev) {
        auto const &reporting = services_.template get<reporting_t>();
        reporting.get().record(std::move(ev));
    }

    void register_extensions(env_t &env, store_t &store) {
        auto store_and_return_cb = [&store](inja::Arguments &args) {
            auto value = args.at(0)->get<inja::json>();
            auto var   = args.at(1)->get<std::string>();

            store[var] = value;
            return value;
        };
        env.add_callback("storeAndReturn", 2, store_and_return_cb);

        auto store_cb = [&store](inja::Arguments &args) {
            auto value = args.at(0)->get<inja::json>();
            auto var   = args.at(1)->get<std::string>();
            store[var] = value;
        };
        env.add_void_callback("store", 2, store_cb);

        auto combine_cb = [](inja::Arguments &args) {
            auto value1 = args.at(0)->get<inja::json>();
            auto value2 = args.at(1)->get<inja::json>();
            value1.insert(value1.end(), value2.begin(), value2.end());
            return value1;
        };
        env.add_callback("combine", 2, combine_cb);

        auto equal_cb = [](inja::Arguments &args) {
            auto value1 = args.at(0)->get<inja::json>();
            auto value2 = args.at(1)->get<inja::json>();
            return value1 == value2;
        };
        env.add_callback("equal", 2, equal_cb);

        auto load_cb = [&store](inja::Arguments &args) {
            auto var = args.at(0)->get<std::string>();
            return store[var];
        };
        env.add_callback("load", 1, load_cb);

        auto assert_cb = [](inja::Arguments &args) {
            auto const value = args.at(0)->get<bool>();
            auto const msg   = args.at(1)->get<std::string>();
            if(!value)
                throw std::logic_error(msg);
        };
        env.add_void_callback("assert", 2, assert_cb);

        auto report_cb = [this](inja::Arguments &args) {
            auto const &reporting = services_.template get<reporting_t>();
            auto value            = args.at(0)->get<std::string>();
            reporting.get().record(SimpleEvent{ "CUSTOM", value });
        };
        env.add_void_callback("report", 1, report_cb);

        auto http_fetch_cb = [this, &store](inja::Arguments &args) {
            auto const &[reporting, con_man] = services_.template get<reporting_t, con_man_t>();
            auto url                         = args.at(0)->get<std::string>();
            auto var                         = args.at(1)->get<std::string>();

            reporting.get().record(SimpleEvent{ "FETCH", url + " into " + var });
            auto value = con_man.get().get(url);
            if(not value.empty()) {
                store[var] = value;
            }
        };
        env.add_void_callback("fetch", 2, http_fetch_cb);

        auto http_fetch_json_cb = [this, &store](inja::Arguments &args) {
            auto const &[reporting, con_man] = services_.template get<reporting_t, con_man_t>();
            auto url                         = args.at(0)->get<std::string>();
            auto var                         = args.at(1)->get<std::string>();

            reporting.get().record(SimpleEvent{ "FETCH JSON", url + " into " + var });
            auto value = con_man.get().post(url);
            if(not value.empty()) {
                store[var] = inja::json::parse(value);
            }
        };
        env.add_void_callback("fetch_json", 2, http_fetch_json_cb);
    }
};
