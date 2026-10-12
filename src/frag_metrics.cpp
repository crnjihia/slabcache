#include "frag_metrics.hpp"

#include <atomic>
#include <thread>
#include <chrono>

namespace slabcache {

struct MetricsInternal {
    std::atomic<std::size_t> total_capacity{0};            // bytes of pages allocated
    std::atomic<std::size_t> allocated_bytes{0};          // current allocated (including internal waste)
    std::atomic<std::size_t> total_requested{0};          // cumulative requested bytes (for internal frag)
    std::atomic<std::size_t> total_wasted_internal{0};   // cumulative internal waste bytes
    std::atomic<double> latest_external_percent{0.0};
    std::atomic<double> latest_internal_percent{0.0};
    std::atomic<bool> running{true};
    std::thread sampler_thread;
} metrics;

void record_page_added(std::size_t page_size) {
    metrics.total_capacity.fetch_add(page_size, std::memory_order_relaxed);
}

void record_allocation(std::size_t request_size, std::size_t actual_size) {
    metrics.total_requested.fetch_add(request_size, std::memory_order_relaxed);
    metrics.allocated_bytes.fetch_add(actual_size, std::memory_order_relaxed);
    if (actual_size > request_size) {
        metrics.total_wasted_internal.fetch_add(actual_size - request_size, std::memory_order_relaxed);
    }
}

void record_free(std::size_t actual_size) {
    // Decrease current allocated bytes; total_requested and wasted stay cumulative.
    metrics.allocated_bytes.fetch_sub(actual_size, std::memory_order_relaxed);
    // Note: we do not adjust total_wasted_internal here – it reflects total waste incurred.
}

static void sampler_loop() {
    using namespace std::chrono_literals;
    while (metrics.running.load(std::memory_order_relaxed)) {
        // Compute external fragmentation percent.
        std::size_t cap = metrics.total_capacity.load(std::memory_order_relaxed);
        std::size_t allocated = metrics.allocated_bytes.load(std::memory_order_relaxed);
        double external = 0.0;
        if (cap > 0) {
            std::size_t free_bytes = cap > allocated ? cap - allocated : 0;
            external = static_cast<double>(free_bytes) * 100.0 / static_cast<double>(cap);
        }
        // Compute internal fragmentation percent.
        std::size_t requested = metrics.total_requested.load(std::memory_order_relaxed);
        std::size_t wasted = metrics.total_wasted_internal.load(std::memory_order_relaxed);
        double internal = 0.0;
        if (requested > 0) {
            internal = static_cast<double>(wasted) * 100.0 / static_cast<double>(requested);
        }
        metrics.latest_external_percent.store(external, std::memory_order_relaxed);
        metrics.latest_internal_percent.store(internal, std::memory_order_relaxed);
        std::this_thread::sleep_for(100ms);
    }
}

void start_metrics_sampler() {
    // Only start once.
    static std::once_flag flag;
    std::call_once(flag, [](){
        metrics.sampler_thread = std::thread(sampler_loop);
        metrics.sampler_thread.detach(); // detach; will run for process lifetime
    });
}

Metrics get_latest_metrics() {
    Metrics m;
    m.external_frag_percent = metrics.latest_external_percent.load(std::memory_order_relaxed);
    m.internal_frag_percent = metrics.latest_internal_percent.load(std::memory_order_relaxed);
    return m;
}

} // namespace slabcache
