#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace pragma {

/**
 * Hash utilities for blockchain operations
 */
class Hash {
public:
    // Double SHA-256 hash (Bitcoin-style)
    static std::string dbl_sha256(const std::string& data);
    static std::string dbl_sha256(const std::vector<uint8_t>& data);
    
    // Single SHA-256 hash
    static std::string sha256(const std::string& data);
    static std::string sha256(const std::vector<uint8_t>& data);
    
    // Convert hex string to bytes and vice versa
    static std::string toHex(const std::vector<uint8_t>& data);
    static std::vector<uint8_t> fromHex(const std::string& hex);
    
    // Reverse byte order (for endianness conversion)
    static std::string reverseHex(const std::string& hex);
    
private:
    static std::string hashInternal(const std::vector<uint8_t>& data, bool double_hash = false);
};

} // namespace pragma
