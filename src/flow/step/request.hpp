#pragma once

#include <flow/exceptions.hpp>
#include <reporting/events.hpp> // probably should not be here, should use report engine public member func instead

#include <di.hpp>

#include <exception>
#include <string>
#include <vector>

namespace step {

template <typename EnvType, typename StoreType, typename ConnectionManagerType, typename ReportEngineType>
class Request {
    using env_t       = EnvType;
    using store_t     = StoreType;
    using con_man_t   = ConnectionManagerType;
    using reporting_t = ReportEngineType;
    using services_t  = di::Deps<env_t, store_t, con_man_t, reporting_t>;

    services_t services_;
    std::string path_;

public:
    Request(services_t services, std::filesystem::path const &path)
        : services_{ services }
        , path_{ path.string() } {
    }

    Request(Request &&)      = default;
    Request(Request const &) = default;

    auto perform() {
        try {
            auto const &[env, con_man, store] = services_.template get<env_t, con_man_t, store_t>();
            auto temp                         = env.get().parse_template(path_);
            auto res                          = env.get().render(temp, store);

            report(RequestEvent{ path_, store, inja::json::parse(res).dump(4) });
            return con_man.get().request(std::move(res));
        } catch(std::exception const &e) {
            auto const issues = std::vector<FailureEvent::Data>{
                { FailureEvent::Data::Type::LOGIC_ERROR, path_, e.what() }
            };
            throw FlowException(path_, issues, "No data");
        }
    }

private:
    void report(auto &&ev) {
        auto const &reporting = services_.template get<reporting_t>();
        reporting.get().record(std::move(ev));
    }
};

} // namespace step
