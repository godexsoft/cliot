#pragma once

#include <fmt/color.h>
#include <fmt/compile.h>

#include <algorithm>
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

    std::string operator()(FailureEvent::Data::Type type) const {
        switch(type) {
        case FailureEvent::Data::Type::LOGIC_ERROR:
            return fmt::format(fg(fmt::color::orange_red) | fmt::emphasis::bold, "LOGIC");
        case FailureEvent::Data::Type::NO_MATCH:
            return fmt::format(fg(fmt::color::indian_red) | fmt::emphasis::bold, "NO MATCH");
        case FailureEvent::Data::Type::NOT_EQUAL:
            return fmt::format(fg(fmt::color::indian_red) | fmt::emphasis::bold, "NOT EQUAL");
        case FailureEvent::Data::Type::TYPE_CHECK:
            return fmt::format(fg(fmt::color::indian_red) | fmt::emphasis::bold, "WRONG TYPE");
        }
    }

    std::string operator()(FailureEvent::Data const &failure) const {
        if(verbose < 1)
            return "";
        auto path   = fmt::format(fg(fmt::color::sky_blue) | fmt::emphasis::bold, "{}", failure.path);
        auto detail = [&failure]() -> std::string {
            if(not failure.detail.empty())
                return fmt::format("\n\nExtra detail:\n---\n{}\n---\n", failure.detail);
            return "";
        }();
        return fmt::format("  {} {} [{}]: {}{}",
            fmt::format(fg(fmt::color::red) | fmt::emphasis::bold, "-"),
            this->operator()(failure.type), path, failure.message, detail);
    }

    void operator()(FailureEvent const &ev) const {
        if(verbose < 1)
            return;

        std::vector<std::string> issues;
        std::transform(std::begin(ev.issues), std::end(ev.issues),
            std::back_inserter(issues),
            [this](FailureEvent::Data const &issue) -> std::string {
                return this->operator()(issue);
            });

        std::string flat_issues = fmt::format("{}", fmt::join(issues, "\n"));
        auto title              = fmt::format("Failed '{}':", fmt::format(fg(fmt::color::pale_violet_red) | fmt::emphasis::bold, "{}", ev.path));

        auto all_issues = fmt::format(fg(fmt::color::dark_red) | fmt::emphasis::bold, "{}", flat_issues);
        auto response   = fmt::format(fg(fmt::color::medium_violet_red) | fmt::emphasis::italic, "{}", ev.response);
        auto message    = fmt::format("\n [-] {}\n\nIssues:\n{}\n\nLive response:\n---\n{}\n---", title, all_issues, response);

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

        fmt::print("Request data:\n---\n{}\n---\nStore state:\n---\n{}\n---\n",
            fmt::format(fg(fmt::color::sky_blue) | fmt::emphasis::italic, "{}", ev.data),
            fmt::format(fg(fmt::color::blue_violet) | fmt::emphasis::italic, "{}", ev.store.dump(4)));
    }

    void operator()(ResponseEvent const &ev) const {
        if(verbose < 2)
            return;

        fmt::print(fg(fmt::color::ghost_white), "? | ");
        fmt::print(fg(fmt::color::pale_green) | fmt::emphasis::bold, "RESPONSE ");
        fmt::print(fg(fmt::color::sky_blue) | fmt::emphasis::bold, "{}: {}\n", ev.index, ev.path);

        fmt::print("Response:\n---\n{}\n---\nExpectations:\n---\n{}\n---\n",
            fmt::format(fg(fmt::color::sky_blue) | fmt::emphasis::italic, "{}", ev.response),
            fmt::format(fg(fmt::color::blue_violet) | fmt::emphasis::italic, "{}", ev.expectations));
    }
};

template <typename T>
class AsyncQueue {
public:
    AsyncQueue(const size_t capacity)
        : capacity_{ capacity } {
    }

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
