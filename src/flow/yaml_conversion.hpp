#pragma once

#include <flow/descriptors.hpp>

#include <yaml-cpp/yaml.h>

namespace YAML {

template <>
struct convert<descriptor::Request> {
    static bool decode(const Node &node, descriptor::Request &rhs) {
        rhs.file = node["file"].as<std::string>();
        return true;
    }
};

template <>
struct convert<descriptor::Response> {
    static bool decode(const Node &node, descriptor::Response &rhs) {
        rhs.file = node["file"].as<std::string>();
        return true;
    }
};

template <>
struct convert<descriptor::RunFlow> {
    static bool decode(const Node &node, descriptor::RunFlow &rhs) {
        rhs.name = node["name"].as<std::string>();
        return true;
    }
};

template <>
struct convert<descriptor::RepeatBlock> {
    static bool decode(const Node &node, descriptor::RepeatBlock &rhs) {
        if(node["repeat"])
            rhs.repeat = node["repeat"].as<uint32_t>();
        rhs.steps = node["steps"].as<std::vector<descriptor::Step>>();
        return true;
    }
};

template <>
struct convert<descriptor::Step> {
    static bool decode(const Node &node, descriptor::Step &rhs) {
        if(not node.IsMap())
            return false;

        auto type = node["type"].as<std::string>();
        if(type == "request")
            rhs = node.as<descriptor::Request>();
        else if(type == "response")
            rhs = node.as<descriptor::Response>();
        else if(type == "run_flow")
            rhs = node.as<descriptor::RunFlow>();
        else if(type == "block")
            rhs = node.as<descriptor::RepeatBlock>();
        else
            return false;
        return true;
    }
};

} // namespace YAML
