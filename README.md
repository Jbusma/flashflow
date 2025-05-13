# flashflow

_Colo-grade crypto-market gateway & matching-engine sandbox._  
Sub-µs deterministic C++ targeting modern x86 (Intel Sapphire Rapids / AMD Genoa, AVX-512 & AVX2).

---

## Vision

1. **Wire-to-wire < 3 µs @ 64-byte packets** – NIC timestamp to user-land
   order-book delta in a Chicago or LD4/FR2 colo rack.  
2. **Deterministic back-testing** – replay every market-data packet and
   every IOC/FOK you ever sent with zero drift.  
3. **Pluggable gateways** – Binance **and** Coinbase live today; Deribit,
   CME MDP3 (FIX/FAST) and BitMEX multicast are on the roadmap.  
4. **Zero code-path divergence** – the same parser & book-update logic
   drives live trading, back-testing and latency benchmarks; no
   “test-only” branches to drift out of sync.  
5. **Everything hot stays in L1** – a compact 8-byte _Level_ (32-bit price
   tick, 32-bit qty) keeps **depth-500 per side** (≈16 KiB bids + 16 KiB
   asks) within the 32 KiB L1-D of Sapphire Rapids; depth-1000 is still
   available for research (L2 hits).

---

## Architectural highlights

| Layer | What we do | Why it matters in colo |
|-------|------------|------------------------|
| Driver ↔ userspace | **io_uring + multishot** `recvmsg` pinned to 2 MiB hugepages | Reduces syscall count by ~30 ns/packet vs. epoll |
| Wire timestamping  | **SO_TIMESTAMPING** / HWTSTAMP-RAW-HW | No PCIe walk-back; nanosecond jitter histogram per symbol |
| Parsing            | **SIMD JSON** (AVX-512BW, AVX2 fallback) | 750 MiB/s single-core throughput on Sapphire Rapids |
| Aggregation        | **Compact 8-byte levels + SoA layout** | Depth-500/side fits in L1 → < 10 ns best-bid lookup |
| Persistence        | **Write-combining ring** → ZSTD → S3 | Sustain 4 Mpkt/s capture without stalling hot path |
| Back-tester        | Same structs, but driven by pcap / gz files | Bit-exact P&L attribution, ±1 tick |

---

## Hardware assumptions

* Sapphire Rapids (AVX-512, PCIe 5.0) or 4th-gen EPYC (“Genoa”)
* ConnectX-6 Dx or Intel 800-Series NIC (Rx hardware timestamping)
* Core isolation + `isolcpus`, `nohz_full`, `rcu_nocbs` boot params
* Jumbo disabled—64/128-byte packets stress the fast path

> ⚠️  The code will _compile_ on any x86-64, but you won't hit advertised
> jitter numbers without AVX2 and a NIC that supports RAW-HW stamps.

---

## Latency budget (target, one-way)

| Stage | Time (ns) |
|-------|-----------|
| NIC ↔ ring DMA            |    ~350 |
| `io_uring` dequeue        |     80 |
| SIMD JSON decode (depth20)|    130 |
| Order-book apply          |     45 |
| User callback / strategy  |    <50 |
| **Total**                 | **≈ 655 ns** |

---

## Quick start

~~~bash
# deps: g++-14 (AVX2+), CMake 3.24+, vcpkg (or Conan), zstd, zlib
git clone https://github.com/Jbusma/flashflow.git
cd flashflow && ./bootstrap.sh                # fetches submodules & vendor libs

# build a Release binary with AVX-512 if the CPU flag is present
cmake -B build -DCMAKE_BUILD_TYPE=Release -DFLASHFLOW_AVX2=ON
cmake --build build -j$(nproc)

# connect to Binance depth-20 and print latency stats to stdout
./build/bin/flashflow --config etc/binance_demo.yml
~~~



---

## Micro-benchmarks

These tests focus purely on nanosecond latency, not correctness.

```bash
cmake -B build-bench -DCMAKE_BUILD_TYPE=Release -DFLASHFLOW_BUILD_BENCH=ON &&
cmake --build build-bench --parallel 8 -- -s &&
./build-bench/apps/ff_bench
```

Run with `--benchmark_format=json` to pipe results into InfluxDB /
Chronograf dashboards and catch performance regressions in CI.

---

## Testing & latency harness

* **Unit / integration tests** (Catch2 + CTest)  
  ```bash
  rm -rf build && cmake -S . -B build -DBUILD_TESTING=ON > /dev/null &&
  cmake --build build --parallel 8 -- -s > /dev/null &&
  ctest --test-dir build
  ```

---

## Roadmap

* CME Group MDP3 multicast decoder (FIX/FAST, half-bandwidth SIMD FAST)
* eBPF kernel probes for IRQ-to-user latency heat-maps
* CUDA-accelerated basket VWAP & TWAP
* On-NIC book building (P4 switch/Nvidia DOCA experiment)

---

## License

MIT.  Use it in prod, quote it in interviews—just drop us a ⭐ if it
shaves microseconds off your stack. 