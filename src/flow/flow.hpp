#pragma once

#include <flow/descriptors.hpp>
#include <flow/exceptions.hpp>
#include <reporting/events.hpp>
#include <runner.hpp>
#include <util/overloaded.hpp>
#include <validation/validator.hpp>

#include <flow/step/repeat_block.hpp>
#include <flow/step/request.hpp>
#include <flow/step/response.hpp>
#include <flow/step/run_flow.hpp>

#include <di.hpp>
#include <fmt/color.h>
#include <fmt/compile.h>
#include <inja/inja.hpp>

#include <filesystem>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

template <typename ConnectionManagerType, typename ReportEngineType, typename ValidatorType, typename FlowFactoryType>
class Flow {
public:
    using env_t             = inja::Environment;
    using store_t           = inja::json;
    using con_man_t         = ConnectionManagerType;
    using reporting_t       = ReportEngineType;
    using validator_t       = ValidatorType;
    using flow_factory_t    = FlowFactoryType;
    using connection_link_t = typename con_man_t::link_ptr_t;
    using flow_t            = Flow<con_man_t, reporting_t, validator_t, flow_factory_t>;
    using services_t        = di::Deps<env_t, store_t, con_man_t, reporting_t, flow_factory_t>;

    using request_step_t      = step::Request<env_t, store_t, ConnectionManagerType, ReportEngineType>;
    using response_step_t     = step::Response<env_t, store_t, ConnectionManagerType, ReportEngineType, ValidatorType, inja::InjaError, inja::json::exception>;
    using run_flow_step_t     = step::RunFlow<env_t, store_t, ConnectionManagerType, ReportEngineType, FlowFactoryType>;
    using repeat_block_step_t = step::RepeatBlock<env_t, store_t, ConnectionManagerType, ReportEngineType, FlowFactoryType>;

    using step_t = std::variant<request_step_t, response_step_t, run_flow_step_t, repeat_block_step_t>;

private:
    services_t services_;
    std::vector<step_t> steps_;

public:
    template <typename Loader>
    explicit Flow(services_t services, Loader const &loader)
        : services_{ services }
        , steps_{ load_from(loader.load(), loader.base_path()) } { }

    std::vector<step_t> const &steps() const {
        return steps_;
    }

private:
    std::vector<step_t> load_from(std::vector<descriptor::Step> const &descriptors, std::filesystem::path const &base_path) {
        std::vector<step_t> steps;
        for(auto const &step : descriptors) {
            // clang-format off
            std::visit( overloaded {
                [this, &steps, &base_path](descriptor::Request const &req) {
                    steps.push_back(request_step_t{ services_, base_path / req.file });
                },
                [this, &steps, &base_path](descriptor::Response const &resp) {
                    steps.push_back(response_step_t{ services_, base_path / resp.file });
                },
                [this, &steps, &base_path](descriptor::RunFlow const &flow) {
                    steps.push_back(run_flow_step_t{ services_, base_path.parent_path().parent_path() / flow.name });
                },
                [this, &steps, &base_path](descriptor::RepeatBlock const &block) {
                    steps.push_back(repeat_block_step_t{ services_, base_path, block.repeat, block.steps });
                }},
            step);
            // clang-format on
        }
        return steps;
    }
};
