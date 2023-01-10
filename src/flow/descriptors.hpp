#pragma once

#include <string>
#include <variant>
#include <vector>

namespace descriptor {

struct Request;
struct Response;
struct RunFlow;
struct RepeatBlock;

using Step = std::variant<Request, Response, RunFlow, RepeatBlock>;

struct Meta {
    std::string subject, description, author, created_on, last_update;
    std::vector<std::string> revisions;
};

struct Request {
    std::string file;
};

struct Response {
    std::string file;
};

struct RunFlow {
    std::string name;
};

struct RepeatBlock {
    std::vector<Step> steps;
    uint32_t repeat;
};

} // namespace descriptor
