#include <gtest/gtest.h>

#include <web.hpp>

#include <fmt/compile.h>

struct MockHandler {
    std::string host;
    uint16_t port;

    std::string request(std::string &&) {
        return "{data}";
    }
};

TEST(Cliot, ConnectionManagerTest) {
    ConnectionManager<MockHandler> man{ "127.0.0.1", 1234 };
    EXPECT_EQ(man.send(R"({"method":"server_info"})"), "{data}");
}

struct SimpleEvent {
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
        fmt::print(fg(fmt::color::ghost_white), "- | ");
        fmt::print(fg(fmt::color::red) | fmt::emphasis::bold, "FAIL ");
        fmt::print("'{}': {}\n",
            fmt::format(fg(fmt::color::sky_blue) | fmt::emphasis::bold, "{}", ev.flow_name),
            fmt::format(fg(fmt::color::red) | fmt::emphasis::italic, "{}", ev.path));
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
        Model(T &&ev, Renderer const &renderer)
            : ev{ std::move(ev) }
            , renderer{ std::cref(renderer) } { }

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
        events_.push_back(AnyEvent{ std::move(ev), renderer });
    }

    void sync_print() {
        std::scoped_lock lk{ mtx_ };
        for(auto const &ev : events_) {
            ev.render();
        }
    }
};

TEST(Cliot, Reporting) {
    ReportEngine re;
    DefaultReportRenderer renderer;

    re.record(SimpleEvent{ "HELLO", "world" }, renderer); // add event to timeline;
    re.record(FailureEvent{ "flow1", "path", { "issue1", "issue2" }, "{json}" }, renderer);

    re.sync_print();
}
