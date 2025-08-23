#include "utils.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <random>
#include <iomanip>
#include <cctype>

namespace pragma {

uint64_t Utils::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::seconds>(duration).count();
}

std::string Utils::timestampToString(uint64_t timestamp) {
    std::time_t time = static_cast<std::time_t>(timestamp);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::string Utils::trim(const std::string& str) {
    size_t first = str.find_first_not_of(' ');
    if (first == std::string::npos) return "";
    
    size_t last = str.find_last_not_of(' ');
    return str.substr(first, (last - first + 1));
}

std::vector<std::string> Utils::split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    
    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }
    
    return tokens;
}

bool Utils::startsWith(const std::string& str, const std::string& prefix) {
    return str.size() >= prefix.size() && 
           str.compare(0, prefix.size(), prefix) == 0;
}

bool Utils::endsWith(const std::string& str, const std::string& suffix) {
    return str.size() >= suffix.size() && 
           str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool Utils::isValidHex(const std::string& hex) {
    if (hex.empty()) return false;
    
    return std::all_of(hex.begin(), hex.end(), [](char c) {
        return std::isxdigit(c);
    });
}

bool Utils::isValidHash(const std::string& hash, size_t expectedLength) {
    return hash.length() == expectedLength && isValidHex(hash);
}

std::vector<uint8_t> Utils::randomBytes(size_t length) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint8_t> dis(0, 255);
    
    std::vector<uint8_t> bytes;
    bytes.reserve(length);
    
    for (size_t i = 0; i < length; ++i) {
        bytes.push_back(dis(gen));
    }
    
    return bytes;
}

uint32_t Utils::randomUint32() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint32_t> dis;
    
    return dis(gen);
}

uint64_t Utils::randomUint64() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis;
    
    return dis(gen);
}

void Utils::logInfo(const std::string& message) {
    std::cout << "[INFO] " << message << std::endl;
}

uint32_t Utils::calculateChecksum(const std::vector<uint8_t>& data) {
    // Simple checksum calculation - in production, use SHA256 double hash
    uint32_t checksum = 0;
    for (size_t i = 0; i < data.size(); ++i) {
        checksum += data[i] * (i + 1);
    }
    return checksum;
}

void Utils::logWarning(const std::string& message) {
    std::cout << "[WARN] " << message << std::endl;
}

void Utils::logError(const std::string& message) {
    std::cerr << "[ERROR] " << message << std::endl;
}

void Utils::logDebug(const std::string& message) {
    std::cout << "[DEBUG] " << message << std::endl;
}

} // namespace pragma
