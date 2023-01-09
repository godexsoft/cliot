#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <queue>

namespace util {

template <typename T>
class AsyncQueue {
public:
    template <typename Fn>
    AsyncQueue(
        std::size_t const capacity, Fn deleter = [](T &) {})
        : capacity_{ capacity }
        , deleter_{ deleter } { }

    void enqueue(T const &element) {
        std::unique_lock l{ mtx_ };
        cv_.wait(l, [this] { return q_.size() < capacity_ || stop_requested_; });

        if(stop_requested_)
            return;

        q_.push(element);
        cv_.notify_all();
    }

    void stop() {
        std::unique_lock l{ mtx_ };
        stop_requested_ = true;
        while(not q_.empty()) {
            deleter_(q_.front());
            q_.pop();
        }
        cv_.notify_all();
    }

    [[nodiscard]] std::size_t size() const {
        std::scoped_lock l{ mtx_ };
        return q_.size();
    }

    [[nodiscard]] std::optional<T> dequeue() {
        std::unique_lock l{ mtx_ };
        cv_.wait(l, [this] { return !q_.empty() || stop_requested_; });

        if(stop_requested_)
            return {};

        auto value = q_.front();
        q_.pop();

        l.unlock();
        cv_.notify_all();
        return std::make_optional<T>(value);
    }

private:
    std::size_t capacity_;
    std::function<void(T &)> deleter_;
    std::queue<T> q_;

    mutable std::mutex mtx_;
    std::condition_variable cv_;
    bool stop_requested_ = false;
};

} // namespace util
