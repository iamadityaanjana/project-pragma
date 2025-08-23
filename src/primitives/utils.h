#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <chrono>

namespace pragma {

/**
 * General utility functions for the blockchain
 */
class Utils {
public:
    // Time utilities
    static uint64_t getCurrentTimestamp();
    static std::string timestampToString(uint64_t timestamp);
    
    // String utilities
    static std::string trim(const std::string& str);
    static std::vector<std::string> split(const std::string& str, char delimiter);
    static bool startsWith(const std::string& str, const std::string& prefix);
    static bool endsWith(const std::string& str, const std::string& suffix);
    
    // Validation utilities
    static bool isValidHex(const std::string& hex);
    static bool isValidHash(const std::string& hash, size_t expectedLength = 64);
    
    // Random utilities
    static std::vector<uint8_t> randomBytes(size_t length);
    
    // Checksum utilities
    static uint32_t calculateChecksum(const std::vector<uint8_t>& data);
    static uint32_t randomUint32();
    static uint64_t randomUint64();
    
    // Logging utilities
    static void logInfo(const std::string& message);
    static void logWarning(const std::string& message);
    static void logError(const std::string& message);
    static void logDebug(const std::string& message);
};

} // namespace pragma
