#pragma once

#include <fmt/color.h>
#include <fmt/compile.h>
#include <inja/inja.hpp>

#include <filesystem>
#include <iostream>
#include <sstream>
#include <string_view>
#include <vector>

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
    Request(inja::Environment &env, std::filesystem::path path)
        : env_{ std::ref(env) }
        , index_{ parse_index(path) }
        , path_{ path } {
    }

    std::string index() const {
        return index_;
    }

    // perform request (asio)
    inja::json perform(inja::json const &store) {
        auto temp = env_.get().parse_template(path_);
        auto res  = env_.get().render(temp, store);
        // fmt::print("+ performing req: '{}'\n", res);

        return inja::json::parse(R"({
  "result": {
    "info": {
      "complete_ledgers": "24147251-24182121",
      "counters": {
        "rpc": {
          "ledger_entry": {
            "started": "1",
            "finished": "0",
            "errored": "1",
            "forwarded": "0",
            "duration_us": "0"
          }
        },
        "work_queue": {
          "queued": 2,
          "queued_duration_us": 62,
          "current_queue_size": 1,
          "max_queue_size": 4294967295
        },
        "subscriptions": {
          "ledger": 0,
          "transactions": 0,
          "transactions_proposed": 0,
          "manifests": 0,
          "validations": 0,
          "account": 0,
          "accounts_proposed": 0,
          "books": 0,
          "book_changes": 0
        }
      },
      "load_factor": 1,
      "clio_version": "1.0.3+c7c5d88",
      "validated_ledger" : {},
      "cache": {
        "size": 255488,
        "is_full": false,
        "latest_ledger_seq": 24182121,
        "object_hit_rate": 0,
        "successor_hit_rate": 1
      },
      "etl": {
        "etl_sources": [
          {
            "probing": {
              "ws": {
                "validated_range": "N/A",
                "is_connected": "0",
                "ip": "127.0.0.1",
                "ws_port": "6006",
                "grpc_port": "50051"
              },
              "wss": {
                "validated_range": "N/A",
                "is_connected": "0",
                "ip": "127.0.0.1",
                "ws_port": "6006",
                "grpc_port": "50051"
              }
            }
          }
        ],
        "is_writer": false,
        "read_only": false
      }
    },
    "validated": true,
    "status": "success"
  },
  "warnings": [
    {
      "id": 2001,
      "message": "This is a clio server. clio only serves validated data. If you want to talk to rippled, include 'ledger_index':'current' in your request"
    },
    {
      "id": 2002,
      "message": "This server may be out of date"
    }
  ]
}
)");
    }
};

class Response {
    std::reference_wrapper<inja::Environment> env_;
    std::string index_;
    std::string path_; // full path to response file

    std::string issues_ = {};
    bool is_valid_      = true;

public:
    Response(inja::Environment &env, std::filesystem::path path)
        : env_{ std::ref(env) }
        , index_{ parse_index(path) }
        , path_{ path } {
    }
    Response(Response const &) = default;
    Response(Response &&)      = default;

    void validate(inja::json const &store, inja::json const &incoming) {
        inja::json data = store;
        data["$res"]    = incoming;

        auto temp         = env_.get().parse_template(path_);
        auto result       = env_.get().render(temp, data);
        auto expectations = inja::json::parse(result);

        dovalidate("", expectations, incoming);
        if(not is_valid_)
            throw std::runtime_error("Failed '" + path_ + "': " + issues_);
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
                    add_issue("NO MATCH", std::string{ path } + '.' + key, "Key is not present in the response");

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

class FlowRunner {
    std::reference_wrapper<inja::Environment> env_;
    std::string_view name_;
    std::vector<Request> requests_;
    std::vector<Response> responses_;

public:
    FlowRunner(
        inja::Environment &env,
        std::string_view name,
        std::vector<Request> const &requests,
        std::vector<Response> const &responses)
        : env_{ std::ref(env) }
        , name_{ name }
        , requests_{ requests }
        , responses_{ responses } {
    }

    // run the flow which can throw
    void run() {
        report_running();
        auto store = env_.get().load_json("./data/environment.json");

        for(auto &req : requests_) {
            auto result = req.perform(store);

            for(auto &resp : responses_) {
                if(req.index() == resp.index()) {
                    // we need to wait for second response to arrive if we
                    // already handled initial result...
                    resp.validate(store, result);
                }
            }
        }
    }

private:
    void report_running() {
        fmt::print(fg(fmt::color::ghost_white), "? | ");
        fmt::print(fg(fmt::color::pale_green) | fmt::emphasis::bold, "RUN FLOW ");
        fmt::print(fg(fmt::color::sky_blue) | fmt::emphasis::bold, "{}\n", name_);
    }
};
