#include <gtest/gtest.h>

#include <web.hpp>

#include <fmt/color.h>
#include <fmt/compile.h>

struct MockHandler {
    std::string host;
    std::string port;

    std::string request(std::string &&) {
        return "{data}";
    }
};

struct MockFetcher {
    std::string get(std::string const &) const {
        return "{data}";
    }
    std::string post(std::string const &) const {
        return "{data}";
    }
};

TEST(Cliot, ConnectionManagerTest) {
    MockFetcher fetcher;
    ConnectionManager<MockHandler, MockFetcher> man{ "127.0.0.1", "1234", std::cref(fetcher) };
    EXPECT_EQ(man.send(R"({"method":"server_info"})"), "{data}");
    EXPECT_EQ(man.get("http://test.com"), "{data}");
    EXPECT_EQ(man.post("https://another.test.com/something"), "{data}");
}
