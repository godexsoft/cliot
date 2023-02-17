#include <validation/validator.hpp>

std::pair<bool, Validator::issues_vec_t> Validator::validate(inja::json const &expectations, inja::json const &incoming) {
    do_validate("", expectations, incoming);
    return { is_valid_, issues_ };
}

void Validator::do_validate(std::string path, inja::json const &expectations, inja::json const &incoming) {
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
    } else if(expectations.is_string() and expectations.get<std::string>().starts_with("$")) {
        // check our filters, like "$bool" should pass if the actual type received is a bool
        auto val    = expectations.get<std::string>();
        auto passes = [&val, &incoming]() {
            if(val == "$bool") {
                return incoming.is_boolean();
            } else if(val == "$string") {
                return incoming.is_string();
            } else if(val.starts_with("$array=")) {
                // FIXME: this is a bit of a hack but don't really care atm
                const auto num        = val.substr(7, val.size() - 7);
                std::size_t const len = atoi(num.c_str());
                return incoming.is_array() && incoming.size() == len;
            } else if(val == "$array") {
                return incoming.is_array();
            } else if(val == "$double") {
                return incoming.is_number_float();
            } else if(val == "$int") {
                return incoming.is_number_integer();
            } else if(val == "$uint") {
                return incoming.is_number_unsigned();
            } else if(val == "$object") {
                return incoming.is_object();
            } else {
                return val == incoming;
            }
        }();

        if(!passes) {
            std::stringstream ss;
            ss << expectations << " is not met for value '" << incoming << "'";
            add_issue(FailureEvent::Data::Type::TYPE_CHECK, path, ss.str());
        }

    } else {
        if(expectations != incoming) {
            std::stringstream ss;
            ss << expectations << " != " << incoming;
            add_issue(FailureEvent::Data::Type::NOT_EQUAL, path, ss.str());
        }
    }
}

void Validator::add_issue(FailureEvent::Data::Type type, std::string const &path, std::string const &message) {
    is_valid_ = false;
    issues_.emplace_back(type, path, message);
}
