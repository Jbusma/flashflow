#include "binance_feed.hpp"
#include "orderbook.hpp"
#include <nlohmann/json.hpp>
#include <zlib.h>

#if FF_HAS_IXWS                  // set by CMake; default 0
#   include <ixwebsocket/IXWebSocket.h>
#   include <ixwebsocket/IXHttpClient.h>
#endif

#include <deque>            // <— NEW

namespace ff {

struct BinanceFeed::Impl {
    UpdateCB cb;
    bool replay;
    std::string src;

#if FF_HAS_IXWS
    // ----- live-stream state ---------------------------------------------
    bool          got_snapshot   = false;
    int64_t       last_update_id = 0;
    std::deque<nlohmann::json> buf;          // buffered deltas before snapshot
#endif
};

// ---------- ctor ------------------------------------------------------------
BinanceFeed::BinanceFeed(UpdateCB c, bool rep, const std::string& s)
: p_(new Impl{std::move(c), rep, s}) {}

// ---------- run() -----------------------------------------------------------
void BinanceFeed::run() {
#if FF_HAS_IXWS
    if (!p_->replay) {
        ix::WebSocket ws;
        ws.setUrl(p_->src.empty()
                  ? "wss://stream.binance.com:9443/ws/btcusdt@depth20@100ms"
                  : p_->src);

        ws.setOnMessageCallback([this](const ix::WebSocketMessagePtr &msg)
        {
            if (msg->type != ix::WebSocketMessageType::Message) return;
            const auto j = nlohmann::json::parse(msg->str, nullptr, false);
            if (!j.is_object()) return;

            // ----------------------------------------------------------------
            // 1. Buffer until we have the snapshot
            // ----------------------------------------------------------------
            if (!p_->got_snapshot) {
                p_->buf.emplace_back(j);
                if (p_->buf.size() == 1) {
                    // first delta ⇒ kick off REST snapshot in the background
                    std::thread([this]{
                        ix::HttpClient cli;
                        auto res = cli.get("https://api.binance.com/api/v3/depth"
                                           "?symbol=BTCUSDT&limit=5000");
                        if (res.statusCode != 200) return;   // give up silently
                        const auto snap = nlohmann::json::parse(res.body);
                        int64_t snap_id = snap["lastUpdateId"];
                        // ---- discard deltas with u <= snap_id ----------------
                        while (!p_->buf.empty() &&
                               p_->buf.front()["u"].get<int64_t>() <= snap_id)
                            p_->buf.pop_front();
                        if (p_->buf.empty()) return;         // will resync soon
                        // first remaining delta must now satisfy U ≤ snap_id+1 ≤ u
                        const auto& first = p_->buf.front();
                        if (snap_id + 1 < first["U"].get<int64_t>() ||
                            snap_id + 1 > first["u"].get<int64_t>()) return;

                        // ---- ship the snapshot to the client -----------------
                        std::vector<int64_t> bp,bq,ap,aq;
                        auto load_side=[&](const char* key,
                                           std::vector<int64_t>& p,
                                           std::vector<int64_t>& q){
                            for (auto& lv : snap[key])
                            {
                                double px  = std::stod(lv[0].get<std::string>());
                                double qty = std::stod(lv[1].get<std::string>());
                                p.push_back(static_cast<int64_t>(px  * 1e4));
                                q.push_back(static_cast<int64_t>(qty * 1e8));
                            }
                        };
                        load_side("bids",bp,bq);
                        load_side("asks",ap,aq);
                        p_->cb(bp,bq,ap,aq);

                        // ---- process the buffered deltas ---------------------
                        p_->last_update_id = snap_id;
                        p_->got_snapshot   = true;
                        for (auto& d : p_->buf) {
                            const int64_t U = d["U"], u = d["u"];
                            if (U <= p_->last_update_id + 1 &&
                                u >= p_->last_update_id + 1)
                            {
                                apply_delta(d);
                                p_->last_update_id = u;
                            } else {
                                // sequence break → trigger reconnect
                                std::terminate();
                            }
                        }
                        p_->buf.clear();
                    }).detach();
                }
                return;
            }

            // ----------------------------------------------------------------
            // 2. Normal streaming path (snapshot already applied)
            // ----------------------------------------------------------------
            const int64_t U = j["U"], u = j["u"];
            if (u < p_->last_update_id + 1)   // too old
                return;
            if (U > p_->last_update_id + 1) { // gap detected
                std::terminate();             // simplest bail-out: let caller restart
            }
            apply_delta(j);
            p_->last_update_id = u;
        });

        ws.start();
        ws.run();              // blocking
        return;
    }
#endif
    // -------- offline gz replay (always available) --------------------------
    gzFile gz = gzopen(p_->src.c_str(), "rb");
    if (!gz) throw std::runtime_error("cannot open " + p_->src);
    std::string buf(8 * 1024, '\0');
    while (gzgets(gz, buf.data(), static_cast<int>(buf.size())) != Z_NULL) {
        const auto j = nlohmann::json::parse(buf, nullptr, false);
        if (!j.is_object()) continue;
        std::vector<int64_t> bp,bq,ap,aq;
        // Price/qty arrive as strings → convert to 1e-4 ticks
        auto parse_side = [](const auto& js,
                             std::vector<int64_t>& p,
                             std::vector<int64_t>& q) {
            p.clear(); q.clear();
            for (auto& lv : js) {
                double px  = std::stod(lv[0].template get<std::string>());
                double qty = std::stod(lv[1].template get<std::string>());
                p.push_back(static_cast<int64_t>(px  * 1e4));
                q.push_back(static_cast<int64_t>(qty * 1e8)); // satoshis
            }
        };
        parse_side(j["b"], bp, bq);
        parse_side(j["a"], ap, aq);
        p_->cb(bp,bq,ap,aq);
    }
    gzclose(gz);
}

// ---------------------------------------------------------------------------
// helper: apply one depth delta JSON to the user callback
// ---------------------------------------------------------------------------
#if FF_HAS_IXWS
void BinanceFeed::apply_delta(const nlohmann::json& j)
{
    auto parse_side = [](const auto& js,
                         std::vector<int64_t>& p,
                         std::vector<int64_t>& q) {
        p.clear(); q.clear();
        for (auto& lv : js) {
            double px  = std::stod(lv[0].get<std::string>());
            double qty = std::stod(lv[1].get<std::string>());
            p.push_back(static_cast<int64_t>(px  * 1e4));
            q.push_back(static_cast<int64_t>(qty * 1e8)); // satoshis
        }
    };

    std::vector<int64_t> bp,bq,ap,aq;
    parse_side(j["b"], bp, bq);
    parse_side(j["a"], ap, aq);
    p_->cb(bp,bq,ap,aq);
}
#endif

BinanceFeed::~BinanceFeed() = default;

} // namespace ff 