#pragma once

#include <flow/descriptors.hpp>

#include <filesystem>
#include <vector>

namespace impl {

class YamlFileLoader {
    std::filesystem::path base_path_;

public:
    YamlFileLoader(std::filesystem::path const &base_path);
    std::vector<descriptor::Step> load() const;
    std::filesystem::path base_path() const;
};

} // namespace impl
