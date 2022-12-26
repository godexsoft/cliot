#pragma once

#include <fmt/compile.h>
#include <inja/inja.hpp>

#include <iostream>

class Validator {
    using issues_vec_t   = std::vector<std::string>;
    issues_vec_t issues_ = {};
    bool is_valid_       = true;

public:
    std::pair<bool, issues_vec_t> validate(inja::json const &expectations, inja::json const &incoming) {
        dovalidate("", expectations, incoming);
        return { is_valid_, issues_ };
    }

private:
    void dovalidate(std::string path, inja::json const &expectations, inja::json const &incoming) {
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
                    add_issue("NO MATCH", full_path, "Key is not present in the response");

                } else {
                    dovalidate(path + (path.empty() ? "" : ".") + key,
                        expectation.value(),
                        incoming[key]);
                }
            }
        } else {
            if(expectations != incoming) {
                std::stringstream ss;
                ss << expectations << " != " << incoming;
                add_issue("NOT EQUAL", path, ss.str());
            }
        }
    }

    void add_issue(std::string_view subject, std::string_view path, std::string_view detail) {
        is_valid_ = false;
        issues_.push_back(fmt::format("*** {} [{}]: {}", subject, path, detail));
    }
};
