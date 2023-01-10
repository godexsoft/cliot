#pragma once

#include <flow/descriptors.hpp>

#include <filesystem>
#include <vector>

namespace impl {

class StepsVecLoader {
    std::filesystem::path base_path_;
    std::vector<descriptor::Step> steps_;

public:
    StepsVecLoader(std::filesystem::path const &base_path, std::vector<descriptor::Step> const &steps)
        : base_path_{ base_path }
        , steps_{ steps } { }

    std::vector<descriptor::Step> load() const {
        return steps_;
    }

    std::filesystem::path base_path() const {
        return base_path_;
    }
};

} // namespace impl
