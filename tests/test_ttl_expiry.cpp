#include <cassert>
#include <chrono>
#include <cstdio>
#include <thread>

#include "ttl.hpp"

int main() {
    slabcache::start_ttl_reaper();

    // Set TTL of 1 second for key "foo"
    slabcache::ttl_set("foo", std::chrono::seconds(1));
    assert(slabcache::ttl_is_valid("foo") && "foo should be valid immediately after set");

    // Sleep slightly less than expiry – still valid
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    assert(slabcache::ttl_is_valid("foo") && "foo should still be valid before expiry");

    // Sleep past expiry
    std::this_thread::sleep_for(std::chrono::milliseconds(700));  // total 1.2s > 1s
    assert(!slabcache::ttl_is_valid("foo") && "foo should be expired after ttl elapsed");

    // Test rescheduling TTL: set for 2 seconds, then update to another 2 seconds after 500ms
    slabcache::ttl_set("bar", std::chrono::seconds(2));
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    slabcache::ttl_set("bar", std::chrono::seconds(2));  // extend
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    assert(slabcache::ttl_is_valid("bar") && "bar should still be valid after extension");

    slabcache::ttl_remove("bar");
    slabcache::stop_ttl_reaper();

    std::printf("TTL expiry tests passed\n");
    return 0;
}
