#pragma once

#include <string>
#include <vector>
#include <optional>
#include <time>
#include <string_view>

#include "hash_table.hpp"
#include "lru.hpp"
#include "ttl.hpp"

namespace slabcache {

/**
 * @brief RESP3 value types.
 */
enum class RespType {
    SimpleString,
    Error,
    Integer,
    BulkString,
    BlobString,
    Null,
    Array,
};

/**
 * @brief Representation of a RESP3 value.
 */
struct RespValue {
    RespType type = RespType::Null;
    std::string str;                     // For simple, error, bulk, blob.
    long long integer = 0;               // For integer.
    std::vector<RespValue> array; // For arrays.
};

/**
 * @brief Parse a RESP3 value from a string starting at offset.
 * @return Pair of parsed value and new offset after the value.
 */
std::pair<RespValue, size_t> parse_resp(const std::string& data, size_t offset = 0);

/**
 * @brief Serialize a RespValue into a RESP3-formatted string.
 */
std::string serialize_resp(const RespValue& val);

/**
 * @brief Process a request array (first element is command) and produce a response value.
 * This function expects the request to be a RESP3 Array of BulkString arguments.
 */
RespValue process_request(const RespValue& request);

/**
 * @brief Convenience: take raw request string, parse, process, and serialize response.
 */
std::string handle_request(const std::string& raw);

} // namespace slabcache
