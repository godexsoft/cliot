#pragma once

#include <flow/exceptions.hpp>
#include <flow/yaml_conversion.hpp>
#include <reporting/events.hpp> // probably should not be here, should use report engine public member func instead
#include <util/overloaded.hpp>

#include <di.hpp>
#include <fmt/compile.h>

#include <exception>
#include <filesystem>
#include <string>
#include <vector>

namespace step {

template <typename ServicesType, typename ConnectionLinkType, typename ReportEngineType, typename RequestStepType, typename ResponseStepType, typename RunFlowStepType>
class RepeatBlock {
    using services_t  = ServicesType;
    using link_ptr_t  = ConnectionLinkType;
    using reporting_t = ReportEngineType;

    services_t services_;
    std::filesystem::path path_;
    uint32_t repeat_;
    std::vector<descriptor::Step> steps_;

public:
    RepeatBlock(services_t services, std::filesystem::path const &path, uint32_t repeat, std::vector<descriptor::Step> const &steps)
        : services_{ services }
        , path_{ path }
        , repeat_{ repeat }
        , steps_{ steps } { }

    RepeatBlock(RepeatBlock &&)      = default;
    RepeatBlock(RepeatBlock const &) = default;

    void run() {
        for(uint32_t i = 0; i < repeat_; ++i) {
            // todo: this should be a custom event
            report(SimpleEvent{ "REPEAT", fmt::format("[{}] Running all steps in block", i + 1) });
            link_ptr_t connection_link;

            // not sure i like that we essentially re-implemented both runner and Flow::load here
            for(auto const &step : steps_) {
                // clang-format off
                std::visit( overloaded {
                    [this, &connection_link](descriptor::Request const &req) {
                        connection_link = RequestStepType{ services_, path_ / req.file }.perform();
                    },
                    [this, &connection_link](descriptor::Response const &resp) {
                        if(not connection_link)
                            throw std::logic_error{ "Response can't come before Request step" };
                        ResponseStepType{ services_, path_ / resp.file }.validate(
                            inja::json::parse(connection_link->read_one())); // inja::json parser should be injected thru services instead
                    },
                    [this](descriptor::RunFlow const &flow) {
                        RunFlowStepType{ services_, path_.parent_path().parent_path() / flow.name }.run();
                    },
                    [this](descriptor::RepeatBlock const &block) {
                        RepeatBlock{ services_, path_, block.repeat, block.steps }.run();
                    }},
                step);
                // clang-format on
            }
        }
    }

private:
    void report(auto &&ev) {
        auto const &reporting = services_.template get<reporting_t>();
        reporting.get().record(std::move(ev));
    }
};

} // namespace step
