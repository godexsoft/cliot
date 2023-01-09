#pragma once

#include <di.hpp>

#include <reporting/events.hpp>

template <typename RendererType>
class ReportEngine {
    using services_t = di::Deps<RendererType>;

    services_t services_;
    bool sync_output_;

public:
    ReportEngine(services_t services, bool sync_output)
        : services_{ services }
        , sync_output_{ sync_output } {
    }

    template <typename EventType>
    void record(EventType &&ev) {
        if(sync_output_)
            services_.template get<RendererType>().get()(ev);
    }
};
