#include <gtest/gtest.h>

#include <flow/flow.hpp>

#include <fmt/color.h>
#include <fmt/compile.h>
#include <yaml-cpp/yaml.h>

// TEST(Cliot, Yaml) {
//     std::string data = R"(---
// metadata:
//   subject: Example flow
//   description: This is an example request-response flow
//   author: Alex
//   created_on: 27 Dec 2022
//   last_update: 28 Dec 2022
//   revisions:
//   - First version
//   - Some modifications done
// steps:
// - type: response
//   file: response.j2
// - type: request
//   file: request.j2
// )";

//     YAML::Node doc = YAML::Load(data);

//     // for(auto const &step : doc["steps"].as<std::vector<descriptor::Step>>()) {
//     //     std::visit( overloaded {
//     //             [this](descriptor::Request &req) {
//     //                 steps_.push_back(request_step_t{ services_, path_ / req.file });
//     //             },
//     //             [this](descriptor::Response &resp) {
//     //                 steps_.push_back(response_step_t{ services_, path_ / resp.file });
//     //             },
//     //             [this](descriptor::RunFlow &flow) {
//     //                 steps_.push_back(run_flow_step_t{ services_, path_.parent_path().parent_path() / flow.name });
//     //             },
//     //             [this](descriptor::RepeatBlock &block) {
//     //                 steps_.push_back(repeat_block_step_t{ services_, path_, block.repeat, block.steps });
//     //             }},
//     //         step);
//     //     // fmt::print("+ step: {} in {}\n", step.name, step.file);
//     // }
// }
