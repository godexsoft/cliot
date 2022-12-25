#pragma once

#include <runner.hpp>

#include <fmt/compile.h>
#include <inja/inja.hpp>

#include <filesystem>
#include <iostream>
#include <queue>
#include <set>

class Crawler {
    using path_set_data_t = std::pair<std::string, std::filesystem::path>;
    using data_t          = std::pair<std::queue<Request>, std::queue<Response>>;
    std::reference_wrapper<inja::Environment> env_;
    std::filesystem::path path_;

    std::set<path_set_data_t> paths_;     // all paths ordered
    std::map<std::string, data_t> flows_; // key ordered by name; requests/responses queued in order

public:
    Crawler(inja::Environment &env, std::filesystem::path path)
        : env_{ std::ref(env) }
        , path_{ path } { }

    auto crawl() {
        auto path = path_ / "flows";
        if(not std::filesystem::exists(path))
            throw std::runtime_error("given path does not appear to be valid: missing 'flows' sub directory");

        for(auto const &entry : std::filesystem::directory_iterator{ path }) {
            crawl(entry.path());
        }

        for(auto const &[flow_name, path] : paths_) {
            auto candidate = path.extension();
            if(candidate == ".jrq") {
                flows_[flow_name].first.emplace(env_, path.string());
            } else if(candidate == ".jrp") {
                flows_[flow_name].second.emplace(env_, path.string());
            }
        }
        return flows_;
    }

private:
    void crawl(std::filesystem::path flow_path) {
        auto flow_name = flow_path.filename().string();
        report_detected(flow_name);

        for(auto const &entry : std::filesystem::directory_iterator{ flow_path }) {
            if(not entry.is_regular_file())
                continue;

            paths_.emplace(flow_name, entry.path());
        }
    }

    void report_detected(std::string_view name) {
        fmt::print(fg(fmt::color::ghost_white), "+ | ");
        fmt::print(fg(fmt::color::light_green) | fmt::emphasis::bold, "DETECT FLOW ");
        fmt::print(fg(fmt::color::sky_blue) | fmt::emphasis::bold, "{}\n", name);
    }
};
