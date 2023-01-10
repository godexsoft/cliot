#include <flow/impl/yaml_conversion.hpp>
#include <flow/impl/yaml_file_loader.hpp>

#include <yaml-cpp/yaml.h>

#include <cassert>
#include <filesystem>
#include <vector>

namespace impl {

YamlFileLoader::YamlFileLoader(std::filesystem::path const &base_path)
    : base_path_{ base_path } {
    assert(std::filesystem::is_directory(base_path));
}

std::vector<descriptor::Step> YamlFileLoader::load() const {
    auto script_path = base_path_ / "script.yaml";
    assert(std::filesystem::exists(script_path));

    YAML::Node doc = YAML::LoadFile(script_path.string());
    return doc["steps"].as<std::vector<descriptor::Step>>();
}

std::filesystem::path YamlFileLoader::base_path() const {
    return base_path_;
}

} // namespace impl
