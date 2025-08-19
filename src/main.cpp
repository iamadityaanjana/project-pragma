#include <iostream>
#include "primitives/hash.h"
#include "primitives/serialize.h"
#include "primitives/utils.h"

using namespace pragma;

int main() {
    Utils::logInfo("=== Pragma Blockchain - Step 1: Hash & Serialize Primitives ===");
    
    // Test hash functions
    Utils::logInfo("\n1. Testing Hash Functions:");
    std::string testData = "Hello, Blockchain!";
    std::string sha256Hash = Hash::sha256(testData);
    std::string doubleSHA256 = Hash::dbl_sha256(testData);
    
    std::cout << "Input: " << testData << std::endl;
    std::cout << "SHA256: " << sha256Hash << std::endl;
    std::cout << "Double SHA256: " << doubleSHA256 << std::endl;
    
    // Test hex conversion
    Utils::logInfo("\n2. Testing Hex Conversion:");
    std::vector<uint8_t> bytes = {0xDE, 0xAD, 0xBE, 0xEF};
    std::string hex = Hash::toHex(bytes);
    std::cout << "Bytes to Hex: " << hex << std::endl;
    
    auto converted = Hash::fromHex(hex);
    std::cout << "Hex to Bytes: ";
    for (auto byte : converted) {
        std::cout << std::hex << static_cast<int>(byte) << " ";
    }
    std::cout << std::dec << std::endl;
    
    // Test serialization
    Utils::logInfo("\n3. Testing Serialization:");
    
    // VarInt test
    uint64_t varIntValue = 300;
    auto encoded = Serialize::encodeVarInt(varIntValue);
    auto [decoded, size] = Serialize::decodeVarInt(encoded);
    
    std::cout << "VarInt " << varIntValue << " -> " << encoded.size() << " bytes -> " << decoded << std::endl;
    
    // Uint32 test
    uint32_t value32 = 0x12345678;
    auto encoded32 = Serialize::encodeUint32LE(value32);
    auto decoded32 = Serialize::decodeUint32LE(encoded32);
    
    std::cout << "Uint32 " << std::hex << value32 << " -> " << decoded32 << std::dec << std::endl;
    
    // String test
    std::string testString = "Blockchain";
    auto encodedString = Serialize::encodeString(testString);
    auto [decodedString, stringSize] = Serialize::decodeString(encodedString);
    
    std::cout << "String '" << testString << "' -> " << encodedString.size() 
              << " bytes -> '" << decodedString << "'" << std::endl;
    
    // Test utilities
    Utils::logInfo("\n4. Testing Utilities:");
    uint64_t timestamp = Utils::getCurrentTimestamp();
    std::cout << "Current timestamp: " << timestamp << std::endl;
    std::cout << "Formatted: " << Utils::timestampToString(timestamp) << std::endl;
    
    auto randomBytes = Utils::randomBytes(8);
    std::cout << "Random bytes: " << Hash::toHex(randomBytes) << std::endl;
    
    Utils::logInfo("\n✅ Hash & Serialize Primitives working!");
    
    return 0;
}
