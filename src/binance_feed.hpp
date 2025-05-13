#pragma once
#include <string>
#include <functional>
#include <memory>
#include <vector>
#include <cstdint>     // <— for int64_t in the new helper declaration
#include <nlohmann/json_fwd.hpp>   // provides  namespace nlohmann { class json; }

namespace ff {

class BinanceFeed {
public:
    using UpdateCB = std::function<void(const std::vector<int64_t>& bids_px,
                                        const std::vector<int64_t>& bids_qty,
                                        const std::vector<int64_t>& asks_px,
                                        const std::vector<int64_t>& asks_qty)>;

    BinanceFeed(UpdateCB cb, bool replay, const std::string& file_or_url);
    ~BinanceFeed();
    void run();          // blocking
#if FF_HAS_IXWS
    void apply_delta(const nlohmann::json&);   // helper for live path
#endif
private:
    struct Impl;         // PImpl hides heavy deps
    std::unique_ptr<Impl> p_;
};

} // namespace ff 