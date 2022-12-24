#pragma once

#include <runner.hpp>

#include <fmt/compile.h>
#include <inja/inja.hpp>

#include <filesystem>
#include <iostream>
#include <vector>

class Crawler {
    using data_t = std::pair<std::vector<Request>, std::vector<Response>>;
    std::reference_wrapper<inja::Environment> env_;
    std::filesystem::path path_;
    std::map<std::string, data_t> flows_; // ordered

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

        return flows_;
    }

private:
    void crawl(std::filesystem::path flow_path) {
        auto flow_name = flow_path.filename().string();
        report_detected(flow_name);

        for(auto const &entry : std::filesystem::directory_iterator{ flow_path }) {
            if(not entry.is_regular_file())
                continue;

            auto candidate = entry.path().extension();
            if(candidate == ".jrq") {
                flows_[flow_name].first.emplace_back(env_, entry.path());
            } else if(candidate == ".jrp") {
                flows_[flow_name].second.emplace_back(env_, entry.path());
            }
        }
    }

    void report_detected(std::string_view name) {
        fmt::print(fg(fmt::color::ghost_white), "+ | ");
        fmt::print(fg(fmt::color::light_green) | fmt::emphasis::bold, "DETECT FLOW ");
        fmt::print(fg(fmt::color::sky_blue) | fmt::emphasis::bold, "{}\n", name);
    }
};
