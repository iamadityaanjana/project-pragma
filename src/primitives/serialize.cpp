#include "serialize.h"
#include <stdexcept>
#include <cstring>

namespace pragma {

std::vector<uint8_t> Serialize::encodeVarInt(uint64_t value) {
    std::vector<uint8_t> result;
    
    if (value < 0xFD) {
        result.push_back(static_cast<uint8_t>(value));
    } else if (value <= 0xFFFF) {
        result.push_back(0xFD);
        auto bytes = encodeUint16LE(static_cast<uint16_t>(value));
        result.insert(result.end(), bytes.begin(), bytes.end());
    } else if (value <= 0xFFFFFFFF) {
        result.push_back(0xFE);
        auto bytes = encodeUint32LE(static_cast<uint32_t>(value));
        result.insert(result.end(), bytes.begin(), bytes.end());
    } else {
        result.push_back(0xFF);
        auto bytes = encodeUint64LE(value);
        result.insert(result.end(), bytes.begin(), bytes.end());
    }
    
    return result;
}

std::pair<uint64_t, size_t> Serialize::decodeVarInt(const std::vector<uint8_t>& data, size_t offset) {
    if (offset >= data.size()) {
        throw std::runtime_error("VarInt decode: insufficient data");
    }
    
    uint8_t first = data[offset];
    
    if (first < 0xFD) {
        return {first, 1};
    } else if (first == 0xFD) {
        if (offset + 3 > data.size()) {
            throw std::runtime_error("VarInt decode: insufficient data for uint16");
        }
        uint16_t value = decodeUint16LE(data, offset + 1);
        return {value, 3};
    } else if (first == 0xFE) {
        if (offset + 5 > data.size()) {
            throw std::runtime_error("VarInt decode: insufficient data for uint32");
        }
        uint32_t value = decodeUint32LE(data, offset + 1);
        return {value, 5};
    } else { // 0xFF
        if (offset + 9 > data.size()) {
            throw std::runtime_error("VarInt decode: insufficient data for uint64");
        }
        uint64_t value = decodeUint64LE(data, offset + 1);
        return {value, 9};
    }
}

std::vector<uint8_t> Serialize::encodeUint16LE(uint16_t value) {
    return toLittleEndian(value);
}

std::vector<uint8_t> Serialize::encodeUint32LE(uint32_t value) {
    return toLittleEndian(value);
}

std::vector<uint8_t> Serialize::encodeUint64LE(uint64_t value) {
    return toLittleEndian(value);
}

uint16_t Serialize::decodeUint16LE(const std::vector<uint8_t>& data, size_t offset) {
    return fromLittleEndian<uint16_t>(data, offset);
}

uint32_t Serialize::decodeUint32LE(const std::vector<uint8_t>& data, size_t offset) {
    return fromLittleEndian<uint32_t>(data, offset);
}

uint64_t Serialize::decodeUint64LE(const std::vector<uint8_t>& data, size_t offset) {
    return fromLittleEndian<uint64_t>(data, offset);
}

std::vector<uint8_t> Serialize::encodeString(const std::string& str) {
    auto lengthBytes = encodeVarInt(str.length());
    auto stringBytes = encodeBytes(str);
    
    std::vector<uint8_t> result;
    result.insert(result.end(), lengthBytes.begin(), lengthBytes.end());
    result.insert(result.end(), stringBytes.begin(), stringBytes.end());
    
    return result;
}

std::pair<std::string, size_t> Serialize::decodeString(const std::vector<uint8_t>& data, size_t offset) {
    auto [length, lengthSize] = decodeVarInt(data, offset);
    
    if (offset + lengthSize + length > data.size()) {
        throw std::runtime_error("String decode: insufficient data");
    }
    
    std::string result(data.begin() + offset + lengthSize, 
                      data.begin() + offset + lengthSize + length);
    
    return {result, lengthSize + length};
}

std::vector<uint8_t> Serialize::encodeBytes(const std::vector<uint8_t>& bytes) {
    return bytes;
}

std::vector<uint8_t> Serialize::encodeBytes(const std::string& str) {
    return std::vector<uint8_t>(str.begin(), str.end());
}

std::vector<uint8_t> Serialize::combine(const std::vector<std::vector<uint8_t>>& parts) {
    std::vector<uint8_t> result;
    
    for (const auto& part : parts) {
        result.insert(result.end(), part.begin(), part.end());
    }
    
    return result;
}

template<typename T>
std::vector<uint8_t> Serialize::toLittleEndian(T value) {
    std::vector<uint8_t> result(sizeof(T));
    std::memcpy(result.data(), &value, sizeof(T));
    return result;
}

template<typename T>
T Serialize::fromLittleEndian(const std::vector<uint8_t>& data, size_t offset) {
    if (offset + sizeof(T) > data.size()) {
        throw std::runtime_error("Insufficient data for type conversion");
    }
    
    T result;
    std::memcpy(&result, data.data() + offset, sizeof(T));
    return result;
}

// Explicit template instantiations
template std::vector<uint8_t> Serialize::toLittleEndian<uint16_t>(uint16_t);
template std::vector<uint8_t> Serialize::toLittleEndian<uint32_t>(uint32_t);
template std::vector<uint8_t> Serialize::toLittleEndian<uint64_t>(uint64_t);
template uint16_t Serialize::fromLittleEndian<uint16_t>(const std::vector<uint8_t>&, size_t);
template uint32_t Serialize::fromLittleEndian<uint32_t>(const std::vector<uint8_t>&, size_t);
template uint64_t Serialize::fromLittleEndian<uint64_t>(const std::vector<uint8_t>&, size_t);

} // namespace pragma
