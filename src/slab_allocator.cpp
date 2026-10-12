#include "slab_allocator.hpp"
#include "frag_metrics.hpp"

#include <cassert>
#include <cstddef>
#include <vector>

#ifdef _WIN32
#   include <windows.h>
#endif

namespace slabcache {

// Page size – use system page size (4KiB typical). For simplicity we fix 4096.
static constexpr std::size_t PAGE_SIZE = 4096;

// Helper to map requested size to a SizeClass enum.
static SizeClass size_to_class(std::size_t size) {
    if (size <= static_cast<std::size_t>(SizeClass::Bytes64))  return SizeClass::Bytes64;
    if (size <= static_cast<std::size_t>(SizeClass::Bytes128)) return SizeClass::Bytes128;
    if (size <= static_cast<std::size_t>(SizeClass::Bytes256)) return SizeClass::Bytes256;
    if (size <= static_cast<std::size_t>(SizeClass::Bytes512)) return SizeClass::Bytes512;
    // For sizes > 512 we deliberately abort – handled by buddy allocator.
    assert(false && "slab_alloc called for size > 512 bytes");
    return SizeClass::Bytes512; // unreachable
}

// Simple struct representing a slab pool for one size class.
struct SlabPool {
    std::size_t objSize;          // Size of each object in this pool.
    std::size_t objsPerPage;      // How many objects fit in a page.
    void* freeList = nullptr;     // Intrusive singly‑linked free list (head).
    std::vector<void*> pages;      // Keep allocated pages for eventual cleanup.

    explicit SlabPool(std::size_t size)
        : objSize(size)
        , objsPerPage(PAGE_SIZE / size)
    {}

    // Allocate a new page and populate the free list.
    void add_page() {
        // Platform‑specific page allocation.
        void* page = nullptr;
    #ifdef _WIN32
        page = VirtualAlloc(nullptr, PAGE_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (!page) { throw std::bad_alloc(); }
    #else
        page = std::aligned_alloc(PAGE_SIZE, PAGE_SIZE);
        if (!page) { throw std::bad_alloc(); }
    #endif
        pages.push_back(page);
        // Notify metrics about a new page.
        record_page_added(PAGE_SIZE);
        // Slice the page into objects and push each onto freeList.
        char* base = static_cast<char*>(page);
        for (std::size_t i = 0; i < objsPerPage; ++i) {
            void* obj = base + i * objSize;
            // Store the current freeList head inside the object.
            *reinterpret_cast<void**>(obj) = freeList;
            freeList = obj;
        }
    }
};

// One static pool per size class, lazily initialized.
static SlabPool& get_pool(SizeClass sc) {
    static SlabPool pool64(static_cast<std::size_t>(SizeClass::Bytes64));
    static SlabPool pool128(static_cast<std::size_t>(SizeClass::Bytes128));
    static SlabPool pool256(static_cast<std::size_t>(SizeClass::Bytes256));
    static SlabPool pool512(static_cast<std::size_t>(SizeClass::Bytes512));
    switch (sc) {
        case SizeClass::Bytes64:  return pool64;
        case SizeClass::Bytes128: return pool128;
        case SizeClass::Bytes256: return pool256;
        case SizeClass::Bytes512: return pool512;
    }
    assert(false && "Invalid SizeClass");
    // Unreachable – silence compiler warnings.
    return pool64;
}

void* slab_alloc(std::size_t size) {
    SizeClass sc = size_to_class(size);
    SlabPool& pool = get_pool(sc);
    if (!pool.freeList) {
        // No free objects – allocate a fresh page.
        pool.add_page();
    }
    // Pop the head of the free list.
    void* obj = pool.freeList;
    pool.freeList = *reinterpret_cast<void**>(obj);
    // Record allocation for metrics.
    record_allocation(size, pool.objSize);
    return obj;
}

void slab_free(void* ptr) {
    if (!ptr) return; // no‑op for null.

    // Find the pool that owns this pointer by checking the page ranges.
    // The pools are static, so we can examine them directly.
    auto try_pool = [&](SlabPool& pool) -> bool {
        std::uintptr_t p = reinterpret_cast<std::uintptr_t>(ptr);
        for (void* page : pool.pages) {
            std::uintptr_t start = reinterpret_cast<std::uintptr_t>(page);
            if (p >= start && p < start + PAGE_SIZE) {
                // Push ptr onto this pool's free list.
                *reinterpret_cast<void**>(ptr) = pool.freeList;
                pool.freeList = ptr;
                // Record free for metrics.
                record_free(pool.objSize);
                return true;
            }
        }
        return false;
    };

    // Check pools from largest to smallest (any order works, but this is deterministic).
    if (try_pool(get_pool(SizeClass::Bytes512))) return;
    if (try_pool(get_pool(SizeClass::Bytes256))) return;
    if (try_pool(get_pool(SizeClass::Bytes128))) return;
    if (try_pool(get_pool(SizeClass::Bytes64))) return;

    // If we reach here the pointer does not belong to any slab pool – this is a programmer error.
    assert(false && "slab_free called with pointer not owned by any slab pool");
}

} // namespace slabcache

