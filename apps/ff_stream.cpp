#include "orderbook.hpp"
#include "binance_feed.hpp"
#include <iostream>
#include <chrono>
#include <thread>

int main(int argc, char** argv) {
    bool replay=false;
    std::string src;
    for (int i=1;i<argc;++i) {
        if(std::string(argv[i])=="--replay" && i+1<argc) { replay=true; src=argv[++i]; }
    }

    ff::OrderBook ob;
    ff::BinanceFeed feed(
        [&](auto& bp,auto& bq,auto& ap,auto& aq){
            ob.apply_bid_delta(bp,bq);
            ob.apply_ask_delta(ap,aq);
        }, replay, src);

    std::thread t([&](){ feed.run(); });

    while (true) {
        auto ts = std::chrono::steady_clock::now();
        static auto next = ts;
        if (ts>=next) {
            int bi=ob.best_bid(), ai=ob.best_ask();
            std::cout << "BID " << ob.bids_price[bi]/1e4 << " / "
                      << "ASK " << ob.asks_price[ai]/1e4 << '\n';
            next += std::chrono::milliseconds(100);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    t.join();
} 