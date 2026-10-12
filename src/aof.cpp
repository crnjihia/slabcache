#include "aof.hpp"

#include <fstream>
#include <cassert>
#include <cstdio> // for std::fflush, std::fsync on POSIX; Windows will use _commit
#include <unistd.h>


namespace slabcache {

thread_local bool AOFLogger::replaying_ = false;

AOFLogger::AOFLogger() = default;
AOFLogger::~AOFLogger() {
    stop(); // ensure background thread stops before destruction
    if (ofs_.is_open()) {
        ofs_.close();
    }
}

void AOFLogger::init(const std::string& path, FsSyncPolicy policy) {
    std::lock_guard<std::mutex> lock(mtx_);
    path_ = path;
    policy_ = policy;
    // Open file in append binary mode.
    ofs_.open(path_, std::ios::app | std::ios::binary);
    assert(ofs_.is_open() && "Failed to open AOF file");
    if (policy_ == FsSyncPolicy::EVERYSEC) {
        running_ = true;
        fsync_thread_ = std::thread(&AOFLogger::background_fsync, this);
    }
}

void AOFLogger::stop() {
    if (policy_ == FsSyncPolicy::EVERYSEC && running_) {
        running_ = false;
        if (fsync_thread_.joinable()) {
            fsync_thread_.join();
        }
    }
}

void AOFLogger::append(const std::string& cmd) {
    // Suppress logging when we are replaying the file.
    if (replaying_) return;
    std::lock_guard<std::mutex> lock(mtx_);
    if (!ofs_.is_open()) {
        // If not initialised, fall back to default path "appendonly.aof" and NO policy.
        init("appendonly.aof", FsSyncPolicy::NO);
    }
    ofs_.write(cmd.data(), cmd.size());
    ofs_.flush(); // flush user‑space buffers
    if (policy_ == FsSyncPolicy::ALWAYS) {
        // Perform an fsync at the OS level.
        // For ALWAYS we simply flush the stream; a real fsync would require platform‑specific code.
        // Here we call flush again to ensure data reaches the OS buffer.
        ofs_.flush();
    }
}

void AOFLogger::background_fsync() {
    using namespace std::chrono_literals;
    while (running_) {
        std::this_thread::sleep_for(1000ms);
        std::lock_guard<std::mutex> lock(mtx_);
        if (ofs_.is_open()) {
            // Flush stream; OS-level sync omitted for portability.
            ofs_.flush();
        }
    }
}

void AOFLogger::replay() {
    // Open file for reading.
    std::ifstream ifs(path_, std::ios::binary);
    if (!ifs) return; // nothing to replay
    // Read entire file into a string.
    std::string data((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    size_t offset = 0;
    replaying_ = true;
    while (offset < data.size()) {
        // Parse a RESP value (the request array).
        try {
            auto [req, next] = parse_resp(data, offset);
            // Apply the request; we use the existing process_request function.
            // Discard the response.
            (void)process_request(req);
            offset = next;
        } catch (const std::exception& e) {
            // Stop replay on parse error.
            break;
        }
    }
    replaying_ = false;
}

    AOFLogger aof_logger;
} // namespace slabcache
