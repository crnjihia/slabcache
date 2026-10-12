#include <benchmark/benchmark.h>
#include "store.hpp"
#include "resp3.hpp"
#include <chrono>
#include <string>

// Simple helper to generate a SET command string.
static std::string make_set_cmd(const std::string& key, const std::string& val) {
    return "*3\r\n$3\r\nSET\r\n$" + std::to_string(key.size()) + "\r\n" + key + "\r\n$" + std::to_string(val.size()) + "\r\n" + val + "\r\n";
}

static void BM_KVSetGet(benchmark::State& state) {
    int i = 0;
    for (auto _ : state) {
        std::string key = "key" + std::to_string(i);
        std::string val = "val" + std::to_string(i);
        std::string set_cmd = make_set_cmd(key, val);
        // Execute SET via RESP3 handler.
        std::string set_resp = handle_request(set_cmd);
        benchmark::DoNotOptimize(set_resp);
        // GET
        std::string get_cmd = "*2\r\n$3\r\nGET\r\n$" + std::to_string(key.size()) + "\r\n" + key + "\r\n";
        std::string get_resp = handle_request(get_cmd);
        benchmark::DoNotOptimize(get_resp);
        ++i;
    }
}

BENCHMARK(BM_KVSetGet)->Threads(4);
BENCHMARK_MAIN();
