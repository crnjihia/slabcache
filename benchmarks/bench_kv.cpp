#include <benchmark/benchmark.h>

#include <string>

#include "resp3.hpp"

static std::string make_set_cmd(const std::string& key, const std::string& val) {
    return "*3\r\n$3\r\nSET\r\n$" + std::to_string(key.size()) + "\r\n" + key + "\r\n$" +
           std::to_string(val.size()) + "\r\n" + val + "\r\n";
}

static std::string make_get_cmd(const std::string& key) {
    return "*2\r\n$3\r\nGET\r\n$" + std::to_string(key.size()) + "\r\n" + key + "\r\n";
}

static void BM_KVSetGet(benchmark::State& state) {
    int i = 0;
    for (auto _ : state) {
        std::string key = "sess:" + std::to_string(i % 10000);
        std::string val = "state_ussd_menu_active_" + std::to_string(i);

        std::string set_cmd = make_set_cmd(key, val);
        std::string set_resp = slabcache::handle_request(set_cmd);
        benchmark::DoNotOptimize(set_resp);

        std::string get_cmd = make_get_cmd(key);
        std::string get_resp = slabcache::handle_request(get_cmd);
        benchmark::DoNotOptimize(get_resp);

        ++i;
    }
}
BENCHMARK(BM_KVSetGet)->Threads(1)->Threads(4);

BENCHMARK_MAIN();
