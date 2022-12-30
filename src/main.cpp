#include <crawler.hpp>
#include <reporting.hpp>
#include <runner.hpp>
#include <scheduler.hpp>
#include <validation.hpp>

#include <cxxopts.hpp>
#include <di.hpp>
#include <fmt/compile.h>
#include <inja/inja.hpp>

#include <filesystem>
#include <iostream>

using namespace inja;

using env_t          = inja::Environment;
using rep_renderer_t = DefaultReportRenderer;
using rep_t          = ReportEngine<rep_renderer_t>;
using store_t        = inja::json;

using validator_t = Validator;
using fetcher_t   = OnDemandFetcher;
using con_man_t   = ConnectionManager<OnDemandConnection, fetcher_t>;
using flow_t      = Flow<con_man_t, rep_t, validator_t>;
using crawler_t   = Crawler<flow_t>;
using scheduler_t = Scheduler<flow_t, crawler_t>;

void usage(std::string msg) {
    fmt::print("{}\nThe first positional argument must be a path to the data folder\n", msg);
    exit(EXIT_SUCCESS);
}

auto parse_options(int argc, char **argv) {
    // clang-format off
    cxxopts::Options options("ClioT", "Integration testing runner for Clio");
    options.add_options()
      ("v,verbose", "Level of output verbosity", cxxopts::value<uint16_t>()->default_value("1"))
      ("p,path", "Path to data folder", cxxopts::value<std::string>())
      ("h,help", "Print help message and exit")
      ("H,host", "Clio server ip address", cxxopts::value<std::string>()->default_value("127.0.0.1"))
      ("P,port", "The port Clio is running on", cxxopts::value<uint16_t>()->default_value("51233"))
      ("f,filter", "Filter flows for execution", cxxopts::value<std::string>()->default_value(""))
    ;
    options.parse_positional({"path"});
    // clang-format on

    auto result = options.parse(argc, argv);
    if(result["help"].as<bool>())
        usage(options.help());

    return result;
}

void register_extensions(env_t &env, store_t &store, rep_t &reporting, con_man_t &con_man) {
    auto store_cb = [&store](Arguments &args) {
        auto value = args.at(0)->get<inja::json>();
        auto var   = args.at(1)->get<std::string>();

        store[var] = value;
        return value;
    };
    env.add_callback("store", 2, store_cb);

    auto report_cb = [&reporting](Arguments &args) {
        auto value = args.at(0)->get<std::string>();
        reporting.record(SimpleEvent{ "CUSTOM", value });
    };
    env.add_void_callback("report", 1, report_cb);

    auto http_fetch_cb = [&store, &con_man, &reporting](Arguments &args) {
        auto url = args.at(0)->get<std::string>();
        auto var = args.at(1)->get<std::string>();

        reporting.record(SimpleEvent{ "FETCH", url + " into " + var });
        auto value = con_man.get(url);
        if(not value.empty()) {
            store[var] = value;
        }
    };
    env.add_void_callback("fetch", 2, http_fetch_cb);

    auto http_fetch_json_cb = [&store, &con_man, &reporting](Arguments &args) {
        auto url = args.at(0)->get<std::string>();
        auto var = args.at(1)->get<std::string>();

        reporting.record(SimpleEvent{ "FETCH JSON", url + " into " + var });
        auto value = con_man.post(url);
        if(not value.empty()) {
            store[var] = inja::json::parse(value);
        }
    };
    env.add_void_callback("fetch_json", 2, http_fetch_json_cb);
}

int main(int argc, char **argv) try {
    auto result  = parse_options(argc, argv);
    auto path    = result["path"].as<std::string>();
    auto host    = result["host"].as<std::string>();
    auto port    = result["port"].as<uint16_t>();
    auto filter  = result["filter"].as<std::string>();
    auto verbose = result["verbose"].as<uint16_t>();

    // this is some sort of context.. maybe wrap this all?
    env_t env;
    store_t store = env.load_json((std::filesystem::path{ path } / "environment.json").string());
    // context end

    rep_renderer_t renderer{ verbose };
    rep_t reporting{ renderer, true };

    fetcher_t fetcher{};

    di::Deps<env_t, rep_t, store_t> deps{ env, reporting, store };
    con_man_t con_man{ host, std::to_string(port), fetcher };

    register_extensions(env, store, reporting, con_man);

    // this nonsense should be just: di::extend(deps, con_man, crawler)
    // todo: find out why it does not work
    crawler_t crawler{ di::combine(deps, di::Deps<con_man_t>{ con_man }), path, filter };
    scheduler_t scheduler(di::combine(deps, di::Deps<con_man_t, crawler_t>{ con_man, crawler }));
    scheduler.run();

    return EXIT_SUCCESS;
} catch(std::exception const &e) {
    fmt::print("{}\n", e.what());
    return EXIT_FAILURE;
}
