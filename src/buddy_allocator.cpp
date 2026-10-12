#include "buddy_allocator.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

#ifdef _WIN32
#   include <windows.h>
#else
#   include <sys/mman.h>
#   include <unistd.h>
#endif

namespace slabcache {

// -----------------------------------------------------------------------------
// Buddy allocator implementation details
// -----------------------------------------------------------------------------

// Minimum order for buddy blocks: 2^10 = 1024 bytes (first size > 512).
static constexpr std::size_t MIN_ORDER = 10;
// Choose a maximum order that gives a reasonable total memory pool (64 MiB).
static constexpr std::size_t MAX_ORDER = 26; // 2^26 = 67,108,864 bytes (~64 MiB)
static constexpr std::size_t TOTAL_SIZE = std::size_t(1) << MAX_ORDER;

// Block header stored at the beginning of each buddy block.
struct BlockHeader {
    std::size_t order;   // Power‑of‑2 exponent (block size = 1 << order).
    bool allocated;      // true when in use, false when free.
    BlockHeader* next;   // Used only when free – singly linked list.
};

static void* buddy_base = nullptr;               // Start of the memory pool.
static BlockHeader* free_list[MAX_ORDER + 1] = {}; // One list per order.
static bool initialized = false;

// -----------------------------------------------------------------------------
// Helper utilities
// -----------------------------------------------------------------------------

// Round up a value to the next power of two and return its exponent.
static std::size_t order_of(std::size_t sz) {
    // Include space for the block header.
    sz += sizeof(BlockHeader);
    std::size_t order = MIN_ORDER;
    std::size_t block = std::size_t(1) << order;
    while (block < sz) {
        ++order;
        block <<= 1;
        // Guard against overflow – should never exceed MAX_ORDER.
        assert(order <= MAX_ORDER && "Requested size exceeds buddy allocator capacity");
    }
    return order;
}

// Initialise the global pool the first time we need it.
static void init_pool() {
    assert(!initialized && "Buddy pool already initialized");
#ifdef _WIN32
    buddy_base = VirtualAlloc(nullptr, TOTAL_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!buddy_base) {
        throw std::bad_alloc();
    }
#else
    buddy_base = mmap(nullptr, TOTAL_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (buddy_base == MAP_FAILED) {
        buddy_base = nullptr;
        throw std::bad_alloc();
    }
#endif
    // The whole region starts as a single free block of MAX_ORDER.
    BlockHeader* initial = static_cast<BlockHeader*>(buddy_base);
    initial->order = MAX_ORDER;
    initial->allocated = false;
    initial->next = nullptr;
    free_list[MAX_ORDER] = initial;
    initialized = true;
}

// Remove a block from its free list (single‑linked). Returns true if removed.
static bool remove_from_freelist(BlockHeader* block, std::size_t order) {
    BlockHeader* prev = nullptr;
    BlockHeader* cur = free_list[order];
    while (cur) {
        if (cur == block) {
            if (prev) {
                prev->next = cur->next;
            } else {
                free_list[order] = cur->next;
            }
            cur->next = nullptr;
            return true;
        }
        prev = cur;
        cur = cur->next;
    }
    return false; // not found – should not happen for a valid free block.
}

// Insert a block into the appropriate free list.
static void insert_into_freelist(BlockHeader* block) {
    block->allocated = false;
    block->next = free_list[block->order];
    free_list[block->order] = block;
}

// Compute the buddy address for a given block.
static BlockHeader* buddy_of(BlockHeader* block) {
    std::uintptr_t offset = reinterpret_cast<std::uintptr_t>(block) - reinterpret_cast<std::uintptr_t>(buddy_base);
    std::uintptr_t buddy_offset = offset ^ (std::size_t(1) << block->order);
    return reinterpret_cast<BlockHeader*>(reinterpret_cast<char*>(buddy_base) + buddy_offset);
}

// -----------------------------------------------------------------------------
// Public API implementation
// -----------------------------------------------------------------------------

void* buddy_alloc(std::size_t size) {
    // Lazily initialise the pool on first use.
    if (!initialized) {
        init_pool();
    }
    // Sizes <= 512 are not managed by the buddy allocator.
    assert(size > 512 && "buddy_alloc called for size <= 512 – use slab allocator");

    std::size_t needed_order = order_of(size);

    // Find the smallest free block that can satisfy the request.
    std::size_t cur_order = needed_order;
    while (cur_order <= MAX_ORDER && free_list[cur_order] == nullptr) {
        ++cur_order;
    }
    if (cur_order > MAX_ORDER) {
        // Out of memory.
        return nullptr;
    }

    // Remove a block from the free list at cur_order.
    BlockHeader* block = free_list[cur_order];
    free_list[cur_order] = block->next;
    block->next = nullptr;

    // Split until we reach the needed order.
    while (cur_order > needed_order) {
        --cur_order;
        // Compute the buddy address for the higher‑order block.
        BlockHeader* buddy = reinterpret_cast<BlockHeader*>(
            reinterpret_cast<char*>(block) + (std::size_t(1) << cur_order)
        );
        // Initialise the buddy as a free block of the lower order.
        buddy->order = cur_order;
        buddy->allocated = false;
        buddy->next = free_list[cur_order];
        free_list[cur_order] = buddy;
        // The original block now represents the lower order.
        block->order = cur_order;
    }

    // Mark the block as allocated and return the user pointer after the header.
    block->allocated = true;
    // No need to set next; it will be overwritten on free.
    return static_cast<void*>(reinterpret_cast<char*>(block) + sizeof(BlockHeader));
}

void buddy_free(void* ptr) {
    if (!ptr) return; // No‑op for null.
    // Lazily initialise if the pool was never used (should not happen).
    if (!initialized) {
        // If no allocation ever happened, nothing to free.
        return;
    }

    // Recover the block header.
    BlockHeader* block = reinterpret_cast<BlockHeader*>(reinterpret_cast<char*>(ptr) - sizeof(BlockHeader));
    // Double‑free detection.
    assert(block->allocated && "buddy_free called on a block that is already free (double free)");
    block->allocated = false;

    // Coalesce with buddy while possible.
    while (block->order < MAX_ORDER) {
        BlockHeader* buddy = buddy_of(block);
        // Buddy must be within the pool and free with the same order.
        if (!buddy->allocated && buddy->order == block->order) {
            // Remove buddy from its free list.
            bool removed = remove_from_freelist(buddy, buddy->order);
            assert(removed && "Buddy block expected to be in free list but was not");
            // Determine which address is lower – that becomes the combined block.
            block = (block < buddy) ? block : buddy;
            ++block->order; // merged to the next higher order
        } else {
            break; // cannot merge further.
        }
    }
    // Insert the (possibly merged) block back into the appropriate free list.
    insert_into_freelist(block);
}

} // namespace slabcache
