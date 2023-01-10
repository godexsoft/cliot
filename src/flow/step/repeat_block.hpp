#pragma once

#include <flow/exceptions.hpp>
#include <runner.hpp>

#include <di.hpp>
#include <fmt/compile.h>

#include <exception>
#include <filesystem>
#include <string>
#include <vector>

namespace step {

template <typename EnvType, typename StoreType, typename ConnectionManagerType, typename ReportEngineType, typename FlowFactoryType>
class RepeatBlock {
    using env_t          = EnvType;
    using store_t        = StoreType;
    using con_man_t      = ConnectionManagerType;
    using reporting_t    = ReportEngineType;
    using flow_factory_t = FlowFactoryType;
    using services_t     = di::Deps<env_t, store_t, con_man_t, reporting_t, flow_factory_t>;

    services_t services_;
    std::string path_;
    uint32_t repeat_;
    std::vector<descriptor::Step> steps_;

public:
    RepeatBlock(services_t services, std::filesystem::path const &path, uint32_t repeat, std::vector<descriptor::Step> const &steps)
        : services_{ services }
        , path_{ path.string() }
        , repeat_{ repeat }
        , steps_{ steps } { }

    RepeatBlock(RepeatBlock &&)      = default;
    RepeatBlock(RepeatBlock const &) = default;

    void run() {
        for(uint32_t i = 0; i < repeat_; ++i) {
            try {
                auto const &[env, store, factory] = services_.template get<env_t, store_t, flow_factory_t>();
                auto flow                         = factory.get().make(path_, steps_, env, store);
                auto runner                       = FlowRunner<flow_factory_t>{
                    services_, fmt::format("block[{}]", i + 1), path_
                };
                runner.run(env.get(), store.get(), flow);
            } catch(FlowException const &e) {
                auto issues = std::vector<FailureEvent::Data>{
                    { FailureEvent::Data::Type::LOGIC_ERROR, path_, "Block execution failed" }
                };
                issues.insert(std::begin(issues), std::begin(e.issues), std::end(e.issues));
                throw FlowException(path_, issues, "No data");
            }
        }
    }
};

} // namespace step
