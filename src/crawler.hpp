#pragma once

#include <flow.hpp>
#include <reporting.hpp>
#include <runner.hpp>

#include <fmt/compile.h>
#include <inja/inja.hpp>

#include <filesystem>
#include <iostream>
#include <queue>
#include <set>

template <typename FlowType>
class Crawler {
    using flow_t = FlowType;

    // all services needed for the crawler and also required for flow:
    using env_t       = typename flow_t::env_t;
    using store_t     = typename flow_t::store_t;
    using con_man_t   = typename flow_t::con_man_t;
    using reporting_t = typename flow_t::reporting_t;
    using services_t  = typename flow_t::services_t;

    using path_set_data_t = std::pair<std::string, std::filesystem::path>;

    services_t services_;
    std::filesystem::path path_;
    std::string filter_;

    std::set<path_set_data_t> paths_;     // all paths ordered
    std::map<std::string, flow_t> flows_; // key ordered by name; steps queued in order

public:
    Crawler(services_t services, std::filesystem::path path, std::string const &filter)
        : services_{ services }
        , path_{ path }
        , filter_{ filter } { }

    auto crawl() {
        auto path = path_ / "flows";

        if(not std::filesystem::exists(path))
            throw std::runtime_error("given path does not appear to be valid: missing 'flows' sub directory");

        for(auto const &entry : std::filesystem::directory_iterator{ path }) {
            crawl(entry.path());
        }

        for(auto const &[flow_name, path] : paths_) {
            if(not is_passing_filter(flow_name))
                continue;

            if(path.filename() == "script.yaml") {
                auto dir_path = path;
                dir_path.remove_filename();
                flows_.emplace(std::make_pair(flow_name, FlowType{ services_, dir_path }));
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

    void report_detected(std::string const &name) {
        auto const &reporting = services_.template get<reporting_t>();
        reporting.get().record(SimpleEvent{ "DETECT FLOW", name });
    }

    bool is_passing_filter(std::string_view flow_name) const {
        return flow_name.find(filter_) != std::string_view::npos;
    }
};
