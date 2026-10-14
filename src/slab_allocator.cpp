#include "slab_allocator.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <new>
#include <unordered_map>
#include <vector>

#include "frag_metrics.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace slabcache {

static constexpr std::size_t PAGE_SIZE = 4096;

SizeClass size_to_class(std::size_t size) {
    if (size <= static_cast<std::size_t>(SizeClass::Bytes64)) return SizeClass::Bytes64;
    if (size <= static_cast<std::size_t>(SizeClass::Bytes128)) return SizeClass::Bytes128;
    if (size <= static_cast<std::size_t>(SizeClass::Bytes256)) return SizeClass::Bytes256;
    if (size <= static_cast<std::size_t>(SizeClass::Bytes512)) return SizeClass::Bytes512;
    assert(false && "slab_alloc called for size > 512 bytes");
    return SizeClass::Bytes512;
}

// Global page directory mapping page base address to SizeClass.
// Meyers heap singleton guarantees it outlives all static destructors.
static std::mutex& get_page_dir_mutex() {
    static std::mutex mtx;
    return mtx;
}

static std::unordered_map<std::uintptr_t, SizeClass>& get_page_directory() {
    static auto* map = new std::unordered_map<std::uintptr_t, SizeClass>();
    return *map;
}

struct SlabPool {
    std::size_t objSize;
    std::size_t objsPerPage;
    void* freeList = nullptr;
    std::vector<void*> pages;
    std::mutex poolMutex;

    explicit SlabPool(std::size_t size) : objSize(size), objsPerPage(PAGE_SIZE / size) {}

    ~SlabPool() = default;

    void add_page() {
        void* page = nullptr;
#ifdef _WIN32
        page = VirtualAlloc(nullptr, PAGE_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (!page) throw std::bad_alloc();
#else
        page = mmap(nullptr, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (page == MAP_FAILED) throw std::bad_alloc();
#endif
        pages.push_back(page);

        // Register page in page directory.
        std::uintptr_t page_base = reinterpret_cast<std::uintptr_t>(page);
        {
            std::lock_guard<std::mutex> lock(get_page_dir_mutex());
            get_page_directory()[page_base] = static_cast<SizeClass>(objSize);
        }

        record_page_added(PAGE_SIZE);

        char* base = static_cast<char*>(page);
        for (std::size_t i = 0; i < objsPerPage; ++i) {
            void* obj = base + i * objSize;
            *reinterpret_cast<void**>(obj) = freeList;
            freeList = obj;
        }
    }

    void* allocate() {
        std::lock_guard<std::mutex> lock(poolMutex);
        if (!freeList) {
            add_page();
        }
        void* obj = freeList;
        freeList = *reinterpret_cast<void**>(obj);
        return obj;
    }

    void deallocate(void* ptr) {
        std::lock_guard<std::mutex> lock(poolMutex);
        *reinterpret_cast<void**>(ptr) = freeList;
        freeList = ptr;
    }

    void teardown() {
        std::lock_guard<std::mutex> lock(poolMutex);
        for (void* page : pages) {
            std::uintptr_t page_base = reinterpret_cast<std::uintptr_t>(page);
            {
                std::lock_guard<std::mutex> dlock(get_page_dir_mutex());
                get_page_directory().erase(page_base);
            }
#ifdef _WIN32
            VirtualFree(page, 0, MEM_RELEASE);
#else
            munmap(page, PAGE_SIZE);
#endif
        }
        pages.clear();
        freeList = nullptr;
    }
};

static SlabPool& get_pool(SizeClass sc) {
    static SlabPool pool64(static_cast<std::size_t>(SizeClass::Bytes64));
    static SlabPool pool128(static_cast<std::size_t>(SizeClass::Bytes128));
    static SlabPool pool256(static_cast<std::size_t>(SizeClass::Bytes256));
    static SlabPool pool512(static_cast<std::size_t>(SizeClass::Bytes512));
    switch (sc) {
        case SizeClass::Bytes64:
            return pool64;
        case SizeClass::Bytes128:
            return pool128;
        case SizeClass::Bytes256:
            return pool256;
        case SizeClass::Bytes512:
            return pool512;
    }
    return pool64;
}

bool slab_owns(void* ptr) {
    if (!ptr) return false;
    std::uintptr_t page_base = reinterpret_cast<std::uintptr_t>(ptr) & ~(PAGE_SIZE - 1);
    std::lock_guard<std::mutex> lock(get_page_dir_mutex());
    auto& dir = get_page_directory();
    return dir.find(page_base) != dir.end();
}

std::size_t slab_ptr_to_index(void* ptr) {
    if (!ptr) return 0;
    std::uintptr_t page_base = reinterpret_cast<std::uintptr_t>(ptr) & ~(PAGE_SIZE - 1);
    SizeClass sc = SizeClass::Bytes64;
    {
        std::lock_guard<std::mutex> lock(get_page_dir_mutex());
        auto& dir = get_page_directory();
        auto it = dir.find(page_base);
        if (it != dir.end()) {
            sc = it->second;
        }
    }
    switch (sc) {
        case SizeClass::Bytes64:
            return 0;
        case SizeClass::Bytes128:
            return 1;
        case SizeClass::Bytes256:
            return 2;
        case SizeClass::Bytes512:
            return 3;
    }
    return 0;
}

void* slab_alloc(std::size_t size) {
    SizeClass sc = size_to_class(size);
    SlabPool& pool = get_pool(sc);
    void* obj = pool.allocate();
    record_allocation(size, pool.objSize);
    return obj;
}

void slab_free(void* ptr) {
    if (!ptr) return;
    std::uintptr_t page_base = reinterpret_cast<std::uintptr_t>(ptr) & ~(PAGE_SIZE - 1);
    SizeClass sc;
    {
        std::lock_guard<std::mutex> lock(get_page_dir_mutex());
        auto& dir = get_page_directory();
        auto it = dir.find(page_base);
        if (it == dir.end()) {
            assert(false && "slab_free called with pointer not owned by any slab pool");
            return;
        }
        sc = it->second;
    }
    SlabPool& pool = get_pool(sc);
    pool.deallocate(ptr);
    record_free(pool.objSize);
}

void slab_teardown() {
    get_pool(SizeClass::Bytes64).teardown();
    get_pool(SizeClass::Bytes128).teardown();
    get_pool(SizeClass::Bytes256).teardown();
    get_pool(SizeClass::Bytes512).teardown();
}

}  // namespace slabcache
