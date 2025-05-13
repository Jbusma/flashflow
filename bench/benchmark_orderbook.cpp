#include <benchmark/benchmark.h>
#include "orderbook.hpp"
#include "binance_feed.hpp"
#include <fstream>

using ff::OrderBook;

static void BM_Scan(benchmark::State& s) {
    OrderBook ob;
    // Prefill book with qty=1 to avoid early exit
    for (std::size_t i=0;i<ff::DEPTH;++i) ob.bids_qty[i]=1, ob.asks_qty[i]=1;
    for (auto _ : s) {
        benchmark::DoNotOptimize(ob.best_bid());
        benchmark::DoNotOptimize(ob.best_ask());
    }
    s.SetItemsProcessed(s.iterations()*2);
}
BENCHMARK(BM_Scan)->Iterations(10'000'000);

static void BM_End2End(benchmark::State& s) {
    OrderBook ob;
    std::ifstream cap("1min_btcusdt_depth.pcap.gz", std::ios::binary);
    std::string all((std::istreambuf_iterator<char>(cap)),
                     std::istreambuf_iterator<char>());
    bool first = true;
    for (auto _ : s) {
        if (!first) s.PauseTiming();  // reset for next iteration
        first=false;
        // --- replay feed into orderbook ----------------------------------
        ff::BinanceFeed feed(
            [&](auto& bp,auto& bq,auto& ap,auto& aq){
                ob.apply_bid_delta(bp,bq);
                ob.apply_ask_delta(ap,aq);
                benchmark::DoNotOptimize(ob.best_bid());
                benchmark::DoNotOptimize(ob.best_ask());
            }, true, "1min_btcusdt_depth.pcap.gz");
        feed.run();
        s.ResumeTiming();
    }
}
BENCHMARK(BM_End2End);
// `main()` now lives in apps/ff_bench.cpp 