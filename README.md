# flashflow

_Colo-grade crypto-market gateway & matching-engine sandbox._  
Built from scratch for sub-µs determinism on modern x86 hardware (Intel
Sapphire Rapids, AMD Genoa) with AVX-512 / AVX2 SIMD throughout.

---

## Vision

1. **Wire-to-wire < 3 µs @ 64-byte packets** inside a Chicago or
   LD4/FR2 colo rack—NIC timestamp to user-land order-book delta.
2. **Deterministic back-testing**—replay every market data packet and
   every IOC/FOK you ever sent. Zero drift between live and sim paths.
3. **Pluggable gateways**—Binance, Coinbase, Deribit live today;
   CME MDP3 (FIX/FAST) and BitMEX multicast up next.
4. **One binary, multiple personalities**—engine, simulator, latency
   harness, even Wireshark dissector, all sharing the same decode path.
5. **Everything hot stays in L1**—a compact 8-byte _Level_  
   (32-bit price tick, 32-bit qty) lets us track up to **depth-500 per side**
   (≈ 16 KiB bids + 16 KiB asks) and still fit inside the 32 KiB L1-D cache
   of Sapphire Rapids. Depth-1000 remains available for research, at the
   cost of spilling into L2.

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

Numbers measured on 2.0 GHz Sapphire Rapids ES, kernel 6.8-rc, `turbo=0`.

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

All command blocks comply with `.cursorrules`; you can run them with one
click in Cursor.

---

## Testing & latency harness

* **Unit / integration tests** (Catch2 + CTest)  
  ```bash
  rm -rf build && cmake -S . -B build -DBUILD_TESTING=ON > /dev/null &&
  cmake --build build --parallel 8 -- -s > /dev/null &&
  ctest --test-dir build
  ```

* **Google Benchmark** micro-benchmarks (`/bench`) stream results straight
  into InfluxDB; dashboards live in `/tools/chronograf`.

---

## Roadmap

* CME Group MDP3 multicast decoder (FIX/FAST, half-bandwidth SIMD FAST)
* eBPF kernel probes for IRQ-to-user latency heat-maps
* CUDA-accelerated basket VWAP & TWAP
* On-NIC book building (P4 switch/Nvidia DOCA experiment)

---

## Contributing

Low-latency patches welcome—guidelines:

1. `clang-format` (LLVM) before committing.  
2. Functions ≤ 40 lines (hot path ≤ 15).  
3. Uphold the latency budget; include `bench/` proof if you improve it.  

---

## License

MIT.  Use it in prod, quote it in interviews—just drop us a ⭐ if it
shaves microseconds off your stack. 