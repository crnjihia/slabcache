#include <atomic>
#include <csignal>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using socket_t = SOCKET;
constexpr socket_t INVALID_SOCK = INVALID_SOCKET;
#else
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
using socket_t = int;
constexpr socket_t INVALID_SOCK = -1;
#endif

#include "aof.hpp"
#include "frag_metrics.hpp"
#include "resp3.hpp"
#include "ttl.hpp"

namespace {
std::atomic<bool> g_server_running{true};
socket_t g_server_sock = INVALID_SOCK;

void signal_handler(int) {
    g_server_running = false;
#ifdef _WIN32
    if (g_server_sock != INVALID_SOCK) closesocket(g_server_sock);
#else
    if (g_server_sock != INVALID_SOCK) close(g_server_sock);
#endif
    g_server_sock = INVALID_SOCK;
}

void close_sock(socket_t s) {
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif
}

void handle_client(socket_t client_sock) {
    // Disable Nagle's algorithm for low-latency RESP3 roundtrips
    int flag = 1;
#ifdef _WIN32
    setsockopt(client_sock, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&flag),
               sizeof(flag));
#else
    setsockopt(client_sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
#endif

    std::string buffer;
    buffer.reserve(4096);
    char recv_buf[4096];

    while (g_server_running) {
        int bytes = recv(client_sock, recv_buf, sizeof(recv_buf), 0);
        if (bytes <= 0) {
            break;  // Connection closed or error
        }
        buffer.append(recv_buf, bytes);

        // Process complete commands from buffer
        size_t offset = 0;
        while (offset < buffer.size()) {
            try {
                auto [req_val, next_off] = slabcache::parse_resp(buffer, offset);
                // Extract slice for this command
                std::string cmd_slice = buffer.substr(offset, next_off - offset);
                std::string resp = slabcache::handle_request(cmd_slice);
                send(client_sock, resp.data(), static_cast<int>(resp.size()), 0);
                offset = next_off;
            } catch (const std::exception&) {
                // Wait for more data if packet was fragmented
                break;
            }
        }
        if (offset > 0) {
            buffer.erase(0, offset);
        }
    }
    close_sock(client_sock);
}

}  // namespace

int main(int argc, char* argv[]) {
    int port = 6380;
    std::string aof_file = "appendonly.aof";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-p" || arg == "--port") && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if ((arg == "-a" || arg == "--aof") && i + 1 < argc) {
            aof_file = argv[++i];
        }
    }

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }
#endif

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::cout << "Starting Slabcache server on port " << port << "...\n";

    // Initialize subsystems
    slabcache::start_metrics_sampler();
    slabcache::start_ttl_reaper();
    slabcache::aof_logger.init(aof_file, slabcache::FsSyncPolicy::EVERYSEC);
    slabcache::aof_logger.replay();

    g_server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (g_server_sock == INVALID_SOCK) {
        std::cerr << "Failed to create socket\n";
        return 1;
    }

    int opt = 1;
#ifdef _WIN32
    setsockopt(g_server_sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt),
               sizeof(opt));
#else
    setsockopt(g_server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(static_cast<uint16_t>(port));

    if (bind(g_server_sock, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
        std::cerr << "Failed to bind to port " << port << "\n";
        close_sock(g_server_sock);
        return 1;
    }

    if (listen(g_server_sock, 128) < 0) {
        std::cerr << "Failed to listen on socket\n";
        close_sock(g_server_sock);
        return 1;
    }

    std::cout << "Slabcache listening on 0.0.0.0:" << port << " (RESP3 wire protocol)\n";

    std::vector<std::thread> workers;

    while (g_server_running) {
        sockaddr_in client_addr{};
#ifdef _WIN32
        int addr_len = sizeof(client_addr);
#else
        socklen_t addr_len = sizeof(client_addr);
#endif
        socket_t client_sock =
            accept(g_server_sock, reinterpret_cast<sockaddr*>(&client_addr), &addr_len);
        if (client_sock == INVALID_SOCK) {
            break;
        }
        workers.emplace_back(handle_client, client_sock);
    }

    std::cout << "Shutting down Slabcache server...\n";

    for (auto& t : workers) {
        if (t.joinable()) {
            t.join();
        }
    }

    slabcache::aof_logger.stop();
    slabcache::stop_metrics_sampler();
    slabcache::stop_ttl_reaper();

#ifdef _WIN32
    WSACleanup();
#endif

    std::cout << "Server terminated cleanly.\n";
    return 0;
}
