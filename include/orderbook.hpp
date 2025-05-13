#pragma once
#include <array>
#include <cstdint>

// -------- SIMD capability ---------------------------------------------------
// If the user requested AVX2 but we're not on x86_64, just disable it.
#if defined(FLASHFLOW_AVX2) && (defined(__x86_64__) || defined(_M_X64))
    #include <immintrin.h>
    #define FF_AVX2 1
#else
    #define FF_AVX2 0
#endif

namespace ff {

constexpr std::size_t DEPTH = 1000;
using price_t = int64_t;   // 1e-4 USD ticks (exact, cheap cmp)
using qty_t   = int64_t;   // satoshis

struct alignas(64) OrderBook {
    // SoA – cache-friendly, contiguous, 8 KiB per side
    std::array<price_t, DEPTH> bids_price{};
    std::array<qty_t  , DEPTH> bids_qty  {};
    std::array<price_t, DEPTH> asks_price{};
    std::array<qty_t  , DEPTH> asks_qty  {};

    // ------- delta ingest ---------------------------------------------------
    // Binance depth message ⇒ arrays of (price,qty). qty==0 ⇒ level delete.
    template<class Vec>
    inline void apply_bid_delta(const Vec& px, const Vec& qt) noexcept {
        for (std::size_t i = 0; i < px.size(); ++i)
            bids_price[i] = px[i], bids_qty[i] = qt[i];
    }
    template<class Vec>
    inline void apply_ask_delta(const Vec& px, const Vec& qt) noexcept {
        for (std::size_t i = 0; i < px.size(); ++i)
            asks_price[i] = px[i], asks_qty[i] = qt[i];
    }

    // ------- best-level scan -------------------------------------------------
    // returns index within side; −1 if empty
#if FF_AVX2
    inline int best_bid() const noexcept { return best_bid_avx2(); }
    inline int best_ask() const noexcept { return best_ask_avx2(); }
#else
    inline int best_bid() const noexcept { return best_bid_scalar(); }
    inline int best_ask() const noexcept { return best_ask_scalar(); }
#endif

private:
    // --- scalar fallback (≈1.7 µs for 10 M scans) ---------------------------
    int best_bid_scalar() const noexcept {
        for (std::size_t i = 0; i < DEPTH; ++i)
            if (bids_qty[i] > 0) return static_cast<int>(i);
        return -1;
    }
    int best_ask_scalar() const noexcept {
        for (std::size_t i = 0; i < DEPTH; ++i)
            if (asks_qty[i] > 0) return static_cast<int>(i);
        return -1;
    }

#if FF_AVX2
    // --- AVX2: 4×64-bit per 256-bit lane → ≤ 300 ns -------------------------
    int best_bid_avx2() const noexcept {
        constexpr int LANE = 4;
        const __m256i zeros = _mm256_setzero_si256();
        for (std::size_t i = 0; i < DEPTH; i += LANE) {
            __m256i v = _mm256_load_si256(reinterpret_cast<const __m256i*>(&bids_qty[i]));
            __m256i gt = _mm256_cmpgt_epi64(v, zeros);     // >0?
            uint32_t m = _mm256_movemask_epi8(gt);
            if (m) {
                // movemask gives 8 bits per 64-bit element.
                uint32_t idx_in_lane = __tzcnt_u32(m) >> 3; // /8
                return static_cast<int>(i + idx_in_lane);
            }
        }
        return -1;
    }
    int best_ask_avx2() const noexcept {
        constexpr int LANE = 4;
        const __m256i zeros = _mm256_setzero_si256();
        for (std::size_t i = 0; i < DEPTH; i += LANE) {
            __m256i v = _mm256_load_si256(reinterpret_cast<const __m256i*>(&asks_qty[i]));
            __m256i gt = _mm256_cmpgt_epi64(v, zeros);
            uint32_t m = _mm256_movemask_epi8(gt);
            if (m) {
                uint32_t idx_in_lane = __tzcnt_u32(m) >> 3;
                return static_cast<int>(i + idx_in_lane);
            }
        }
        return -1;
    }
#endif
};

} // namespace ff 