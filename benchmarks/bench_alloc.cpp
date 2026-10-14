#include <benchmark/benchmark.h>

#include <cstdlib>

#include "arena_allocator.hpp"
#include "buddy_allocator.hpp"
#include "slab_allocator.hpp"
#include "thread_cache.hpp"

#if defined(SLABCACHE_USE_JEMALLOC)
#include <jemalloc/jemalloc.h>
#endif

// -------------------------------------------------------------
// Slab Allocator Benchmarks
// -------------------------------------------------------------
static void BM_SlabAlloc64(benchmark::State& state) {
    for (auto _ : state) {
        void* p = slabcache::tc_alloc(64);
        benchmark::DoNotOptimize(p);
        slabcache::tc_free(p);
    }
}
BENCHMARK(BM_SlabAlloc64);

static void BM_SlabAlloc128(benchmark::State& state) {
    for (auto _ : state) {
        void* p = slabcache::tc_alloc(128);
        benchmark::DoNotOptimize(p);
        slabcache::tc_free(p);
    }
}
BENCHMARK(BM_SlabAlloc128);

static void BM_SlabAlloc256(benchmark::State& state) {
    for (auto _ : state) {
        void* p = slabcache::tc_alloc(256);
        benchmark::DoNotOptimize(p);
        slabcache::tc_free(p);
    }
}
BENCHMARK(BM_SlabAlloc256);

static void BM_SlabAlloc512(benchmark::State& state) {
    for (auto _ : state) {
        void* p = slabcache::tc_alloc(512);
        benchmark::DoNotOptimize(p);
        slabcache::tc_free(p);
    }
}
BENCHMARK(BM_SlabAlloc512);

// -------------------------------------------------------------
// Arena Allocator Benchmarks
// -------------------------------------------------------------
static void BM_ArenaAlloc64(benchmark::State& state) {
    slabcache::Arena arena(1024 * 1024);
    for (auto _ : state) {
        void* p = arena.alloc(64);
        benchmark::DoNotOptimize(p);
        if (arena.used() > 900 * 1024) {
            arena.reset();
        }
    }
}
BENCHMARK(BM_ArenaAlloc64);

static void BM_ArenaAlloc128(benchmark::State& state) {
    slabcache::Arena arena(1024 * 1024);
    for (auto _ : state) {
        void* p = arena.alloc(128);
        benchmark::DoNotOptimize(p);
        if (arena.used() > 900 * 1024) {
            arena.reset();
        }
    }
}
BENCHMARK(BM_ArenaAlloc128);

static void BM_ArenaAlloc256(benchmark::State& state) {
    slabcache::Arena arena(1024 * 1024);
    for (auto _ : state) {
        void* p = arena.alloc(256);
        benchmark::DoNotOptimize(p);
        if (arena.used() > 900 * 1024) {
            arena.reset();
        }
    }
}
BENCHMARK(BM_ArenaAlloc256);

static void BM_ArenaAlloc512(benchmark::State& state) {
    slabcache::Arena arena(1024 * 1024);
    for (auto _ : state) {
        void* p = arena.alloc(512);
        benchmark::DoNotOptimize(p);
        if (arena.used() > 900 * 1024) {
            arena.reset();
        }
    }
}
BENCHMARK(BM_ArenaAlloc512);

// -------------------------------------------------------------
// Buddy Allocator Benchmark
// -------------------------------------------------------------
static void BM_BuddyAlloc1024(benchmark::State& state) {
    for (auto _ : state) {
        void* p = slabcache::buddy_alloc(1024);
        benchmark::DoNotOptimize(p);
        slabcache::buddy_free(p);
    }
}
BENCHMARK(BM_BuddyAlloc1024);

// -------------------------------------------------------------
// Standard System Malloc Benchmarks
// -------------------------------------------------------------
static void BM_Malloc64(benchmark::State& state) {
    for (auto _ : state) {
        void* p = std::malloc(64);
        benchmark::DoNotOptimize(p);
        std::free(p);
    }
}
BENCHMARK(BM_Malloc64);

static void BM_Malloc128(benchmark::State& state) {
    for (auto _ : state) {
        void* p = std::malloc(128);
        benchmark::DoNotOptimize(p);
        std::free(p);
    }
}
BENCHMARK(BM_Malloc128);

static void BM_Malloc256(benchmark::State& state) {
    for (auto _ : state) {
        void* p = std::malloc(256);
        benchmark::DoNotOptimize(p);
        std::free(p);
    }
}
BENCHMARK(BM_Malloc256);

static void BM_Malloc512(benchmark::State& state) {
    for (auto _ : state) {
        void* p = std::malloc(512);
        benchmark::DoNotOptimize(p);
        std::free(p);
    }
}
BENCHMARK(BM_Malloc512);

BENCHMARK_MAIN();
