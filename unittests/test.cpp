#include <gtest/gtest.h>

#include <flow/flow.hpp>

#include <fmt/color.h>
#include <fmt/compile.h>
#include <yaml-cpp/yaml.h>

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
