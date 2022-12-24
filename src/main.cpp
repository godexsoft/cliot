#include <crawler.hpp>
#include <runner.hpp>
#include <scheduler.hpp>
#include <validation.hpp>

#include <cxxopts.hpp>
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
    // parse command line options
    // first arg is path to data directory

    // create runner
    // runner is given the path
    // runner will crawl the path for flows

    // runner will end up with a list of flows to execute
    // flows are found in by data/flows/1_some_flow/1.jrq,1.jrp,2.jrq,2.jrp etc.
    // flows are collections of requests and responses as templates in inja
    // so they can include other subflows or helpers

    // flow consists of two arrays, one with requests and one with responses
    // the flow is executed synchronously. the names of requests and responses
    // are starting from a digit. this determains which request should match which response
    // as well as their order.

    // we have two pointers each pointing at the current request and response in the queue
    // sometimes one request could yield more than one response that we wish to await..
    // so we have to work around that.

    // as the request is executed it's injected with the environment (environment.json)
    // as well as a special entry is created at `$res` which contains the actual response
    // data that we are working with at this moment.

    // since we were able to save things from the response and onto our store
    // we now can inject the store alongside the environment so the next request
    // can benefit from both saved data as well as stock environment values.
    // TBD can the environment json be the store simultaniously? that kinda makes sense.

    // as the request is done and we receive the response, we are going to
    // - load the response template
    // - render it with environment/store injected
    // - send this template and the input request to the validator

    // validator receives two strings with json
    // 1) the expectations object
    // 2) the input response json data
    //
    // the expectations are recursively applied to the input json
    // any failure should result in a thrown exception with a reasonable message
    // we catch the exceptions all the way in the runner, mark the test failed and
    // carry on with the other tests.

    // think about stats and logging/debugging tests

    // Scheduler - top level flow scheduler. combines all other parts
    //   Crawler - find the flows and return back paths to each flow (parsed into hierarchy)
    //   flows   - ordered, perform for each:
    //      Runner  - runs a single flow
    //        Request  - handle response
    //           - asio stuff, store/env, inja parsing
    //        Response - handle response
    //           - asio, store/env, inja
    //           Validator - flow validation

    // this is some sort of context.. maybe wrap this all?
    Environment env;
    json store_ = env.load_json("./data/environment.json");

    auto store_cb = [&store_](Arguments &args) {
        auto value = args.at(0)->get<json>();
        auto var   = args.at(1)->get<std::string>();

        store_[var] = value;
        return value;
    };
    env.add_callback("store", 2, store_cb);
    // context end

    // clang-format off
    cxxopts::Options options("ClioT", "Integration testing runner for Clio");
    options.add_options()
      ("p,path", "Path to data folder", cxxopts::value<std::string>())
      ("h,help", "Print help message and exit")
    ;
    options.parse_positional({"path"});
    // clang-format on

    auto result = options.parse(argc, argv);
    if(result["help"].as<bool>())
        usage(options.help());

    auto path      = result["path"].as<std::string>();
    auto scheduler = Scheduler{ env, Crawler{ env, path } };
    scheduler.run();

    return EXIT_SUCCESS;
} catch(std::exception const &e) {
    fmt::print("{}\n", e.what());
    return EXIT_FAILURE;
}
