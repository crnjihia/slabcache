#include <cassert>
#include <chrono>
#include <cstdio>
#include <thread>

#include "frag_metrics.hpp"
#include "slab_allocator.hpp"

int main() {
    using namespace std::chrono_literals;

    slabcache::start_metrics_sampler();

    // Allocate a few objects
    void* a = slabcache::slab_alloc(64);
    void* b = slabcache::slab_alloc(128);
    void* c = slabcache::slab_alloc(256);

    slabcache::slab_free(b);

    std::this_thread::sleep_for(150ms);

    slabcache::Metrics m = slabcache::get_latest_metrics();
    assert(m.external_frag_percent >= 0.0 && m.external_frag_percent <= 100.0);
    assert(m.internal_frag_percent >= 0.0 && m.internal_frag_percent <= 100.0);

    slabcache::slab_free(a);
    slabcache::slab_free(c);

    slabcache::stop_metrics_sampler();

    std::printf("frag metrics test passed (external %.2f%%, internal %.2f%%)\n",
                m.external_frag_percent, m.internal_frag_percent);
    return 0;
}
