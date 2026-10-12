#include "resp3.hpp"
#include "store.hpp"
#include "frag_metrics.hpp"
#include "aof.hpp"


#include <cstring>
#include <stdexcept>
#include <algorithm>
#include <sstream>

namespace slabcache {

// Helper to read a line terminated by CRLF.
static std::string read_line(const std::string& data, size_t& offset) {
    size_t pos = data.find("\r\n", offset);
    if (pos == std::string::npos) {
        throw std::runtime_error("Incomplete RESP data");
    }
    std::string line = data.substr(offset, pos - offset);
    offset = pos + 2; // skip CRLF
    return line;
}

static RespValue parse_simple(const std::string& data, size_t& offset, RespType type) {
    RespValue v; v.type = type;
    v.str = read_line(data, offset);
    return v;
}

static RespValue parse_integer(const std::string& data, size_t& offset) {
    RespValue v; v.type = RespType::Integer;
    std::string line = read_line(data, offset);
    v.integer = std::stoll(line);
    return v;
}

static RespValue parse_bulk(const std::string& data, size_t& offset, RespType type) {
    RespValue v; v.type = type;
    std::string len_str = read_line(data, offset);
    long len = std::stol(len_str);
    if (len == -1) {
        // Null bulk.
        v.type = RespType::Null;
        return v;
    }
    if (len < 0) throw std::runtime_error("Invalid bulk length");
    if (static_cast<size_t>(len) + 2 > data.size() - offset) {
        throw std::runtime_error("Incomplete bulk data");
    }
    v.str = data.substr(offset, static_cast<size_t>(len));
    offset += static_cast<size_t>(len);
    // Expect CRLF after payload.
    if (data.substr(offset, 2) != "\r\n") {
        throw std::runtime_error("Missing CRLF after bulk payload");
    }
    offset += 2;
    return v;
}

std::pair<RespValue, size_t> parse_resp(const std::string& data, size_t offset) {
    if (offset >= data.size()) throw std::runtime_error("Empty RESP data");
    char prefix = data[offset++];
    RespValue v;
    switch (prefix) {
        case '+': v = parse_simple(data, offset, RespType::SimpleString); break;
        case '-': v = parse_simple(data, offset, RespType::Error); break;
        case ':': v = parse_integer(data, offset); break;
        case '$': v = parse_bulk(data, offset, RespType::BulkString); break;
        case '`': v = parse_bulk(data, offset, RespType::BlobString); break;
        case '_': {
            // Null – just consume CRLF.
            std::string dummy = read_line(data, offset);
            v.type = RespType::Null;
            break;
        }
        case '*': {
            v.type = RespType::Array;
            std::string count_str = read_line(data, offset);
            int count = std::stoi(count_str);
            if (count < 0) {
                // Null array – treat as Null.
                v.type = RespType::Null;
                break;
            }
            v.array.reserve(count);
            for (int i = 0; i < count; ++i) {
                auto [elem, new_off] = parse_resp(data, offset);
                v.array.push_back(std::move(elem));
                offset = new_off;
            }
            break;
        }
        case '%': {
            // Map – not needed for our command protocol, treat as error for now.
            throw std::runtime_error("Map parsing not implemented");
        }
        default:
            throw std::runtime_error(std::string("Unknown RESP prefix: ") + prefix);
    }
    return {v, offset};
}

// Serialization helpers.
static std::string serialize_simple(const RespValue& v) {
    char prefix = '+';
    if (v.type == RespType::Error) prefix = '-';
    return std::string(1, prefix) + v.str + "\r\n";
}

static std::string serialize_integer(const RespValue& v) {
    return ":" + std::to_string(v.integer) + "\r\n";
}

static std::string serialize_bulk(const RespValue& v) {
    if (v.type == RespType::Null) {
        return "$-1\r\n"; // RESP2 null bulk string
    }
    return "$" + std::to_string(v.str.size()) + "\r\n" + v.str + "\r\n";
}

static std::string serialize_array(const RespValue& v) {
    if (v.type == RespType::Null) {
        return "* -1\r\n"; // Not used
    }
    std::string out = "*" + std::to_string(v.array.size()) + "\r\n";
    for (const auto& elem : v.array) {
        out += serialize_resp(elem);
    }
    return out;
}

std::string serialize_resp(const RespValue& val) {
    switch (val.type) {
        case RespType::SimpleString:
        case RespType::Error:
            return serialize_simple(val);
        case RespType::Integer:
            return serialize_integer(val);
        case RespType::BulkString:
        case RespType::BlobString:
        case RespType::Null:
            return serialize_bulk(val);
        case RespType::Array:
            return serialize_array(val);
    }
    return ""; // unreachable
}

// Store implementation.
static Store global_store; // defined in store.cpp later.

RespValue process_request(const RespValue& request) {
    if (request.type != RespType::Array) {
        RespValue err{RespType::Error, "ERR expected array request"};
        return err;
    }
    if (request.array.empty()) {
        RespValue err{RespType::Error, "ERR empty request"};
        return err;
    }
    // First element must be bulk/string command.
    const RespValue& cmd_val = request.array[0];
    if (cmd_val.type != RespType::BulkString && cmd_val.type != RespType::SimpleString) {
        RespValue err{RespType::Error, "ERR command must be bulk string"};
        return err;
    }
    std::string cmd = cmd_val.str;
    // Transform to upper case.
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);

    // Helper to collect arguments as strings.
    std::vector<std::string> args;
    for (size_t i = 1; i < request.array.size(); ++i) {
        const RespValue& a = request.array[i];
        if (a.type == RespType::BulkString || a.type == RespType::SimpleString) {
            args.push_back(a.str);
        } else if (a.type == RespType::Null) {
            args.push_back("");
        } else {
            // ignore unsupported types.
            args.push_back("");
        }
    }

    // Dispatch based on command.
    if (cmd == "PING") {
        return RespValue{RespType::SimpleString, "PONG"};
    }
    if (cmd == "QUIT") {
        // In a real server we would close connection. Here just OK.
        return RespValue{RespType::SimpleString, "OK"};
    }
    if (cmd == "SET") {
        if (args.size() < 2) return RespValue{RespType::Error, "ERR wrong number of arguments for 'SET'"};
        global_store.set(args[0], args[1]);
        return RespValue{RespType::SimpleString, "OK"};
    }
    if (cmd == "GET") {
        if (args.empty()) return RespValue{RespType::Error, "ERR wrong number of arguments for 'GET'"};
        auto opt = global_store.get(args[0]);
        if (!opt) return RespValue{RespType::Null, ""};
        return RespValue{RespType::BulkString, *opt};
    }
    if (cmd == "DEL") {
        if (args.empty()) return RespValue{RespType::Error, "ERR wrong number of arguments for 'DEL'"};
        bool removed = global_store.del(args[0]);
        return RespValue{RespType::Integer, "", removed ? 1 : 0};
    }
    if (cmd == "EXPIRE") {
        if (args.size() < 2) return RespValue{RespType::Error, "ERR wrong number of arguments for 'EXPIRE'"};
        long secs = std::stol(args[1]);
        bool ok = global_store.expire(args[0], std::chrono::seconds(secs));
        return RespValue{RespType::Integer, "", ok ? 1 : 0};
    }
    if (cmd == "TTL") {
        if (args.empty()) return RespValue{RespType::Error, "ERR wrong number of arguments for 'TTL'"};
        auto maybe = global_store.ttl(args[0]);
        if (!maybe) {
            // Key does not exist.
            return RespValue{RespType::Integer, "", -2};
        }
        if (!maybe->has_value()) {
            // No expiry.
            return RespValue{RespType::Integer, "", -1};
        }
        return RespValue{RespType::Integer, "", maybe->value().count()};
    }
    if (cmd == "STATS") {
        // Return simple string with placeholder metrics.
        Metrics m = get_latest_metrics();
        char buf[128];
        std::snprintf(buf, sizeof(buf), "ext=%.2f%% int=%.2f%%", m.external_frag_percent, m.internal_frag_percent);
        return RespValue{RespType::SimpleString, std::string(buf)};
    }
    if (cmd == "INFO") {
        return RespValue{RespType::SimpleString, "Slabcache 1.0"};
    }

    // Unknown command.
    return RespValue{RespType::Error, "ERR unknown command"};
}

std::string handle_request(const std::string& raw) {
    // Ensure background threads are running.
    start_ttl_reaper();
    // Parse request.
    auto [val, off] = parse_resp(raw);
    (void)off; // ignore extra data.
    // Determine command name for AOF logging.
    std::string cmd_name;
    if (val.type == RespType::Array && !val.array.empty()) {
        const RespValue& cv = val.array[0];
        if (cv.type == RespType::BulkString || cv.type == RespType::SimpleString) {
            cmd_name = cv.str;
            std::transform(cmd_name.begin(), cmd_name.end(), cmd_name.begin(), ::toupper);
        }
    }
    bool is_mutating = (cmd_name == "SET" || cmd_name == "DEL" || cmd_name == "EXPIRE");
    if (is_mutating) {
        aof_logger.append(raw);
    }
    RespValue resp = process_request(val);
    return serialize_resp(resp);
}


} // namespace slabcache
