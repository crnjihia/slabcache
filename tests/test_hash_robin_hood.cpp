#include <cassert>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "hash_table.hpp"

int main() {
    slabcache::RobinHoodMap map(32);

    const char* key1 = "alpha";
    const char* key2 = "beta";
    const char* key3 = "gamma";
    const char* key4 = "delta";

    // Insert entries
    bool inserted;
    inserted = map.insert(std::string_view(key1), (void*)0x1);
    assert(inserted);
    inserted = map.insert(std::string_view(key2), (void*)0x2);
    assert(inserted);
    inserted = map.insert(std::string_view(key3), (void*)0x3);
    assert(inserted);

    // Overwrite existing key
    inserted = map.insert(std::string_view(key2), (void*)0x22);
    assert(!inserted);  // not a new entry

    // Find values
    assert(map.find(std::string_view(key1)) == (void*)0x1);
    assert(map.find(std::string_view(key2)) == (void*)0x22);
    assert(map.find(std::string_view(key3)) == (void*)0x3);
    assert(map.find(std::string_view(key4)) == nullptr);

    // Erase a key
    bool erased = map.erase(std::string_view(key2));
    assert(erased);
    assert(map.find(std::string_view(key2)) == nullptr);
    assert(map.find(std::string_view(key1)) == (void*)0x1);
    assert(map.find(std::string_view(key3)) == (void*)0x3);

    // Insert again after erase
    inserted = map.insert(std::string_view(key2), (void*)0x2b);
    assert(inserted);
    assert(map.find(std::string_view(key2)) == (void*)0x2b);

    // Stress test: insert many keys to trigger rehash
    std::vector<std::string> keys;
    keys.reserve(500);
    for (int i = 0; i < 500; ++i) {
        keys.push_back("key" + std::to_string(i));
    }
    for (int i = 0; i < 500; ++i) {
        map.insert(std::string_view(keys[i]), reinterpret_cast<void*>(static_cast<uintptr_t>(i)));
    }

    assert(map.find(std::string_view("key0")) ==
           reinterpret_cast<void*>(static_cast<uintptr_t>(0)));
    assert(map.find(std::string_view("key123")) ==
           reinterpret_cast<void*>(static_cast<uintptr_t>(123)));
    assert(map.find(std::string_view("key499")) ==
           reinterpret_cast<void*>(static_cast<uintptr_t>(499)));

    return 0;
}
