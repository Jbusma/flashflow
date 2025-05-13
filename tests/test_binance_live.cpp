#include "binance_feed.hpp"
#include "orderbook.hpp"
#include <catch2/catch_test_macros.hpp>
#include <atomic>
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <zlib.h>
#include <nlohmann/json.hpp>

#if FF_HAS_IXWS   // ───────────────────────────────────────────── live test only
TEST_CASE("Live Binance depth stream (BTC/USDT)", "[live][binance]") {
    constexpr int WANT_LINES = 20;          // grab this many snapshots max
    std::atomic<int> got{0};
    std::mutex m;
    std::condition_variable cv;

    // open gzip recorder – will be reused by the replay test
    gzFile gz = gzopen("/tmp/ff_live.gz", "wb");
    REQUIRE(gz != nullptr);

    ff::OrderBook ob;
    ff::BinanceFeed feed(
        [&](auto& bp,auto& bq,auto& ap,auto& aq){
            // engine response
            ob.apply_bid_delta(bp,bq);
            ob.apply_ask_delta(ap,aq);
            (void)ob.best_bid();   // touch engine fast/slow path
            // record raw JSON back from feed's internal buffer
            // (we reconstruct it from deltas so the file is valid JSON)
            nlohmann::json j;
            for(std::size_t i=0;i<bp.size();++i)
                j["b"].push_back({bp[i]/1e4, bq[i]/1e8});
            for(std::size_t i=0;i<ap.size();++i)
                j["a"].push_back({ap[i]/1e4, aq[i]/1e8});
            std::string line=j.dump()+"\n";
            gzputs(gz, line.c_str());

            if(++got >= WANT_LINES) cv.notify_one();
        },
        /*replay=*/false,
        ""   // default BTCUSDT depth20 WS
    );

    std::thread th([&]{ feed.run(); });

    // wait max 10 s
    std::unique_lock<std::mutex> lk(m);
    bool ok = cv.wait_for(lk,std::chrono::seconds(10),
                          [&]{ return got.load() >= WANT_LINES; });
    REQUIRE(ok);

    // cleanup
    gzclose(gz);
    pthread_cancel(th.native_handle());   // quickest way out of ix::WebSocket
    th.join();
}
#else
TEST_CASE("Live Binance depth stream (skipped – no ixwebsocket)",
          "[live-skip][binance]")
{
    SUCCEED("ixwebsocket not found – test skipped");
}
#endif   // <-- add this line 