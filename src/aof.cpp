#include "aof.hpp"

#include <cassert>
#include <cstdio>
#include <fstream>

#include "resp3.hpp"

#ifdef _WIN32
#include <io.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace slabcache {

thread_local bool AOFLogger::replaying_ = false;

AOFLogger::AOFLogger() = default;

AOFLogger::~AOFLogger() {
    stop();
}

void AOFLogger::init(const std::string& path, FsSyncPolicy policy) {
    stop();
    std::lock_guard<std::mutex> lock(mtx_);
    if (ofs_.is_open()) {
        ofs_.close();
    }
    path_ = path;
    policy_ = policy;
    ofs_.open(path_, std::ios::app | std::ios::binary);
    assert(ofs_.is_open() && "Failed to open AOF file");

    if (policy_ == FsSyncPolicy::EVERYSEC) {
        running_ = true;
        fsync_thread_ = std::thread(&AOFLogger::background_fsync, this);
    }
}

void AOFLogger::stop() {
    if (running_.exchange(false)) {
        if (fsync_thread_.joinable()) {
            fsync_thread_.join();
        }
    }
    std::lock_guard<std::mutex> lock(mtx_);
    if (ofs_.is_open()) {
        ofs_.flush();
        ofs_.close();
    }
}

void AOFLogger::do_fsync() {
    ofs_.flush();
#ifndef _WIN32
    int fd = ::open(path_.c_str(), O_WRONLY | O_APPEND);
    if (fd >= 0) {
        ::fdatasync(fd);
        ::close(fd);
    }
#endif
}

void AOFLogger::append(const std::string& cmd) {
    if (replaying_) return;
    std::lock_guard<std::mutex> lock(mtx_);
    if (!ofs_.is_open()) {
        path_ = "appendonly.aof";
        policy_ = FsSyncPolicy::NO;
        ofs_.open(path_, std::ios::app | std::ios::binary);
    }
    if (ofs_.is_open()) {
        ofs_.write(cmd.data(), cmd.size());
        ofs_.flush();
        if (policy_ == FsSyncPolicy::ALWAYS) {
            do_fsync();
        }
    }
}

void AOFLogger::background_fsync() {
    using namespace std::chrono_literals;
    while (running_) {
        std::this_thread::sleep_for(1000ms);
        if (!running_) break;
        std::lock_guard<std::mutex> lock(mtx_);
        if (ofs_.is_open()) {
            do_fsync();
        }
    }
}

void AOFLogger::replay() {
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (ofs_.is_open()) {
            ofs_.flush();
        }
    }
    std::ifstream ifs(path_, std::ios::binary);
    if (!ifs) return;

    std::string data((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    size_t offset = 0;
    replaying_ = true;
    while (offset < data.size()) {
        try {
            auto [req, next] = parse_resp(data, offset);
            (void)process_request(req);
            if (next <= offset) {
                break;
            }
            offset = next;
        } catch (const std::exception&) {
            break;
        }
    }
    replaying_ = false;
}

AOFLogger aof_logger;

}  // namespace slabcache
