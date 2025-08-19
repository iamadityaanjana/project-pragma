#include "hash.h"
#include <openssl/sha.h>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace pragma {

std::string Hash::dbl_sha256(const std::string& data) {
    std::vector<uint8_t> bytes(data.begin(), data.end());
    return hashInternal(bytes, true);
}

std::string Hash::dbl_sha256(const std::vector<uint8_t>& data) {
    return hashInternal(data, true);
}

std::string Hash::sha256(const std::string& data) {
    std::vector<uint8_t> bytes(data.begin(), data.end());
    return hashInternal(bytes, false);
}

std::string Hash::sha256(const std::vector<uint8_t>& data) {
    return hashInternal(data, false);
}

std::string Hash::hashInternal(const std::vector<uint8_t>& data, bool double_hash) {
    unsigned char hash1[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, data.data(), data.size());
    SHA256_Final(hash1, &sha256);
    
    if (double_hash) {
        unsigned char hash2[SHA256_DIGEST_LENGTH];
        SHA256_Init(&sha256);
        SHA256_Update(&sha256, hash1, SHA256_DIGEST_LENGTH);
        SHA256_Final(hash2, &sha256);
        
        std::vector<uint8_t> result(hash2, hash2 + SHA256_DIGEST_LENGTH);
        return toHex(result);
    } else {
        std::vector<uint8_t> result(hash1, hash1 + SHA256_DIGEST_LENGTH);
        return toHex(result);
    }
}

std::string Hash::toHex(const std::vector<uint8_t>& data) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (uint8_t byte : data) {
        ss << std::setw(2) << static_cast<int>(byte);
    }
    return ss.str();
}

std::vector<uint8_t> Hash::fromHex(const std::string& hex) {
    std::vector<uint8_t> bytes;
    
    // Remove any whitespace and ensure even length
    std::string cleanHex = hex;
    cleanHex.erase(std::remove_if(cleanHex.begin(), cleanHex.end(), ::isspace), cleanHex.end());
    
    if (cleanHex.length() % 2 != 0) {
        throw std::invalid_argument("Hex string must have even length");
    }
    
    for (size_t i = 0; i < cleanHex.length(); i += 2) {
        std::string byteString = cleanHex.substr(i, 2);
        uint8_t byte = static_cast<uint8_t>(std::strtol(byteString.c_str(), nullptr, 16));
        bytes.push_back(byte);
    }
    
    return bytes;
}

std::string Hash::reverseHex(const std::string& hex) {
    std::vector<uint8_t> bytes = fromHex(hex);
    std::reverse(bytes.begin(), bytes.end());
    return toHex(bytes);
}

} // namespace pragma
