#pragma once

#include <flow/flow.hpp>
#include <flow/impl/steps_vec_loader.hpp>
#include <flow/impl/yaml_file_loader.hpp>

#include <di.hpp>
#include <fmt/color.h>
#include <fmt/compile.h>
#include <inja/inja.hpp>

#include <filesystem>

template <typename ConnectionManagerType, typename ReportEngineType>
class DefaultFlowFactory {
public:
    // todo: decouple later, inject inja types
    using env_t       = inja::Environment;
    using store_t     = inja::json;
    using con_man_t   = ConnectionManagerType;
    using reporting_t = ReportEngineType;
    using flow_t      = Flow<con_man_t, reporting_t, Validator, DefaultFlowFactory<con_man_t, reporting_t>>;

    using services_t = di::Deps<con_man_t, reporting_t>;

    DefaultFlowFactory(services_t services)
        : services_{ services } { }

    auto make(std::filesystem::path const &base_path, env_t &env, store_t &store) {
        // we inject *this as the factory so that step::RunFlow can benefit.
        return flow_t{
            di::combine(services_,
                di::Deps<env_t, store_t, DefaultFlowFactory<con_man_t, reporting_t>>{ env, store, *this }),
            impl::YamlFileLoader{ base_path }
        };
    }

    auto make(std::filesystem::path const &base_path, std::vector<descriptor::Step> const &steps, env_t &env, store_t &store) {
        // we inject *this as the factory so that step::RunFlow can benefit.
        return flow_t{
            di::combine(services_,
                di::Deps<env_t, store_t, DefaultFlowFactory<con_man_t, reporting_t>>{ env, store, *this }),
            impl::StepsVecLoader{ base_path, steps }
        };
    }

private:
    services_t services_;
};
