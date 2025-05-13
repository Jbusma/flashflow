#include "binance_feed.hpp"
#include "orderbook.hpp"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <atomic>
#include <zlib.h>

TEST_CASE("Replay /tmp/ff_live.gz through OrderBook",
          "[replay][binance]") {
    const std::string cap = "/tmp/ff_live.gz";
    if(!std::filesystem::exists(cap)) {
        SUCCEED("no capture found – run live test first");
        return;
    }

    ff::OrderBook ob;
    std::atomic<int> hits{0};

    ff::BinanceFeed feed(
        [&](auto& bp,auto& bq,auto& ap,auto& aq){
            ob.apply_bid_delta(bp,bq);
            ob.apply_ask_delta(ap,aq);
            ++hits;
        },
        /*replay=*/true,
        cap);

    feed.run();
    REQUIRE(hits > 0);

    gzFile gz = gzopen("/tmp/ff_live.gz", "rb");
    if (!gz) {
        SUCCEED("/tmp/ff_live.gz not found – live test was skipped");
        return;
    }
} 