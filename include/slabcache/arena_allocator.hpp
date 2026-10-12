#pragma once

#include <cstddef>

namespace slabcache {

/**
 * @brief Simple arena (bump‑pointer) allocator.
 *
 * The arena is backed by a contiguous memory region (typically mmap'd).
 * Allocation is O(1); reset frees all memory in O(1) as well.
 *
 * This header defines a minimal C‑style API; the implementation will be
 * provided later.
 */

/// Initialise the global arena with a given capacity (bytes).
/// Subsequent calls to arena_alloc will carve memory from this region.
/// Must be called before any arena_alloc; calling twice without arena_reset
/// is undefined behaviour.
void arena_init(std::size_t capacity);

/// Reset the arena, discarding all previous allocations.
void arena_reset();

/// Allocate `size` bytes from the arena. Returns nullptr on out‑of‑memory.
void* arena_alloc(std::size_t size);

/// Release any resources owned by the arena (e.g. unmap memory).
void arena_destroy();

} // namespace slabcache
