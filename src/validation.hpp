#pragma once

#include <fmt/compile.h>
#include <inja/inja.hpp>

#include <iostream>

#include <events.hpp>

class Validator {
    using issues_vec_t   = std::vector<FailureEvent::Data>;
    issues_vec_t issues_ = {};
    bool is_valid_       = true;

public:
    std::pair<bool, issues_vec_t> validate(inja::json const &expectations, inja::json const &incoming) {
        do_validate("", expectations, incoming);
        return { is_valid_, issues_ };
    }

private:
    void do_validate(std::string path, inja::json const &expectations, inja::json const &incoming) {
        if(expectations.is_null() or expectations.empty())
            return;

        if(expectations.is_object()) {
            for(auto const &expectation : expectations.items()) {
                auto const &key = expectation.key();
                if(not incoming.contains(key)) {
                    auto const full_path = [&path, &key]() -> std::string {
                        if(path.empty())
                            return key;
                        return path + '.' + key;
                    }();
                    add_issue(FailureEvent::Data::Type::NO_MATCH, full_path, "Key is not present in the response");

                } else {
                    do_validate(path + (path.empty() ? "" : ".") + key,
                        expectation.value(),
                        incoming[key]);
                }
            }
        } else {
            if(expectations != incoming) {
                std::stringstream ss;
                ss << expectations << " != " << incoming;
                add_issue(FailureEvent::Data::Type::NOT_EQUAL, path, ss.str());
            }
        }
    }

    void add_issue(FailureEvent::Data::Type type, std::string const &path, std::string const &message) {
        is_valid_ = false;
        issues_.emplace_back(type, path, message);
    }
};
