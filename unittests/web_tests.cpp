#include <gtest/gtest.h>

#include <web/async_connection_pool.hpp>
#include <web/connection_manager.hpp>

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

// TEST(Web, PoolConnections) {
//     try {
//         auto pool = AsyncConnectionPool{ "127.0.0.1", "51233" };
//         auto con  = pool.borrow(); // blocks waiting for connection here

//         con->write("{}");
//         auto resp = con->read_one();
//         fmt::print("resp: {}\n", resp);
//     } catch(std::exception const &e) {
//         fmt::print("oops: {}\n", e.what());
//     }
// }

TEST(Web, ConManTest) {
    MockFetcher fetcher;
    ConnectionManager<AsyncConnectionPool, MockFetcher> man{ "127.0.0.1", "51233", std::cref(fetcher) };

    auto link = man.request(R"({"method":"server_info"})"); // this now should block until connection is established
    auto resp = link->read_one();
    EXPECT_NE(resp, "{data}");

    EXPECT_EQ(man.get("http://test.com"), "{data}");
    EXPECT_EQ(man.post("https://another.test.com/something"), "{data}");
}
