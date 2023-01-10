#pragma once

#include <flow/exceptions.hpp>
#include <reporting/events.hpp> // probably should not be here, should use report engine public member func instead
#include <runner.hpp>

#include <di.hpp>
#include <fmt/compile.h>

#include <filesystem>
#include <string>
#include <vector>

namespace step {

template <typename EnvType, typename StoreType, typename ConnectionManagerType, typename ReportEngineType, typename FlowFactoryType>
class RunFlow {
    using env_t          = EnvType;
    using store_t        = StoreType;
    using con_man_t      = ConnectionManagerType;
    using reporting_t    = ReportEngineType;
    using flow_factory_t = FlowFactoryType;
    using services_t     = di::Deps<env_t, store_t, con_man_t, reporting_t, flow_factory_t>;

    services_t services_;
    std::string path_;

public:
    RunFlow(services_t services, std::filesystem::path const &path)
        : services_{ services }
        , path_{ path.string() } { }

    RunFlow(RunFlow &&)      = default;
    RunFlow(RunFlow const &) = default;

    void run() {
        try {
            auto const &[env, store] = services_.template get<env_t, store_t>();
            auto runner              = FlowRunner<flow_factory_t>{
                services_, fmt::format("subflow[{}]", path_), path_
            };
            runner.run(env.get(), store.get());
        } catch(FlowException const &e) {
            auto issues = std::vector<FailureEvent::Data>{
                { FailureEvent::Data::Type::LOGIC_ERROR, path_, "Subflow execution failed" }
            };
            issues.insert(std::begin(issues), std::begin(e.issues), std::end(e.issues));
            throw FlowException(path_, issues, "No data");
        }
    }
};

} // namespace step
