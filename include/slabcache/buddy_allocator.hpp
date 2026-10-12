#pragma once

#include <cstddef>

namespace slabcache {

/**
 * @brief Allocate a block of at least `size` bytes using a buddy system.
 *
 * The allocator handles sizes > 512 bytes, rounding up to the next power‑of‑2.
 *
 * @param size Minimum number of bytes requested.
 * @return Pointer to a usable region, or nullptr on failure.
 *
 * @note Stub implementation – aborts if called.
 */
void* buddy_alloc(std::size_t size);

/**
 * @brief Free a block previously allocated with buddy_alloc.
 *
 * @param ptr Pointer returned by buddy_alloc.
 *
 * @note Stub implementation – aborts if called.
 */
void buddy_free(void* ptr);

} // namespace slabcache
