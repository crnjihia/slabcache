#include "ttl.hpp"

#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <queue>
#include <cassert>

namespace slabcache {

TTLManager::TTLManager() = default;

TTLManager::~TTLManager() {
    stop();
}

void TTLManager::start() {
    bool expected = false;
    if (running_.compare_exchange_strong(expected, true)) {
        thread_ = std::thread(&TTLManager::reaper_loop, this);
    }
}

void TTLManager::stop() {
    bool expected = true;
    if (running_.compare_exchange_strong(expected, false)) {
        cv_.notify_all(); // wake up thread.
        if (thread_.joinable()) {
            thread_.join();
        }
    }
}

void TTLManager::set(const std::string& key, std::chrono::seconds ttl) {
    if (ttl.count() <= 0) {
        // Immediate expiration – just remove.
        remove(key);
        return;
    }
    auto expiry = Clock::now() + ttl;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        map_[key] = expiry;
        heap_.emplace(expiry, key);
    }
    cv_.notify_one(); // wake up thread to possibly adjust wait time.
}

void TTLManager::remove(const std::string& key) {
    std::lock_guard<std::mutex> lock(mtx_);
    map_.erase(key);
    // Note: heap may still contain stale entry for this key – will be ignored when popped.
}

std::optional<std::chrono::seconds> TTLManager::time_left(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = map_.find(key);
    if (it == map_.end()) {
        return std::nullopt; // key not present.
    }
    auto now = Clock::now();
    if (now >= it->second) {
        // Already expired – treat as no TTL.
        return std::optional<std::chrono::seconds>{}; // empty optional indicates expired.
    }
    auto secs = std::chrono::duration_cast<std::chrono::seconds>(it->second - now);
    return secs;
}

void TTLManager::reaper_loop() {
    std::unique_lock<std::mutex> lock(mtx_);
    while (running_) {
        if (heap_.empty()) {
            // Wait until a new entry arrives or stop.
            cv_.wait(lock, [this] { return !running_ || !heap_.empty(); });
            continue;
        }
        auto [exp_time, key] = heap_.top();
        auto now = Clock::now();
        if (now >= exp_time) {
            // Expiration due – pop and potentially erase from map.
            heap_.pop();
            auto map_it = map_.find(key);
            if (map_it != map_.end() && map_it->second == exp_time) {
                // Entry still present and matches expiry – erase it.
                map_.erase(map_it);
                // Here we would notify the store to remove the key; omitted for now.
            }
            // Loop to process any other expired items.
            continue;
        }
        // Wait until the next expiry or a new notification.
        cv_.wait_until(lock, exp_time, [this, exp_time] { return !running_ || heap_.empty() || heap_.top().first != exp_time; });
    }
}

// Global manager definition.
TTLManager ttl_manager;

} // namespace slabcache
