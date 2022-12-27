#pragma once

#include <reporting.hpp>
#include <runner.hpp>

#include <fmt/compile.h>
#include <inja/inja.hpp>

#include <filesystem>
#include <iostream>
#include <queue>
#include <set>

template <typename RequestType, typename ResponseType, typename ReportEngineType>
class Crawler {
    // all deps needed for the crawler:
    using env_t       = inja::Environment;
    using reporting_t = ReportEngineType;
    using services_t  = di::Deps<env_t, reporting_t>;

    using path_set_data_t = std::pair<std::string, std::filesystem::path>;
    using data_t          = std::pair<std::queue<RequestType>, std::queue<ResponseType>>;

    services_t services_;
    std::filesystem::path path_;
    std::string filter_;

    std::set<path_set_data_t> paths_;     // all paths ordered
    std::map<std::string, data_t> flows_; // key ordered by name; requests/responses queued in order

public:
    Crawler(services_t services, std::filesystem::path path, std::string const &filter)
        : services_{ services }
        , path_{ path }
        , filter_{ filter } { }

    auto crawl() {
        auto const &env = services_.template get<env_t>();
        auto path       = path_ / "flows";

        if(not std::filesystem::exists(path))
            throw std::runtime_error("given path does not appear to be valid: missing 'flows' sub directory");

        for(auto const &entry : std::filesystem::directory_iterator{ path }) {
            crawl(entry.path());
        }

        for(auto const &[flow_name, path] : paths_) {
            if(not passes_filter(flow_name))
                continue;

            auto candidate = path.extension();
            if(candidate == ".jrq") {
                flows_[flow_name].first.emplace(env, path.string());
            } else if(candidate == ".jrp") {
                flows_[flow_name].second.emplace(env, path.string());
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

    bool passes_filter(std::string_view flow_name) const {
        return flow_name.find(filter_) != std::string_view::npos;
    }
};
