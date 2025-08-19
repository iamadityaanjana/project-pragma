#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace pragma {

/**
 * Serialization utilities for blockchain data structures
 */
class Serialize {
public:
    // VarInt encoding/decoding (Bitcoin-style)
    static std::vector<uint8_t> encodeVarInt(uint64_t value);
    static std::pair<uint64_t, size_t> decodeVarInt(const std::vector<uint8_t>& data, size_t offset = 0);
    
    // Little-endian encoding/decoding
    static std::vector<uint8_t> encodeUint16LE(uint16_t value);
    static std::vector<uint8_t> encodeUint32LE(uint32_t value);
    static std::vector<uint8_t> encodeUint64LE(uint64_t value);
    static uint16_t decodeUint16LE(const std::vector<uint8_t>& data, size_t offset = 0);
    static uint32_t decodeUint32LE(const std::vector<uint8_t>& data, size_t offset = 0);
    static uint64_t decodeUint64LE(const std::vector<uint8_t>& data, size_t offset = 0);
    
    // String encoding (length-prefixed)
    static std::vector<uint8_t> encodeString(const std::string& str);
    static std::pair<std::string, size_t> decodeString(const std::vector<uint8_t>& data, size_t offset = 0);
    
    // Raw bytes
    static std::vector<uint8_t> encodeBytes(const std::vector<uint8_t>& bytes);
    static std::vector<uint8_t> encodeBytes(const std::string& str);
    
    // Combine multiple byte vectors
    static std::vector<uint8_t> combine(const std::vector<std::vector<uint8_t>>& parts);
    
private:
    // Helper functions for endian conversion
    template<typename T>
    static std::vector<uint8_t> toLittleEndian(T value);
    
    template<typename T>
    static T fromLittleEndian(const std::vector<uint8_t>& data, size_t offset);
};

} // namespace pragma
