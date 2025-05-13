#include "orderbook.hpp"
#include <catch2/catch_test_macros.hpp>
#include <vector>

TEST_CASE("OrderBook best_bid / best_ask happy-path", "[orderbook]") {
    ff::OrderBook ob;
    // empty book ⇒ -1
    REQUIRE(ob.best_bid() == -1);
    REQUIRE(ob.best_ask() == -1);

    // add one bid @ index 0, ask @ 2
    ob.bids_price[0] = 300'0000;  ob.bids_qty[0] = 1;
    ob.asks_price[2] = 301'0000;  ob.asks_qty[2] = 2;

    REQUIRE(ob.best_bid() == 0);
    REQUIRE(ob.best_ask() == 2);
}

TEST_CASE("apply_*_delta overwrites in-place", "[orderbook]") {
    ff::OrderBook ob;
    std::vector<int64_t> px{300'0000, 299'9000};
    std::vector<int64_t> qt{1, 2};

    ob.apply_bid_delta(px, qt);
    REQUIRE(ob.bids_price[0] == 300'0000);
    REQUIRE(ob.bids_qty[1]  == 2);
} 