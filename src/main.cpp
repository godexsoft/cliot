#include <crawler.hpp>
#include <reporting.hpp>
#include <runner.hpp>
#include <scheduler.hpp>
#include <validation.hpp>

#include <cxxopts.hpp>
#include <di.hpp>
#include <fmt/compile.h>

using rep_renderer_t = DefaultReportRenderer;
using reporting_t    = ReportEngine<rep_renderer_t>;
using fetcher_t      = OnDemandFetcher;
using con_man_t      = ConnectionManager<AsyncConnectionPool, fetcher_t>;
using flow_factory_t = DefaultFlowFactory<con_man_t, reporting_t>;
using crawler_t      = Crawler<reporting_t>;
using scheduler_t    = Scheduler<flow_factory_t, con_man_t, reporting_t, crawler_t>;

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

int main(int argc, char **argv) try {
    auto result  = parse_options(argc, argv);
    auto path    = result["path"].as<std::string>();
    auto host    = result["host"].as<std::string>();
    auto port    = result["port"].as<uint16_t>();
    auto filter  = result["filter"].as<std::string>();
    auto verbose = result["verbose"].as<uint16_t>();

    rep_renderer_t renderer{ verbose };
    auto reporting_deps = di::Deps<rep_renderer_t>{ renderer };
    reporting_t reporting{ reporting_deps, true };

    di::Deps<reporting_t> base_deps{ reporting };

    fetcher_t fetcher{};
    con_man_t con_man{ host, std::to_string(port), fetcher };
    crawler_t crawler{ base_deps, path, filter };

    auto flow_deps = di::combine(base_deps, di::Deps<con_man_t>{ con_man });
    flow_factory_t flow_factory{ flow_deps };

    // todo: find out why di::extend() does not work
    auto scheduler_deps = di::combine(flow_deps, di::Deps<flow_factory_t, crawler_t>{ flow_factory, crawler });
    scheduler_t scheduler{ scheduler_deps };

    return scheduler.run();
} catch(std::exception const &e) {
    fmt::print("{}\n", e.what());
    return EXIT_FAILURE;
}
