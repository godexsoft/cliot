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
