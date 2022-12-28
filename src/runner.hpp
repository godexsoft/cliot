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
#include <reporting.hpp>
#include <web.hpp>

// good candidate for CTRE?
std::string parse_index(std::filesystem::path path) {
    auto name = path.filename().string();
    auto pos  = name.find_first_of('_');
    if(pos == std::string::npos)
        throw std::runtime_error("could not parse index from flow name");

    return name.substr(0, pos);
}

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

class Request {
    std::reference_wrapper<inja::Environment> env_;
    std::string index_;
    std::string path_;

public:
    Request(inja::Environment &env, std::string const &path)
        : env_{ std::ref(env) }
        , index_{ parse_index(path) }
        , path_{ path } {
    }
    Request(Request const &) = default;
    Request(Request &&)      = delete;

    std::string index() const {
        return index_;
    }

    template <typename ConnectionManagerType, typename ReportHelper>
    std::string perform(inja::json const &store, ConnectionManagerType &con_man, ReportHelper report) {
        try {
            auto temp = env_.get().parse_template(path_);
            auto res  = env_.get().render(temp, store);

            report(RequestEvent{ path_, index_, store, inja::json::parse(res).dump(4) });
            return con_man.send(std::move(res));
        } catch(std::exception const &e) {
            auto const issues = std::vector<FailureEvent::Data>{
                { FailureEvent::Data::Type::LOGIC_ERROR, path_, e.what() }
            };
            throw FlowException(path_, issues, "No data");
        }
    }
};

template <typename ValidatorType>
class Response {
    std::reference_wrapper<inja::Environment> env_;
    std::string index_;
    std::string path_;
    ValidatorType validator_;

public:
    Response(inja::Environment &env, std::string const &path)
        : env_{ std::ref(env) }
        , index_{ parse_index(path) }
        , path_{ path } { }

    Response(Response &&)      = delete;
    Response(Response const &) = default;

    template <typename ReportHelper>
    void validate(inja::json const &store, inja::json const &incoming, ReportHelper report) {
        try {
            inja::json data = store;
            data["$res"]    = incoming;

            auto temp   = env_.get().parse_template(path_);
            auto result = env_.get().render(temp, data);
            try {
                auto expectations = inja::json::parse(result);

                report(ResponseEvent{ path_, index_, incoming.dump(4), expectations.dump(4) });
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

    std::string index() const {
        return index_;
    }
};

template <
    typename ConnectionManagerType,
    typename RequestType,
    typename ResponseType,
    typename ReportEngineType>
class FlowRunner {
    // all services needed for the flow runner:
    using env_t       = inja::Environment;
    using store_t     = inja::json;
    using con_man_t   = ConnectionManagerType;
    using reporting_t = ReportEngineType;
    using services_t  = di::Deps<env_t, store_t, con_man_t, reporting_t>;

    services_t services_;

    std::string name_;
    std::queue<RequestType> requests_;
    std::queue<ResponseType> responses_;

public:
    FlowRunner(
        services_t services,
        std::string_view name,
        std::queue<RequestType> const &requests,
        std::queue<ResponseType> const &responses)
        : services_{ services }
        , name_{ name }
        , requests_{ requests }
        , responses_{ responses } { }

    void run() {
        report("RUNNING", name_);
        auto const &[store, con_man] = services_.template get<store_t, con_man_t>();

        while(not requests_.empty()) {
            auto &req = requests_.front();

            // expecting at least one response
            if(responses_.empty())
                throw std::runtime_error("No more responses left to check.. unbalanced");

            report("RUN REQUEST", req.index());
            auto data = req.template perform<ConnectionManagerType>(
                store, con_man, std::bind_front(&FlowRunner::report<RequestEvent>, this));

            if(responses_.front().index() != req.index())
                throw std::logic_error("Index of request is not matching index of response");

            while(!responses_.empty()) {
                auto &resp = responses_.front();
                if(resp.index() != req.index())
                    break; // done matching responses

                // TODO: hm, what about multiple messages??
                report("CHECK RESPONSE", resp.index());
                resp.validate(store, inja::json::parse(data),
                    std::bind_front(&FlowRunner::report<ResponseEvent>, this));

                responses_.pop();
            }

            requests_.pop();
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
