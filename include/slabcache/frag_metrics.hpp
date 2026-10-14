#pragma once

#include <cstddef>

namespace slabcache {

/**
 * @brief Fragmentation metrics snapshot.
 */
struct Metrics {
    double external_frag_percent = 0.0;  // % of total capacity that is free (external)
    double internal_frag_percent = 0.0;  // % of allocated bytes that are wasted due to rounding
};

void record_allocation(std::size_t request_size, std::size_t actual_size);
void record_free(std::size_t actual_size);
void record_page_added(std::size_t page_size);

void start_metrics_sampler();
void stop_metrics_sampler();
Metrics get_latest_metrics();

}  // namespace slabcache
