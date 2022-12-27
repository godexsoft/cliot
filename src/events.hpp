#pragma once

#include <inja/inja.hpp>

#include <chrono>
#include <memory>
#include <string>
#include <vector>

struct MetaEvent {
    std::chrono::system_clock::time_point time = std::chrono::system_clock::now();
};

struct SimpleEvent : public MetaEvent {
    SimpleEvent(std::string const &label, std::string const &message)
        : MetaEvent{}
        , label{ label }
        , message{ message } {
    }
    std::string label;
    std::string message;
};

struct SuccessEvent : public MetaEvent {
    SuccessEvent(
        std::string const &flow_name)
        : MetaEvent{}
        , flow_name{ flow_name } { }
    std::string flow_name;
};

struct FailureEvent : public MetaEvent {
    FailureEvent(
        std::string const &flow_name,
        std::string const &path,
        std::vector<std::string> const &issues,
        std::string const &response)
        : MetaEvent{}
        , flow_name{ flow_name }
        , path{ path }
        , issues{ issues }
        , response{ response } { }
    std::string flow_name;
    std::string path;
    std::vector<std::string> issues;
    std::string response;
};

struct RequestEvent : public MetaEvent {
    RequestEvent(
        std::string const &path,
        std::string const &index,
        inja::json const &store,
        std::string const &data)
        : MetaEvent{}
        , path{ path }
        , index{ index }
        , store{ store }
        , data{ data } { }
    std::string path;
    std::string index;
    inja::json store;
    std::string data;
};

struct ResponseEvent : public MetaEvent {
    ResponseEvent(
        std::string const &path,
        std::string const &index,
        std::string const &response,
        std::string const &expectations)
        : MetaEvent{}
        , path{ path }
        , index{ index }
        , response{ response }
        , expectations{ expectations } { }
    std::string path;
    std::string index;
    std::string response;
    std::string expectations;
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
