#pragma once

#include <di.hpp>
#include <fmt/color.h>
#include <fmt/compile.h>
#include <inja/inja.hpp>
#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <events.hpp>
#include <reporting.hpp>
#include <runner.hpp>
#include <validation.hpp>
#include <web.hpp>

struct FlowException : public std::runtime_error {
    FlowException(
        std::string const &path,
        std::vector<FailureEvent::Data> const &issues,
        std::string const &response)
        : std::runtime_error{ "FlowException" }
        , path{ path }
        , issues{ issues }
        , response{ response } { }

    std::string path;
    std::vector<FailureEvent::Data> issues;
    std::string response;
};

// this stuff is detail
struct Meta {
    std::string subject, description, author, created_on, last_update;
    std::vector<std::string> revisions;
};

struct Step {
    enum class Type {
        INVALID_TYPE,
        REQUEST,
        RESPONSE,
        RUN_FLOW,
        INJECT,
        REPEAT_BLOCK,
    };

    Type type = Type::INVALID_TYPE;
    std::string name;
    YAML::Node data;
    std::string file;
    std::optional<std::vector<Step>> steps;
    std::optional<uint32_t> repeat; // only for block step
};

namespace YAML {
template <>
struct convert<Step::Type> {
    static bool decode(const Node &node, Step::Type &rhs) {
        if(not node.IsScalar())
            return false;
        auto type = node.as<std::string>();
        if(type == "request")
            rhs = Step::Type::REQUEST;
        else if(type == "response")
            rhs = Step::Type::RESPONSE;
        else if(type == "run_flow")
            rhs = Step::Type::RUN_FLOW;
        else if(type == "inject")
            rhs = Step::Type::INJECT;
        else if(type == "block")
            rhs = Step::Type::REPEAT_BLOCK;
        else
            return false;
        return true;
    }
};

template <>
struct convert<Step> {
    static bool decode(const Node &node, Step &rhs) {
        if(not node.IsMap())
            return false;

        rhs.type = node["type"].as<Step::Type>();
        if(node["name"])
            rhs.name = node["name"].as<std::string>();
        if(node["file"])
            rhs.file = node["file"].as<std::string>();
        if(node["data"])
            rhs.data = node["data"];
        if(node["repeat"])
            rhs.repeat = node["repeat"].as<uint32_t>();
        if(node["steps"])
            rhs.steps = node["steps"].as<std::vector<Step>>();

        return true;
    }
};
}

template <typename ConnectionManagerType, typename ReportEngineType, typename ValidatorType, typename FlowFactoryType>
class Flow {
public:
    class RequestStep {
        using env_t       = inja::Environment;
        using store_t     = inja::json;
        using con_man_t   = ConnectionManagerType;
        using reporting_t = ReportEngineType;
        using services_t  = di::Deps<env_t, store_t, con_man_t, reporting_t>;

        services_t services_;
        std::string path_;

    public:
        RequestStep(services_t services, std::filesystem::path const &path)
            : services_{ services }
            , path_{ path.string() } {
        }

        RequestStep(RequestStep &&)      = default;
        RequestStep(RequestStep const &) = default;

        std::string perform() {
            try {
                auto const &[env, con_man, store] = services_.template get<env_t, con_man_t, store_t>();
                auto temp                         = env.get().parse_template(path_);
                auto res                          = env.get().render(temp, store);

                report(RequestEvent{ path_, store, inja::json::parse(res).dump(4) });
                return con_man.get().send(std::move(res));
            } catch(std::exception const &e) {
                auto const issues = std::vector<FailureEvent::Data>{
                    { FailureEvent::Data::Type::LOGIC_ERROR, path_, e.what() }
                };
                throw FlowException(path_, issues, "No data");
            }
        }

    private:
        void
        report(auto &&ev) {
            auto const &reporting = services_.template get<reporting_t>();
            reporting.get().record(std::move(ev));
        }
    };

    class ResponseStep {
        using env_t       = inja::Environment;
        using store_t     = inja::json;
        using con_man_t   = ConnectionManagerType;
        using reporting_t = ReportEngineType;
        using services_t  = di::Deps<env_t, store_t, con_man_t, reporting_t>;

        services_t services_;
        std::string path_;
        ValidatorType validator_;

    public:
        ResponseStep(services_t services, std::filesystem::path const &path)
            : services_{ services }
            , path_{ path.string() } { }

        ResponseStep(ResponseStep &&)      = default;
        ResponseStep(ResponseStep const &) = default;

        void validate(inja::json const &incoming) {
            try {
                auto const &[env, store] = services_.template get<env_t, store_t>();
                store.get()["$res"]      = incoming;

                auto temp   = env.get().parse_template(path_);
                auto result = env.get().render(temp, store);

                try {
                    auto expectations = inja::json::parse(result);

                    report(ResponseEvent{ path_, incoming.dump(4), expectations.dump(4) });
                    auto [valid, issues] = validator_.validate(expectations, incoming);

                    if(not valid)
                        throw FlowException(path_, issues, incoming.dump(4));
                } catch(inja::json::exception const &e) {
                    auto const issues = std::vector<FailureEvent::Data>{
                        { FailureEvent::Data::Type::LOGIC_ERROR, path_, e.what(), result }
                    };

                    throw FlowException(path_, issues, incoming.dump(4));
                }
            } catch(inja::InjaError const &e) {
                auto const issues = std::vector<FailureEvent::Data>{
                    { FailureEvent::Data::Type::LOGIC_ERROR, path_, e.message }
                };

                throw FlowException(path_, issues, incoming.dump(4));
            } catch(FlowException const &e) {
                // just rethrow inner one
                throw e;
            } catch(std::exception const &e) {
                auto const issues = std::vector<FailureEvent::Data>{
                    { FailureEvent::Data::Type::LOGIC_ERROR, path_, e.what() }
                };

                throw FlowException(path_, issues, incoming.dump(4));
            }
        }

    private:
        void
        report(auto &&ev) {
            auto const &reporting = services_.template get<reporting_t>();
            reporting.get().record(std::move(ev));
        }
    };

    class RunFlowStep {
        using env_t          = inja::Environment;
        using store_t        = inja::json;
        using con_man_t      = ConnectionManagerType;
        using reporting_t    = ReportEngineType;
        using flow_factory_t = FlowFactoryType;
        using services_t     = di::Deps<env_t, store_t, con_man_t, reporting_t, flow_factory_t>;

        services_t services_;
        std::string path_;

    public:
        RunFlowStep(services_t services, std::filesystem::path const &path)
            : services_{ services }
            , path_{ path.string() } { }

        RunFlowStep(RunFlowStep &&)      = default;
        RunFlowStep(RunFlowStep const &) = default;

        void run() {
            try {
                auto const &[env, store] = services_.template get<env_t, store_t>();
                auto runner              = FlowRunner<flow_factory_t>{
                    services_, fmt::format("subflow[{}]", path_), path_
                };
                runner.run(env.get(), store.get());
            } catch(FlowException const &e) {
                auto issues = std::vector<FailureEvent::Data>{
                    { FailureEvent::Data::Type::LOGIC_ERROR, path_, "Subflow execution failed" }
                };
                issues.insert(std::begin(issues), std::begin(e.issues), std::end(e.issues));
                throw FlowException(path_, issues, "No data");
            }
        }
    };

    class RepeatBlockStep {
        using env_t          = inja::Environment;
        using store_t        = inja::json;
        using con_man_t      = ConnectionManagerType;
        using reporting_t    = ReportEngineType;
        using flow_factory_t = FlowFactoryType;
        using services_t     = di::Deps<env_t, store_t, con_man_t, reporting_t, flow_factory_t>;

        services_t services_;
        std::filesystem::path path_;
        uint32_t repeat_;
        std::vector<Step> steps_;

    public:
        RepeatBlockStep(services_t services, std::filesystem::path const &path, uint32_t repeat, std::vector<Step> const &steps)
            : services_{ services }
            , path_{ path }
            , repeat_{ repeat }
            , steps_{ steps } { }

        RepeatBlockStep(RepeatBlockStep &&)      = default;
        RepeatBlockStep(RepeatBlockStep const &) = default;

        void run() {
            for(uint32_t i = 0; i < repeat_; ++i) {
                // todo: this should be a custom event
                report(SimpleEvent{ "REPEAT", fmt::format("[{}] Running all steps in block", i + 1) });
                std::string last_response_;

                // not sure i like that we essentially re-implemented both runner and Flow::load here
                for(auto const &step : steps_) {
                    switch(step.type) {
                    case Step::Type::REQUEST:
                        last_response_ = RequestStep{ services_, path_ / step.file }.perform();
                        break;
                    case Step::Type::RESPONSE:
                        ResponseStep{ services_, path_ / step.file }.validate(inja::json::parse(last_response_));
                        break;
                    case Step::Type::RUN_FLOW:
                        RunFlowStep{ services_, path_.parent_path().parent_path() / step.name }.run();
                        break;
                    case Step::Type::REPEAT_BLOCK:
                        RepeatBlockStep{ services_, path_, step.repeat.value_or(1), step.steps.value() }.run();
                        break;
                    default:
                        break;
                    }
                }
            }
        }

    private:
        void
        report(auto &&ev) {
            auto const &reporting = services_.template get<reporting_t>();
            reporting.get().record(std::move(ev));
        }
    };

public:
    using env_t          = inja::Environment;
    using store_t        = inja::json;
    using con_man_t      = ConnectionManagerType;
    using reporting_t    = ReportEngineType;
    using validator_t    = ValidatorType;
    using flow_factory_t = FlowFactoryType;
    using flow_t         = Flow<con_man_t, reporting_t, validator_t, flow_factory_t>;
    using services_t     = di::Deps<env_t, store_t, con_man_t, reporting_t, flow_factory_t>;

    using request_step_t      = RequestStep;
    using response_step_t     = ResponseStep;
    using run_flow_step_t     = RunFlowStep;
    using repeat_block_step_t = RepeatBlockStep;

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
    void load() {
        auto script_path = path_ / "script.yaml";
        assert(std::filesystem::exists(script_path));
        YAML::Node doc = YAML::LoadFile(script_path.string());

        for(auto const &step : doc["steps"].as<std::vector<Step>>()) {
            switch(step.type) {
            case Step::Type::REQUEST:
                steps_.push_back(RequestStep{ services_, path_ / step.file });
                break;
            case Step::Type::RESPONSE:
                steps_.push_back(ResponseStep{ services_, path_ / step.file });
                break;
            case Step::Type::RUN_FLOW:
                steps_.push_back(RunFlowStep{ services_, path_.parent_path().parent_path() / step.name });
                break;
            case Step::Type::REPEAT_BLOCK:
                steps_.push_back(RepeatBlockStep{ services_, path_, step.repeat.value_or(1), step.steps.value() });
                break;
            default:
                break;
            }
        }
    }
};

template <typename ConnectionManagerType, typename ReportEngineType>
class DefaultFlowFactory {
public:
    using env_t       = inja::Environment;
    using store_t     = inja::json;
    using con_man_t   = ConnectionManagerType;
    using reporting_t = ReportEngineType;
    using flow_t      = Flow<con_man_t, reporting_t, Validator, DefaultFlowFactory<con_man_t, reporting_t>>;

    using services_t = di::Deps<con_man_t, reporting_t>;

    DefaultFlowFactory(services_t services)
        : services_{ services } { }

    auto make(std::filesystem::path const &path, env_t &env, store_t &store) {
        // we inject *this as the factory so that SubFlowRunner can benefit.
        return flow_t{
            di::combine(services_,
                di::Deps<env_t, store_t, DefaultFlowFactory<con_man_t, reporting_t>>{ env, store, *this }),
            path
        };
    }

private:
    services_t services_;
};
