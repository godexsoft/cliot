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

void usage(std::string msg) {
    fmt::print("{}\nThe first positional argument must be a path to the data folder\n", msg);
    exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) try {
    // this is some sort of context.. maybe wrap this all?
    Environment env;
    json store = env.load_json("./data/environment.json");

    auto store_cb = [&store](Arguments &args) {
        auto value = args.at(0)->get<json>();
        auto var   = args.at(1)->get<std::string>();

        store[var] = value;
        return value;
    };
    env.add_callback("store", 2, store_cb);
    // context end

    // clang-format off
    cxxopts::Options options("ClioT", "Integration testing runner for Clio");
    options.add_options()
      ("p,path", "Path to data folder", cxxopts::value<std::string>())
      ("h,help", "Print help message and exit")
      ("H,host", "Clio server ip address", cxxopts::value<std::string>()->default_value("127.0.0.1"))
      ("P,port", "The port Clio is running on", cxxopts::value<uint16_t>()->default_value("51233"))
    ;
    options.parse_positional({"path"});
    // clang-format on

    auto result = options.parse(argc, argv);
    if(result["help"].as<bool>())
        usage(options.help());

    auto path = result["path"].as<std::string>();
    auto host = result["host"].as<std::string>();
    auto port = result["port"].as<uint16_t>();

    using env_t          = inja::Environment;
    using rep_t          = ReportEngine;
    using rep_renderer_t = DefaultReportRenderer;
    using store_t        = inja::json;

    using req_t       = Request;
    using resp_t      = Response<Validator>;
    using crawler_t   = Crawler<req_t, resp_t>;
    using con_man_t   = ConnectionManager<OnDemandConnection>;
    using scheduler_t = Scheduler<con_man_t, req_t, resp_t, const rep_renderer_t>;

    rep_t reporting{};
    rep_renderer_t rep_renderer{};

    di::Deps<env_t, rep_t, const rep_renderer_t, store_t> deps{ env, reporting, rep_renderer, store };
    con_man_t con_man{ host, port };
    crawler_t crawler{ deps, path };

    // this nonsense should be just: di::extend(deps, con_man, crawler)
    // todo: find out why it does not work
    scheduler_t scheduler(di::combine(deps, di::Deps<con_man_t, crawler_t>{ con_man, crawler }));
    scheduler.run();

    // prints report after completion
    reporting.sync_print();

    return EXIT_SUCCESS;
} catch(std::exception const &e) {
    fmt::print("{}\n", e.what());
    return EXIT_FAILURE;
}
