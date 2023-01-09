#pragma once

#include <reporting/events.hpp>

#include <fmt/compile.h>
#include <inja/inja.hpp>

#include <vector>

class Validator {
    using issues_vec_t   = std::vector<FailureEvent::Data>;
    issues_vec_t issues_ = {};
    bool is_valid_       = true;

public:
    /**
     * @brief 
     * 
     * @param expectations 
     * @param incoming 
     * @return std::pair<bool, issues_vec_t> 
     */
    std::pair<bool, issues_vec_t> validate(inja::json const &expectations, inja::json const &incoming);

private:
    void do_validate(std::string path, inja::json const &expectations, inja::json const &incoming);
    void add_issue(FailureEvent::Data::Type type, std::string const &path, std::string const &message);
};
