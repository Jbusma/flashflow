# flashflow

Ultra-low-latency crypto-market **gateway** and **back-testing sandbox** written in modern C++.

|            |                               |
|------------|-------------------------------|
| **Status** | ✅  All unit tests pass        |
| **License**| MIT                           |
| **Requires** | g++-14 (AVX2), CMake ≥ 3.24 |

---

## Why flashflow?

HFT desks care about code that

* moves packets fast  
* measures sub-µs jitter  
* fails gracefully  

`flashflow` shows all of that in one repo.

---

## Core components

| Module                 | Highlights |
|------------------------|------------|
| **Feed handler**       | Zero-copy Binance & Coinbase depth streams via pure **io_uring** WebSockets (multishot recv, huge-page buffers). Nanosecond packet stamps via **SO_TIMESTAMPING**. |
| **Matching-engine sim**| Deterministic event queue lets you replay the live tape against your own orders for exact P&L. |
| **Execution adapter**  | Async REST bulk-cancels with rate-limit back-off (ready for raw-TCP gateways). |
| **Latency harness**    | Google Benchmark + InfluxDB / Chronograf dashboards. |

---

## Quick start

~~~bash
# deps: g++-14, CMake 3.24+, vcpkg (or Conan)
git clone https://github.com/your-handle/flashflow.git
cd flashflow && ./bootstrap.sh          # pulls submodules & third-party libs

# Release build (AVX2)
cmake -B build -DCMAKE_BUILD_TYPE=Release -DFLASHFLOW_AVX2=ON
cmake --build build -j$(nproc)

# run a demo config (see /etc)
./build/bin/flashflow --config etc/binance_demo.yml
~~~

---

## Run the tests

Full silent build + suite:

~~~bash
rm -rf build &&
cmake -S . -B build -DBUILD_TESTING=ON > /dev/null &&
cmake --build build --parallel 8 -- -s > /dev/null &&
ctest --test-dir build
~~~

Binance-only subset:

~~~bash
rm -rf build &&
cmake -S . -B build -DBUILD_TESTING=ON > /dev/null &&
cmake --build build --parallel 8 -- -s > /dev/null &&
ctest --test-dir build &&
ctest --test-dir build -R binance
~~~

High-resolution per-test timings (bypass CTest):

~~~bash
build/flashflow_tests -r compact --durations yes
~~~

_All command blocks obey `.cursorrules` (commands joined with `&&`, comments stripped)._

---

## Repo layout 

```
+/engine        core event loop, matching sim
+/gateways      exchange adapters
+/bench         micro-benchmarks
+/tools         packet sniffers, log rotators
+/etc           sample YAML configs
```

---

## CPU requirements

* x86-64 with AVX2 (tested on Intel Sapphire Rapids & AMD Genoa)  
* Optional AVX-512 paths auto-enable when the `cpuid` flag is present.

---

## Roadmap

* CME Group MDP3 decoder (FIX/FAST)  
* eBPF-based latency harness  
* CUDA-accelerated VWAP calculation  
* Additional exchanges (Deribit, Kraken, KuCoin)

---

## Contributing

1. Run `clang-format` (LLVM style) before committing.  
2. Keep functions ≤ 40 lines where practical.  
3. Preserve sub-µs latency in hot paths.  

If **flashflow** helps you land that HFT role, please ⭐ the repo!

---

MIT License • © 2025 your-handle 