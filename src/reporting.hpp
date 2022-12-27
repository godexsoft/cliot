#pragma once

#include <fmt/color.h>
#include <fmt/compile.h>

#include <condition_variable>
#include <mutex>
#include <string>
#include <vector>

#include <events.hpp>

struct DefaultReportRenderer {
    uint16_t verbose;
    DefaultReportRenderer(uint16_t verbose)
        : verbose{ verbose } { }

    void operator()(SimpleEvent const &ev) const {
        if(verbose < 1)
            return;
        fmt::print(fg(fmt::color::ghost_white), "? | ");
        fmt::print(fg(fmt::color::pale_green) | fmt::emphasis::bold, "{} ", ev.label);
        fmt::print(fg(fmt::color::sky_blue) | fmt::emphasis::bold, "{}\n", ev.message);
    }

    void operator()(SuccessEvent const &ev) const {
        if(verbose < 1)
            return;
        fmt::print(fg(fmt::color::ghost_white), "+ | ");
        fmt::print(fg(fmt::color::pale_green) | fmt::emphasis::bold, "SUCCESS ");
        fmt::print(fg(fmt::color::sky_blue) | fmt::emphasis::bold, "{}\n", ev.flow_name);
    }

    void operator()(FailureEvent const &ev) const {
        if(verbose < 1)
            return;

        std::string flat_issues = fmt::format("{}", fmt::join(ev.issues, "\n"));
        auto message            = fmt::format(
            "Failed '{}':\n{}\nLive response:\n---\n{}\n---",
            ev.path,
            fmt::format(fg(fmt::color::dark_red) | fmt::emphasis::bold, "{}", flat_issues),
            fmt::format(fg(fmt::color::medium_violet_red) | fmt::emphasis::italic, "{}", ev.response));

        fmt::print(fg(fmt::color::ghost_white), "- | ");
        fmt::print(fg(fmt::color::red) | fmt::emphasis::bold, "FAIL ");
        fmt::print("'{}': {}\n",
            fmt::format(fg(fmt::color::sky_blue) | fmt::emphasis::bold, "{}", ev.flow_name),
            message);
    }

    void operator()(RequestEvent const &ev) const {
        if(verbose < 2)
            return;

        fmt::print(fg(fmt::color::ghost_white), "? | ");
        fmt::print(fg(fmt::color::pale_green) | fmt::emphasis::bold, "REQUEST ");
        fmt::print(fg(fmt::color::sky_blue) | fmt::emphasis::bold, "{}: {}\n", ev.index, ev.path);

        fmt::print("Request data:\n---\n{}\n---\nStore state:\n---\n{}\n",
            fmt::format(fg(fmt::color::sky_blue) | fmt::emphasis::italic, "{}", ev.data),
            fmt::format(fg(fmt::color::blue_violet) | fmt::emphasis::italic, "{}", ev.store.dump(4)));
    }

    void operator()(ResponseEvent const &ev) const {
        if(verbose < 2)
            return;

        fmt::print(fg(fmt::color::ghost_white), "? | ");
        fmt::print(fg(fmt::color::pale_green) | fmt::emphasis::bold, "RESPONSE ");
        fmt::print(fg(fmt::color::sky_blue) | fmt::emphasis::bold, "{}: {}\n", ev.index, ev.path);

        fmt::print("Response:\n---\n{}\n---\nExpectations:\n---\n{}\n",
            fmt::format(fg(fmt::color::sky_blue) | fmt::emphasis::italic, "{}", ev.response),
            fmt::format(fg(fmt::color::blue_violet) | fmt::emphasis::italic, "{}", ev.expectations));
    }
};

template <typename T>
class AsyncQueue {
public:
    AsyncQueue(const size_t capacity)
        : capacity_{ capacity } { }

    void enqueue(T element) {
        std::unique_lock l{ mtx_ };
        cv_.wait(l, [this] { return q_.size() < capacity_; });
        q_.push(std::move(element));
        cv_.notify_all();
    }

    T dequeue() {
        std::unique_lock l{ mtx_ };
        cv_.wait(l, [this] { return !q_.empty(); });
        T value = std::move(q_.front());
        q_.pop();

        l.unlock();
        cv_.notify_all();
        return value;
    }

private:
    size_t capacity_;
    std::queue<T> q_;

    mutable std::mutex mtx_;
    std::condition_variable cv_;
};

template <typename RendererType>
class ReportEngine {
    AsyncQueue<AnyEvent> events_{ 500 };
    std::reference_wrapper<const RendererType> renderer_;
    bool sync_output_;

public:
    ReportEngine(RendererType const &renderer, bool sync_output)
        : renderer_{ std::cref(renderer) }
        , sync_output_{ sync_output } {
    }

    template <typename EventType>
    void record(EventType &&ev) {
        if(sync_output_)
            renderer_.get()(ev);
        events_.enqueue(AnyEvent{ std::move(ev), renderer_ });
    }
};
