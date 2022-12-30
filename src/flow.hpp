#pragma once

#include <di.hpp>
#include <fmt/color.h>
#include <fmt/compile.h>
#include <inja/inja.hpp>
#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <events.hpp>
#include <reporting.hpp>
#include <runner.hpp>
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
        INJECT
    };

    Type type = Type::INVALID_TYPE;
    std::string name;
    YAML::Node data;
    std::string file;
};

namespace YAML {
template <>
struct convert<Step::Type> {
    static Node encode(const Step::Type &rhs) {
        switch(rhs) {
        case Step::Type::REQUEST:
            return Node{ "request" };
        case Step::Type::RESPONSE:
            return Node{ "response" };
        case Step::Type::RUN_FLOW:
            return Node{ "run_flow" };
        case Step::Type::INJECT:
            return Node{ "inject" };
        case Step::Type::INVALID_TYPE:
            throw std::runtime_error("Invalid step type detected");
        }
    }

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
        else
            return false;
        return true;
    }
};

template <>
struct convert<Step> {
    static Node encode(const Step &rhs) {
        Node node;
        node["type"] = rhs.type;
        node["name"] = rhs.name;
        node["file"] = rhs.file;
        node["data"] = rhs.data;
        return node;
    }

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

        return true;
    }
};
}

template <typename ConnectionManagerType, typename ReportEngineType, typename ValidatorType>
class Flow {
public:
    class RequestStep {
        using env_t       = inja::Environment;
        using store_t     = inja::json;
        using con_man_t   = ConnectionManagerType;
        using reporting_t = ReportEngineType;
        using validator_t = ValidatorType;
        using services_t  = di::Deps<env_t, store_t, con_man_t, reporting_t>;
        using flow_t      = Flow<con_man_t, reporting_t, validator_t>;

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

                // report(RequestEvent{ path_, "index", store, inja::json::parse(res).dump(4) });
                return con_man.get().send(std::move(res));
            } catch(std::exception const &e) {
                auto const issues = std::vector<FailureEvent::Data>{
                    { FailureEvent::Data::Type::LOGIC_ERROR, path_, e.what() }
                };
                throw FlowException(path_, issues, "No data");
            }
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
                auto [env, store]   = services_.template get<env_t, store_t>();
                store.get()["$res"] = incoming;

                auto temp   = env.get().parse_template(path_);
                auto result = env.get().render(temp, store);
                try {
                    auto expectations = inja::json::parse(result);

                    // report(ResponseEvent{ path_, "index", incoming.dump(4), expectations.dump(4) });
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
            } catch(std::exception const &e) {
                auto const issues = std::vector<FailureEvent::Data>{
                    { FailureEvent::Data::Type::LOGIC_ERROR, path_, e.what() }
                };
                throw FlowException(path_, issues, incoming.dump(4));
            }
        }
    };

    class RunFlowStep {
        // all requirements are for FlowRunner
        using env_t       = inja::Environment;
        using store_t     = inja::json;
        using con_man_t   = ConnectionManagerType;
        using reporting_t = ReportEngineType;
        using services_t  = di::Deps<env_t, store_t, con_man_t, reporting_t>;

        using flow_t = Flow<con_man_t, reporting_t, ValidatorType>;

        services_t services_;
        std::string path_;

    public:
        RunFlowStep(services_t services, std::filesystem::path const &path)
            : services_{ services }
            , path_{ path.string() } {
        }

        RunFlowStep(RunFlowStep &&)      = default;
        RunFlowStep(RunFlowStep const &) = default;

        void run() {
            try {
                auto runner = FlowRunner<flow_t>{
                    services_, fmt::format("subflow[{}]", path_), flow_t{ services_, path_ }
                };
                runner.run();
            } catch(FlowException const &e) {
                auto issues = std::vector<FailureEvent::Data>{
                    { FailureEvent::Data::Type::LOGIC_ERROR, path_, "Subflow execution failed" }
                };
                issues.insert(std::begin(issues), std::begin(e.issues), std::end(e.issues));
                throw FlowException(path_, issues, "No data");
            }
        }
    };

public:
    using env_t       = inja::Environment;
    using store_t     = inja::json;
    using con_man_t   = ConnectionManagerType;
    using reporting_t = ReportEngineType;
    using services_t  = di::Deps<env_t, store_t, con_man_t, reporting_t>;

    using request_step_t  = RequestStep;
    using response_step_t = ResponseStep;
    using run_flow_step_t = RunFlowStep;

    using step_t = std::variant<request_step_t, response_step_t, run_flow_step_t>;

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
        YAML::Node doc   = YAML::LoadFile(script_path.string());

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
            default:
                break;
            }
        }
    }
};