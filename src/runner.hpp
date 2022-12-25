#pragma once

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

class Response {
    std::reference_wrapper<inja::Environment> env_;
    std::string index_;
    std::string path_; // full path to response file

    std::string issues_ = {};
    bool is_valid_      = true;

public:
    Response(inja::Environment &env, std::string const &path)
        : env_{ std::ref(env) }
        , index_{ parse_index(path) }
        , path_{ path } {
    }
    Response(Response &&)      = delete;
    Response(Response const &) = default;

    void validate(inja::json const &store, inja::json const &incoming) {
        inja::json data = store;
        data["$res"]    = incoming;

        auto temp         = env_.get().parse_template(path_);
        auto result       = env_.get().render(temp, data);
        auto expectations = inja::json::parse(result);

        dovalidate("", expectations, incoming);
        if(not is_valid_)
            throw std::runtime_error(fmt::format(
                "Failed '{}': {}\n\n---\n{}\n---",
                path_, issues_, incoming.dump(4)));
    }

    std::string index() const {
        return index_;
    }

private:
    void dovalidate(std::string path, inja::json const &expectations, inja::json const &incoming) {
        if(expectations.is_null() or expectations.empty())
            return;

        if(expectations.is_object()) {
            for(auto const &expectation : expectations.items()) {
                auto const &key = expectation.key();
                if(not incoming.contains(key)) {
                    auto const full_path = [&path, &key]() -> std::string {
                        if(path.empty())
                            return key;
                        return path + '.' + key;
                    }();
                    add_issue("NO MATCH", full_path, "Key is not present in the response");

                } else {
                    dovalidate(path + (path.empty() ? "" : ".") + key,
                        expectation.value(),
                        incoming[key]);
                }
            }
        } else {
            if(expectations != incoming) {
                std::stringstream ss;
                ss << expectations << " != " << incoming;
                add_issue("NOT EQUAL", path, ss.str());
            }
        }
    }

    void add_issue(std::string_view subject, std::string_view path, std::string_view detail) {
        is_valid_ = false;
        issues_ += fmt::format("\n    *** {} [{}]: {}", subject, path, detail);
    }
};

template <typename ConnectionManagerType>
class FlowRunner {
    std::reference_wrapper<inja::Environment> env_;
    std::reference_wrapper<inja::json> store_;
    std::reference_wrapper<ConnectionManagerType> con_man_;
    std::string_view name_;
    std::queue<Request> requests_;
    std::queue<Response> responses_;

public:
    FlowRunner(
        inja::Environment &env,
        inja::json &store,
        ConnectionManagerType &con_man,
        std::string_view name,
        std::queue<Request> const &requests,
        std::queue<Response> const &responses)
        : env_{ std::ref(env) }
        , store_{ std::ref(store) }
        , con_man_{ std::ref(con_man) }
        , name_{ name }
        , requests_{ requests }
        , responses_{ responses } {
    }

    void run() {
        report_running();

        while(not requests_.empty()) {
            auto &req = requests_.front();

            // expecting at least one response
            if(responses_.empty())
                throw std::runtime_error("No more responses left to check.. unbalanced");

            report_request(req.index());
            auto data = req.perform<ConnectionManagerType>(store_, con_man_);
            if(responses_.front().index() != req.index())
                throw std::logic_error("Index of request is not matching index of response");

            while(!responses_.empty()) {
                auto &resp = responses_.front();
                if(resp.index() != req.index())
                    break; // done matching responses

                // TODO: hm, what about multiple messages??
                report_response(resp.index());
                resp.validate(store_, inja::json::parse(data));
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
