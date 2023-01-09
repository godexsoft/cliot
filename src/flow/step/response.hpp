#pragma once

#include <flow/exceptions.hpp>
#include <reporting/events.hpp> // probably should not be here, should use report engine public member func instead

#include <di.hpp>

#include <exception>
#include <string>
#include <vector>

namespace step {

template <typename EnvType, typename StoreType, typename ConnectionManagerType, typename ReportEngineType, typename ValidatorType, typename EnvError, typename StoreException>
class Response {
    using env_t       = EnvType;
    using store_t     = StoreType;
    using con_man_t   = ConnectionManagerType;
    using reporting_t = ReportEngineType;
    using services_t  = di::Deps<env_t, store_t, con_man_t, reporting_t>;

    services_t services_;
    std::string path_;
    ValidatorType validator_;

public:
    Response(services_t services, std::filesystem::path const &path)
        : services_{ services }
        , path_{ path.string() } { }

    Response(Response &&)      = default;
    Response(Response const &) = default;

    void validate(store_t const &incoming) {
        try {
            auto const &[env, store] = services_.template get<env_t, store_t>();
            store.get()["$res"]      = incoming;

            auto temp   = env.get().parse_template(path_);
            auto result = env.get().render(temp, store);

            try {
                auto expectations = store_t::parse(result);

                report(ResponseEvent{ path_, incoming.dump(4), expectations.dump(4) });
                auto [valid, issues] = validator_.validate(expectations, incoming);

                if(not valid)
                    throw FlowException(path_, issues, incoming.dump(4));
            } catch(StoreException const &e) {
                auto const issues = std::vector<FailureEvent::Data>{
                    { FailureEvent::Data::Type::LOGIC_ERROR, path_, e.what(), result }
                };

                throw FlowException(path_, issues, incoming.dump(4));
            }
        } catch(EnvError const &e) {
            auto const issues = std::vector<FailureEvent::Data>{
                { FailureEvent::Data::Type::LOGIC_ERROR, path_, e.message }
            };

            throw FlowException(path_, issues, incoming.dump(4));
        } catch(FlowException const &e) {
            // just rethrow inner one
            throw e;
        } catch(std::exception const &e) {
            auto const issues = std::vector<FailureEvent::Data>{
                { FailureEvent::Data::Type::LOGIC_ERROR, path_, e.what() }
            };

            throw FlowException(path_, issues, incoming.dump(4));
        }
    }

private:
    void report(auto &&ev) {
        auto const &reporting = services_.template get<reporting_t>();
        reporting.get().record(std::move(ev));
    }
};

} // namespace step
