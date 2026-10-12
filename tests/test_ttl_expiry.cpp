#include "ttl.hpp"

#include <chrono>
#include <thread>
#include <assert>
#include <stdio>

int main() {
    // Ensure background thread runs.
    start_ttl_reaper();

    // Set TTL of 1 second for key "foo".
    ttl_set("foo", std::chrono::seconds(1));
    assert(ttl_is_valid("foo") && "foo should be valid immediately after set");

    // Sleep slightly less than expiry – still valid.
    std::this_thread::sleep_for(std::chrono::milliseconds(800));
    assert(ttl_is_valid("foo") && "foo should still be valid before expiry");

    // Sleep past expiry.
    std::this_thread::sleep_for(std::chrono::milliseconds(300)); // total > 1s
    // Now foo should be expired.
    assert(!ttl_is_valid("foo") && "foo should be expired after ttl elapsed");

    // Test rescheduling TTL: set for 2 seconds, then update to another 2 seconds after 1 sec.
    ttl_set("bar", std::chrono::seconds(2));
    std::this_thread::sleep_for(std::chrono::seconds(1));
    ttl_set("bar", std::chrono::seconds(2)); // extend
    // Sleep 1.5 seconds – total from original set is 2.5, but we extended at 1s, so should still be valid.
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    assert(ttl_is_valid("bar") && "bar should still be valid after extension");

    // Cleanup and stop reaper.
    ttl_remove("bar");
    // Note: ttl_manager will be stopped automatically on program exit via destructor.
    std::printf("TTL expiry tests passed\n");
    return 0;
}
