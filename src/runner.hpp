#pragma once

#include <di.hpp>
#include <fmt/color.h>
#include <fmt/compile.h>
#include <inja/inja.hpp>

#include <filesystem>
#include <iostream>
#include <sstream>
#include <string_view>
#include <vector>

#include <web.hpp>

// good candidate for CTRE?
std::string parse_index(std::filesystem::path path) {
    auto name = path.filename().string();
    auto pos  = name.find_first_of('_');
    if(pos == std::string::npos)
        throw std::runtime_error("could not parse index from flow name");

    return name.substr(0, pos);
}

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

        if(not valid) {
            std::string flat_issues = fmt::format("{}", fmt::join(issues, "\n"));
            auto message            = fmt::format(
                "Failed '{}':\n{}\nLive response:\n---\n{}\n---",
                path_, flat_issues, incoming.dump(4));

            throw std::runtime_error(message);
        }
    }

    std::string index() const {
        return index_;
    }
};

template <typename ConnectionManagerType, typename RequestType, typename ResponseType>
class FlowRunner {
    // all services needed for the flow runner:
    using env_t      = inja::Environment;
    using store_t    = inja::json;
    using con_man_t  = ConnectionManagerType;
    using services_t = di::Deps<env_t, store_t, con_man_t>;

    services_t services_;

    std::string_view name_;
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
        report_running();
        auto const &[store, con_man] = services_.template get<store_t, con_man_t>();

        while(not requests_.empty()) {
            auto &req = requests_.front();

            // expecting at least one response
            if(responses_.empty())
                throw std::runtime_error("No more responses left to check.. unbalanced");

            report_request(req.index());
            auto data = req.template perform<ConnectionManagerType>(store, con_man);

            if(responses_.front().index() != req.index())
                throw std::logic_error("Index of request is not matching index of response");

            while(!responses_.empty()) {
                auto &resp = responses_.front();
                if(resp.index() != req.index())
                    break; // done matching responses

                // TODO: hm, what about multiple messages??
                report_response(resp.index());
                resp.validate(store, inja::json::parse(data));
                responses_.pop();
            }

            requests_.pop();
        }
    }

private:
    void
    report_running() {
        fmt::print(fg(fmt::color::ghost_white), "? | ");
        fmt::print(fg(fmt::color::pale_green) | fmt::emphasis::bold, "RUN FLOW ");
        fmt::print(fg(fmt::color::sky_blue) | fmt::emphasis::bold, "{}\n", name_);
    }

    void
    report_request(std::string_view idx) {
        fmt::print(fg(fmt::color::ghost_white), "? | ");
        fmt::print(fg(fmt::color::pale_green) | fmt::emphasis::bold, "RUN REQUEST ");
        fmt::print(fg(fmt::color::sky_blue) | fmt::emphasis::bold, "request {}\n", idx);
    }

    void
    report_response(std::string_view idx) {
        fmt::print(fg(fmt::color::ghost_white), "? | ");
        fmt::print(fg(fmt::color::pale_green) | fmt::emphasis::bold, "CHECK RESPONSE ");
        fmt::print(fg(fmt::color::sky_blue) | fmt::emphasis::bold, "response {}\n", idx);
    }
};
