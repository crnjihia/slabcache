#pragma once

#include <cstddef>

namespace slabcache {

/**
 * @brief Fragmentation metrics snapshot.
 */
struct Metrics {
    double external_frag_percent = 0.0; // % of total capacity that is free (external)
    double internal_frag_percent = 0.0; // % of allocated bytes that are wasted due to rounding
};

/**
 * @brief Record the allocation of an object.
 *
 * @param request_size Size requested by the caller.
 * @param actual_size  Size actually provided by the allocator (including any padding).
 */
void record_allocation(std::size_t request_size, std::size_t actual_size);

/**
 * @brief Record the freeing of an object.
 *
 * @param actual_size Size of the object being freed (the actual slab size).
 */
void record_free(std::size_t actual_size);

/**
 * @brief Record that a new memory page has been added to the slab pools.
 *
 * @param page_size Size of the page (typically system page size).
 */
void record_page_added(std::size_t page_size);

/**
 * @brief Start the background sampler that updates metrics every 100 ms.
 */
void start_metrics_sampler();

/**
 * @brief Retrieve the most recent metrics snapshot.
 */
Metrics get_latest_metrics();

} // namespace slabcache
