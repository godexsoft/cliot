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
        std::vector<std::string> const &issues,
        std::string const &response)
        : std::runtime_error{ "FlowException" }
        , path{ path }
        , issues{ issues }
        , response{ response } { }

    std::string path;
    std::vector<std::string> issues;
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

    template <typename ConnectionManagerType>
    std::string perform(inja::json const &store, ConnectionManagerType &con_man) {
        auto temp = env_.get().parse_template(path_);
        auto res  = env_.get().render(temp, store);
        return con_man.send(std::move(res));
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

    void validate(inja::json const &store, inja::json const &incoming) {
        inja::json data = store;
        data["$res"]    = incoming;

        auto temp         = env_.get().parse_template(path_);
        auto result       = env_.get().render(temp, data);
        auto expectations = inja::json::parse(result);

        auto [valid, issues] = validator_.validate(expectations, incoming);

        if(not valid)
            throw FlowException(path_, issues, incoming.dump(4));
    }

    std::string index() const {
        return index_;
    }
};

template <
    typename ConnectionManagerType,
    typename RequestType,
    typename ResponseType,
    typename ReportRendererType>
class FlowRunner {
    // all services needed for the flow runner:
    using env_t             = inja::Environment;
    using store_t           = inja::json;
    using con_man_t         = ConnectionManagerType;
    using reporting_t       = ReportEngine;
    using report_renderer_t = ReportRendererType;
    using services_t        = di::Deps<env_t, store_t, con_man_t, reporting_t, report_renderer_t>;

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
            auto data = req.template perform<ConnectionManagerType>(store, con_man);

            if(responses_.front().index() != req.index())
                throw std::logic_error("Index of request is not matching index of response");

            while(!responses_.empty()) {
                auto &resp = responses_.front();
                if(resp.index() != req.index())
                    break; // done matching responses

                // TODO: hm, what about multiple messages??
                report("CHECK RESPONSE", resp.index());
                resp.validate(store, inja::json::parse(data));
                responses_.pop();
            }

            requests_.pop();
        }
    }

private:
    void
    report(std::string const &label, std::string const &message) {
        auto const &[reporting, renderer] = services_.template get<reporting_t, report_renderer_t>();
        reporting.get().record(SimpleEvent{ label, message }, renderer);
    }
};
