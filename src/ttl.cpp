#include "ttl.hpp"

#include <cassert>

namespace slabcache {

TTLManager::TTLManager() = default;

TTLManager::~TTLManager() {
    stop();
}

void TTLManager::start() {
    bool expected = false;
    if (running_.compare_exchange_strong(expected, true)) {
        if (thread_.joinable()) {
            thread_.join();
        }
        thread_ = std::thread(&TTLManager::reaper_loop, this);
    }
}

void TTLManager::stop() {
    bool expected = true;
    if (running_.compare_exchange_strong(expected, false)) {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            cv_.notify_all();
        }
        if (thread_.joinable()) {
            thread_.join();
        }
    }
}

void TTLManager::set(const std::string& key, std::chrono::seconds ttl) {
    if (ttl.count() <= 0) {
        remove(key);
        return;
    }
    auto expiry = Clock::now() + ttl;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        map_[key] = expiry;
        heap_.emplace(expiry, key);
    }
    cv_.notify_one();
}

void TTLManager::remove(const std::string& key) {
    std::lock_guard<std::mutex> lock(mtx_);
    map_.erase(key);
}

bool TTLManager::is_valid(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = map_.find(key);
    if (it == map_.end()) {
        return false;
    }
    return Clock::now() < it->second;
}

std::optional<std::chrono::seconds> TTLManager::time_left(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = map_.find(key);
    if (it == map_.end()) {
        return std::nullopt;
    }
    auto now = Clock::now();
    if (now >= it->second) {
        return std::optional<std::chrono::seconds>{};  // Expired
    }
    auto secs = std::chrono::duration_cast<std::chrono::seconds>(it->second - now);
    return secs;
}

void TTLManager::reaper_loop() {
    std::unique_lock<std::mutex> lock(mtx_);
    while (running_) {
        if (heap_.empty()) {
            cv_.wait(lock, [this] { return !running_ || !heap_.empty(); });
            continue;
        }
        auto [exp_time, key] = heap_.top();
        auto now = Clock::now();
        if (now >= exp_time) {
            heap_.pop();
            auto map_it = map_.find(key);
            if (map_it != map_.end() && map_it->second == exp_time) {
                map_.erase(map_it);
            }
            continue;
        }
        cv_.wait_until(lock, exp_time, [this, exp_time] {
            return !running_ || heap_.empty() || heap_.top().first != exp_time;
        });
    }
}

TTLManager ttl_manager;

}  // namespace slabcache
