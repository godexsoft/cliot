#pragma once

#include <yaml-cpp/yaml.h>

#include <string>
#include <vector>

struct Meta {
    std::string subject, description, author, created_on, last_update;
    std::vector<std::string> revisions;
};

// todo: refactor to use custom struct per type, maybe via std::variant?
struct Step {
    enum class Type {
        INVALID_TYPE,
        REQUEST,
        RESPONSE,
        RUN_FLOW,
        INJECT,
        REPEAT_BLOCK,
    };

    Type type = Type::INVALID_TYPE;
    std::string name;
    YAML::Node data;
    std::string file;
    std::optional<std::vector<Step>> steps;
    std::optional<uint32_t> repeat; // only for block step
};

namespace YAML {
template <>
struct convert<Step::Type> {
    static bool decode(const Node &node, Step::Type &rhs) {
        if(not node.IsScalar())
            return false;
        auto type = node.as<std::string>();
        if(type == "request")
            rhs = Step::Type::REQUEST;
        else if(type == "response")
            rhs = Step::Type::RESPONSE;
        else if(type == "run_flow")
            rhs = Step::Type::RUN_FLOW;
        else if(type == "inject")
            rhs = Step::Type::INJECT;
        else if(type == "block")
            rhs = Step::Type::REPEAT_BLOCK;
        else
            return false;
        return true;
    }
};

template <>
struct convert<Step> {
    static bool decode(const Node &node, Step &rhs) {
        if(not node.IsMap())
            return false;

        rhs.type = node["type"].as<Step::Type>();
        if(node["name"])
            rhs.name = node["name"].as<std::string>();
        if(node["file"])
            rhs.file = node["file"].as<std::string>();
        if(node["data"])
            rhs.data = node["data"];
        if(node["repeat"])
            rhs.repeat = node["repeat"].as<uint32_t>();
        if(node["steps"])
            rhs.steps = node["steps"].as<std::vector<Step>>();

        return true;
    }
};
}
