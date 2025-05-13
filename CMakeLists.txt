cmake_minimum_required(VERSION 3.19)
project(flashflow LANGUAGES CXX)

option(FLASHFLOW_AVX2 "Enable AVX2 scan" OFF)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ---- compile flags ---------------------------------------------------------
if(APPLE)
    add_compile_options(-mcpu=apple-m1)
elseif(FLASHFLOW_AVX2)
    add_compile_options(-mavx2 -march=x86-64-v3)      # Sapphire Rapids ⇒ AVX-512 at runtime
endif()

add_library(flashflow_core INTERFACE)
target_include_directories(flashflow_core INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/src)

# nlohmann/json.hpp (header-only).  Home-brew uses /opt/homebrew/include.
find_path(NLOHMANN_JSON_INCLUDE_DIR nlohmann/json.hpp)
if(NLOHMANN_JSON_INCLUDE_DIR)
    message(STATUS "Found nlohmann/json: ${NLOHMANN_JSON_INCLUDE_DIR}")
    target_include_directories(flashflow_core INTERFACE ${NLOHMANN_JSON_INCLUDE_DIR})
else()
    message(FATAL_ERROR "nlohmann/json.hpp not found. Install with `brew install nlohmann-json`")
endif()

# ---------- Google-Benchmark ------------------------------------------------
find_package(benchmark REQUIRED)

# 1) Only object files – no main(), no linker problems
#    (need BinanceFeed code + zlib in the benchmark)
add_library(flashflow_bench OBJECT
    bench/benchmark_orderbook.cpp
    src/binance_feed.cpp)
target_link_libraries(flashflow_bench PRIVATE flashflow_core benchmark::benchmark z)

#  ixwebsocket present?  Propagate the same macro used for ff_stream.
if(ixwebsocket_FOUND)
    target_compile_definitions(flashflow_bench PRIVATE FF_HAS_IXWS=1)
else()
    target_compile_definitions(flashflow_bench PRIVATE FF_HAS_IXWS=0)
endif()

# ---------- CLIs ------------------------------------------------------------
# Provide the single main() and pull in the objects above
add_executable(ff_bench
    apps/ff_bench.cpp
    $<TARGET_OBJECTS:flashflow_bench>)
target_link_libraries(ff_bench PRIVATE flashflow_core benchmark::benchmark z)

# ------------------------------------------------------------------
# ff_stream – websockets optional
find_package(ixwebsocket QUIET)      # <— silently look for it

add_executable(ff_stream apps/ff_stream.cpp src/binance_feed.cpp)
target_link_libraries(ff_stream PRIVATE flashflow_core z)

if(ixwebsocket_FOUND)
    target_compile_definitions(ff_stream PRIVATE FF_HAS_IXWS=1)
    target_link_libraries(ff_stream PRIVATE ixwebsocket)
else()
    target_compile_definitions(ff_stream PRIVATE FF_HAS_IXWS=0)
endif()

# -----------------------------------------------------  enable tests
include(CTest)
if(BUILD_TESTING)
    # --- Catch2 (header-only, v3) ----------------------
    include(FetchContent)
    FetchContent_Declare(
        catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.4.0)
    FetchContent_MakeAvailable(catch2)

    add_executable(flashflow_tests
        tests/test_orderbook.cpp
        tests/test_feed_stub.cpp
        tests/test_binance_live.cpp
        tests/test_binance_replay.cpp
        src/binance_feed.cpp
        )      # offline stub – no websocket needed

    target_link_libraries(flashflow_tests PRIVATE
        flashflow_core
        Catch2::Catch2WithMain
        z)

    # ------------------------------------------------------------
    # Expose every Catch2 TEST_CASE to CTest
    include(Catch)                  # provided by Catch2 v3
    catch_discover_tests(flashflow_tests)

    # propagate websocket toggle so the compile matches ff_stream
    if(ixwebsocket_FOUND)
        target_compile_definitions(flashflow_tests PRIVATE FF_HAS_IXWS=1)
        target_link_libraries(flashflow_tests PRIVATE ixwebsocket)
    else()
        target_compile_definitions(flashflow_tests PRIVATE FF_HAS_IXWS=0)
    endif()

    # old single-wrapper test (remove to avoid duplicates)
    # add_test(NAME flashflow_unit  COMMAND flashflow_tests)
endif() 