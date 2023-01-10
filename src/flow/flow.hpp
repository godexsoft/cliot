#pragma once

#include <flow/descriptors.hpp>
#include <flow/exceptions.hpp>
#include <flow/yaml_conversion.hpp>
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
    using repeat_block_step_t = step::RepeatBlock<services_t, connection_link_t, reporting_t, request_step_t, response_step_t, run_flow_step_t>;

    using step_t = std::variant<request_step_t, response_step_t, run_flow_step_t, repeat_block_step_t>;

private:
    services_t services_;

    std::filesystem::path path_;
    std::vector<step_t> steps_;

public:
    explicit Flow(services_t services, std::filesystem::path const &path)
        : services_{ services }
        , path_{ path } {
        assert(std::filesystem::is_directory(path));
        load();
    }

    std::vector<step_t> const &steps() const {
        return steps_;
    }

private:
    // todo: the actual implementation (yaml) should be a strategy injected via services_t
    void load() {
        auto script_path = path_ / "script.yaml";
        assert(std::filesystem::exists(script_path));
        YAML::Node doc = YAML::LoadFile(script_path.string());

        for(auto const &step : doc["steps"].as<std::vector<descriptor::Step>>()) {
            // clang-format off
            std::visit( overloaded {
                [this](descriptor::Request const &req) {
                    steps_.push_back(request_step_t{ services_, path_ / req.file });
                },
                [this](descriptor::Response const &resp) {
                    steps_.push_back(response_step_t{ services_, path_ / resp.file });
                },
                [this](descriptor::RunFlow const &flow) {
                    steps_.push_back(run_flow_step_t{ services_, path_.parent_path().parent_path() / flow.name });
                },
                [this](descriptor::RepeatBlock const &block) {
                    steps_.push_back(repeat_block_step_t{ services_, path_, block.repeat, block.steps });
                }},
            step);
            // clang-format on
        }
    }
};
