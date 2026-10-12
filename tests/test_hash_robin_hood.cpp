#include "hash_table.hpp"
#include "slab_allocator.hpp"

#include <cassert>
#include <string_view>

int main() {
    // Ensure the sampler is running – not required for hash map but for completeness.
    // (No need to start metrics here.)

    slabcache::RobinHoodMap map(32);

    const char* key1 = "alpha";
    const char* key2 = "beta";
    const char* key3 = "gamma";
    const char* key4 = "delta";

    // Insert entries.
    bool inserted;
    inserted = map.insert(std::string_view(key1), (void*)0x1);
    assert(inserted);
    inserted = map.insert(std::string_view(key2), (void*)0x2);
    assert(inserted);
    inserted = map.insert(std::string_view(key3), (void*)0x3);
    assert(inserted);
    // Overwrite existing key.
    inserted = map.insert(std::string_view(key2), (void*)0x22);
    assert(!inserted); // not a new entry.

    // Find values.
    assert(map.find(std::string_view(key1)) == (void*)0x1);
    assert(map.find(std::string_view(key2)) == (void*)0x22);
    assert(map.find(std::string_view(key3)) == (void*)0x3);
    assert(map.find(std::string_view(key4)) == nullptr);

    // Erase a key.
    bool erased = map.erase(std::string_view(key2));
    assert(erased);
    assert(map.find(std::string_view(key2)) == nullptr);
    // Ensure other keys still present.
    assert(map.find(std::string_view(key1)) == (void*)0x1);
    assert(map.find(std::string_view(key3)) == (void*)0x3);

    // Insert again after erase.
    inserted = map.insert(std::string_view(key2), (void*)0x2b);
    assert(inserted);
    assert(map.find(std::string_view(key2)) == (void*)0x2b);

    // Stress test: insert many keys to trigger rehash.
    for (int i = 0; i < 500; ++i) {
        std::string s = "key" + std::to_string(i);
        map.insert(std::string_view(s), (void*)(uintptr_t)i);
    }
    // Verify a few.
    assert(map.find(std::string_view("key0")) == (void*)0);
    assert(map.find(std::string_view("key123")) == (void*)123);
    assert(map.find(std::string_view("key499")) == (void*)499);

    return 0;
}
