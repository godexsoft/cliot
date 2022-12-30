#include <gtest/gtest.h>

#include <flow.hpp>
#include <runner.hpp>
#include <web.hpp>

#include <fmt/color.h>
#include <fmt/compile.h>
#include <yaml-cpp/yaml.h>

struct MockHandler {
    std::string host;
    std::string port;

    std::string request(std::string &&) {
        return "{data}";
    }
};

struct MockFetcher {
    std::string get(std::string const &) const {
        return "{data}";
    }
    std::string post(std::string const &) const {
        return "{data}";
    }
};

TEST(Cliot, ConnectionManagerTest) {
    MockFetcher fetcher;
    ConnectionManager<MockHandler, MockFetcher> man{ "127.0.0.1", "1234", std::cref(fetcher) };
    EXPECT_EQ(man.send(R"({"method":"server_info"})"), "{data}");
    EXPECT_EQ(man.get("http://test.com"), "{data}");
    EXPECT_EQ(man.post("https://another.test.com/something"), "{data}");
}

TEST(Cliot, Yaml) {
    std::string data = R"(---
metadata:
  subject: Example flow
  description: This is an example request-response flow
  author: Alex
  created_on: 27 Dec 2022
  last_update: 28 Dec 2022
  revisions:
  - First version
  - Some modifications done
steps:
- type: response
  file: response.j2
- type: request
  file: request.j2
)";

    YAML::Node doc = YAML::Load(data);

    for(auto const &step : doc["steps"].as<std::vector<Step>>()) {
        fmt::print("+ step: {} in {}\n", step.name, step.file);
    }
}

TEST(Cliot, Flow) {
    auto flow   = Flow<void, void>("/home/godexsoft/Development/cliot/data/flows/1_example");
    auto runner = FlowRunner();
}
