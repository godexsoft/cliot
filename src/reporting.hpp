#pragma once

#include <fmt/color.h>
#include <fmt/compile.h>

#include <memory>
#include <mutex>
#include <string>
#include <vector>

struct SimpleEvent {
    SimpleEvent(std::string const &label, std::string const &message)
        : label{ label }
        , message{ message } {
    }
    std::string label;
    std::string message;
};

struct FailureEvent {
    std::string flow_name;
    std::string path;
    std::vector<std::string> issues;
    std::string response;
};

struct DefaultReportRenderer {
    void operator()(SimpleEvent const &ev) const {
        fmt::print(fg(fmt::color::ghost_white), "? | ");
        fmt::print(fg(fmt::color::pale_green) | fmt::emphasis::bold, "{} ", ev.label);
        fmt::print(fg(fmt::color::sky_blue) | fmt::emphasis::bold, "{}\n", ev.message);
    }

    void operator()(FailureEvent const &ev) const {
        std::string flat_issues = fmt::format("{}", fmt::join(ev.issues, "\n"));
        auto message            = fmt::format(
            "Failed '{}':\n{}\nLive response:\n---\n{}\n---",
            ev.path, flat_issues, ev.response);

        fmt::print(fg(fmt::color::ghost_white), "- | ");
        fmt::print(fg(fmt::color::red) | fmt::emphasis::bold, "FAIL ");
        fmt::print("'{}': {}\n",
            fmt::format(fg(fmt::color::sky_blue) | fmt::emphasis::bold, "{}", ev.flow_name),
            fmt::format(fg(fmt::color::red) | fmt::emphasis::italic, "{}", message));
    }
};

class AnyEvent {
public:
    template <typename T, typename Renderer>
    AnyEvent(T &&ev, Renderer const &renderer)
        : pimpl_{ std::make_unique<Model<T, Renderer>>(std::move(ev), renderer) } {
    }

    void render() const {
        pimpl_->render();
    }

private:
    struct Concept {
        virtual ~Concept()          = default;
        virtual void render() const = 0;
    };

    template <typename T, typename Renderer>
    struct Model : public Concept {
        Model(T &&ev, std::reference_wrapper<const Renderer> renderer)
            : ev{ std::move(ev) }
            , renderer{ renderer } { }

        void render() const override {
            renderer.get()(ev);
        }

        T ev;
        std::reference_wrapper<const Renderer> renderer;
    };

private:
    std::unique_ptr<Concept> pimpl_;
};

class ReportEngine {
    std::vector<AnyEvent> events_;
    std::mutex mtx_;

public:
    template <typename EventType, typename RendererType>
    void record(EventType &&ev, RendererType const &renderer) {
        std::scoped_lock lk{ mtx_ };
        events_.emplace_back(std::move(ev), std::cref(renderer));
    }

    void sync_print() {
        std::scoped_lock lk{ mtx_ };
        for(auto const &ev : events_) {
            ev.render();
        }
    }
};
