#include "aof.hpp"
#include "resp3.hpp"

#include <cstdio>
#include <assert>
#include <time>

int main() {
    const std::string aof_path = "test_aof.aof";
    // Ensure a clean start.
    std::remove(aof_path.c_str());

    // Initialise logger with NO fsync (tests run fast).
    aof_logger.init(aof_path, FsSyncPolicy::NO);

    // Ensure TTL reaper is active (required for EXPIRE handling).
    start_ttl_reaper();

    // Helper to send a raw request.
    auto send = [&](const std::string& req) {
        return handle_request(req);
    };

    // SET foo bar
    std::string set_req = "*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\nbar\r\n";
    assert(send(set_req) == "+OK\r\n");

    // DEL foo
    std::string del_req = "*2\r\n$3\r\nDEL\r\n$3\r\nfoo\r\n";
    assert(send(del_req) == ":1\r\n");

    // GET foo should be null after delete.
    std::string get_req = "*2\r\n$3\r\nGET\r\n$3\r\nfoo\r\n";
    assert(send(get_req) == "_\r\n");

    // Replay the AOF – it will re‑apply SET then DEL.
    aof_logger.replay();

    // After replay, GET foo should still be null.
    assert(send(get_req) == "_\r\n");

    std::printf("AOF replay test passed\n");
    return 0;
}
