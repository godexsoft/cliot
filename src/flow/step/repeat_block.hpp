#pragma once

#include <flow/exceptions.hpp>
#include <flow/yaml_conversion.hpp>
#include <reporting/events.hpp> // probably should not be here, should use report engine public member func instead

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
    std::vector<Step> steps_;

public:
    RepeatBlock(services_t services, std::filesystem::path const &path, uint32_t repeat, std::vector<Step> const &steps)
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
                switch(step.type) {
                case Step::Type::REQUEST:
                    connection_link = RequestStepType{ services_, path_ / step.file }.perform();
                    break;
                case Step::Type::RESPONSE:
                    if(not connection_link)
                        throw std::logic_error{ "Response can't come before Request step" };
                    ResponseStepType{ services_, path_ / step.file }.validate(
                        inja::json::parse(connection_link->read_one())); // inja::json parser should be injected thru services instead
                    break;
                case Step::Type::RUN_FLOW:
                    RunFlowStepType{ services_, path_.parent_path().parent_path() / step.name }.run();
                    break;
                case Step::Type::REPEAT_BLOCK:
                    RepeatBlock{ services_, path_, step.repeat.value_or(1), step.steps.value() }.run();
                    break;
                default:
                    throw std::runtime_error{ "Unsupported step type inside of RepeatBlock" };
                }
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
