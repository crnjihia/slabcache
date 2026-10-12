#include <benchmark/benchmark.h>
#include "slab_allocator.hpp"
#include "frag_metrics.hpp"

static void BM_SlabAlloc64(benchmark::State& state) {
    for (auto _ : state) {
        void* p = slab_alloc(64);
        benchmark::DoNotOptimize(p);
        slab_free(p);
    }
}
static void BM_SlabAlloc128(benchmark::State& state) {
    for (auto _ : state) {
        void* p = slab_alloc(128);
        benchmark::DoNotOptimize(p);
        slab_free(p);
    }
}
static void BM_SlabAlloc256(benchmark::State& state) {
    for (auto _ : state) {
        void* p = slab_alloc(256);
        benchmark::DoNotOptimize(p);
        slab_free(p);
    }
}
static void BM_SlabAlloc512(benchmark::State& state) {
    for (auto _ : state) {
        void* p = slab_alloc(512);
        benchmark::DoNotOptimize(p);
        slab_free(p);
    }
}

BENCHMARK(BM_SlabAlloc64);
BENCHMARK(BM_SlabAlloc128);
BENCHMARK(BM_SlabAlloc256);
BENCHMARK(BM_SlabAlloc512);

BENCHMARK_MAIN();
