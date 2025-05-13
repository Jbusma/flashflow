#include "binance_feed.hpp"
#include <catch2/catch_test_macros.hpp>
#include <fstream>
#include <zlib.h>

// plain C-string with escapes (raw string caused clang error)
static const char* kLine =
"{\"b\":[[\"30000.0\",\"0.5\"]],\"a\":[[\"30010.0\",\"1.0\"]]}\n";

TEST_CASE("BinanceFeed replay invokes callback", "[feed]") {
    // 1. write the json line into a gz file inside /tmp
    std::string fname = "/tmp/ff_test.gz";
    gzFile gz = gzopen(fname.c_str(), "wb");
    REQUIRE(gz != nullptr);
    gzputs(gz, kLine);
    gzclose(gz);

    // 2. capture the callback invocation
    int calls = 0;
    ff::BinanceFeed feed(
        [&](auto& bp,auto& bq,auto& ap,auto& aq){
            ++calls;
            REQUIRE(bp[0] == 300000000);   // 30000.0 * 1e4
            REQUIRE(ap[0] == 300100000);   // 30010.0 * 1e4
        },
        /*replay=*/true,
        fname);

    feed.run();
    REQUIRE(calls == 1);
} 