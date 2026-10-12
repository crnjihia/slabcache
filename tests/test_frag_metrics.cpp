#include "frag_metrics.hpp"
#include "slab_allocator.hpp"

#include <thread>
#include <chrono>
#include <assert>
#include <stdio>

int main() {
    // Ensure the background sampler is running.
    start_metrics_sampler();

    // Allocate a few objects of different sizes.
    void* a = slab_alloc(64);
    void* b = slab_alloc(128);
    void* c = slab_alloc(256);
    // Free the middle object to create a fragmentation pattern.
    slab_free(b);

    // Give the sampler time to collect a sample.
    std::this_thread::sleep_for(250ms);

    Metrics m = get_latest_metrics();
    // Basic sanity checks – percentages should be in [0,100].
    assert(m.external_frag_percent >= 0.0 && m.external_frag_percent <= 100.0);
    assert(m.internal_frag_percent >= 0.0 && m.internal_frag_percent <= 100.0);

    // Clean up remaining allocations.
    slab_free(a);
    slab_free(c);

    // Wait a bit more for another sample.
    std::this_thread::sleep_for(150ms);
    (void)get_latest_metrics();

    std::printf("frag metrics test passed (external %.2f%%, internal %.2f%%)\n",
                m.external_frag_percent, m.internal_frag_percent);
    return 0;
}
