#include "protocol.h"
#include "../core/transaction.h"
#include "../core/block.h"
#include "../primitives/serialize.h"
#include "../primitives/hash.h"
#include "../primitives/utils.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace pragma {

// InventoryVector implementation
std::vector<uint8_t> InventoryVector::serialize() const {
    std::vector<uint8_t> result;
    auto typeBytes = Serialize::encodeUint32LE(static_cast<uint32_t>(type));
    auto hashBytes = Serialize::encodeString(hash);
    result.insert(result.end(), typeBytes.begin(), typeBytes.end());
    result.insert(result.end(), hashBytes.begin(), hashBytes.end());
    return result;
}

InventoryVector InventoryVector::deserialize(const std::vector<uint8_t>& data, size_t& offset) {
    InventoryVector inv;
    inv.type = static_cast<InventoryType>(Serialize::decodeUint32LE(data, offset));
    offset += 4;
    auto stringResult = Serialize::decodeString(data, offset);
    inv.hash = stringResult.first;
    offset += stringResult.second;
    return inv;
}

// NetworkAddress implementation
std::vector<uint8_t> NetworkAddress::serialize() const {
    std::vector<uint8_t> result;
    auto servicesBytes = Serialize::encodeUint64LE(services);
    auto ipBytes = Serialize::encodeString(ip);
    auto portBytes = Serialize::encodeUint16LE(port);
    auto timestampBytes = Serialize::encodeUint64LE(timestamp);
    
    result.insert(result.end(), servicesBytes.begin(), servicesBytes.end());
    result.insert(result.end(), ipBytes.begin(), ipBytes.end());
    result.insert(result.end(), portBytes.begin(), portBytes.end());
    result.insert(result.end(), timestampBytes.begin(), timestampBytes.end());
    return result;
}

NetworkAddress NetworkAddress::deserialize(const std::vector<uint8_t>& data, size_t& offset) {
    NetworkAddress addr;
    addr.services = Serialize::decodeUint64LE(data, offset);
    offset += 8;
    auto ipResult = Serialize::decodeString(data, offset);
    addr.ip = ipResult.first;
    offset += ipResult.second;
    addr.port = Serialize::decodeUint16LE(data, offset);
    offset += 2;
    addr.timestamp = Serialize::decodeUint64LE(data, offset);
    offset += 8;
    return addr;
}

// MessageHeader implementation
std::vector<uint8_t> MessageHeader::serialize() const {
    std::vector<uint8_t> result;
    auto magicBytes = Serialize::encodeUint32LE(magic);
    auto commandBytes = Serialize::encodeUint32LE(static_cast<uint32_t>(command));
    auto lengthBytes = Serialize::encodeUint32LE(length);
    auto checksumBytes = Serialize::encodeUint32LE(checksum);
    
    result.insert(result.end(), magicBytes.begin(), magicBytes.end());
    result.insert(result.end(), commandBytes.begin(), commandBytes.end());
    result.insert(result.end(), lengthBytes.begin(), lengthBytes.end());
    result.insert(result.end(), checksumBytes.begin(), checksumBytes.end());
    return result;
}

MessageHeader MessageHeader::deserialize(const std::vector<uint8_t>& data, size_t& offset) {
    MessageHeader header;
    header.magic = Serialize::decodeUint32LE(data, offset);
    offset += 4;
    header.command = static_cast<MessageType>(Serialize::decodeUint32LE(data, offset));
    offset += 4;
    header.length = Serialize::decodeUint32LE(data, offset);
    offset += 4;
    header.checksum = Serialize::decodeUint32LE(data, offset);
    offset += 4;
    return header;
}

// VersionMessage implementation
std::vector<uint8_t> VersionMessage::serialize() const {
    std::vector<uint8_t> result;
    auto versionBytes = Serialize::encodeUint32LE(version);
    auto servicesBytes = Serialize::encodeUint64LE(services);
    auto timestampBytes = Serialize::encodeUint64LE(timestamp);
    auto addrRecvBytes = addrRecv.serialize();
    auto addrFromBytes = addrFrom.serialize();
    auto nonceBytes = Serialize::encodeUint64LE(nonce);
    auto userAgentBytes = Serialize::encodeString(userAgent);
    auto startHeightBytes = Serialize::encodeUint32LE(startHeight);
    auto relayBytes = Serialize::encodeUint8LE(relay ? 1 : 0);
    
    result.insert(result.end(), versionBytes.begin(), versionBytes.end());
    result.insert(result.end(), servicesBytes.begin(), servicesBytes.end());
    result.insert(result.end(), timestampBytes.begin(), timestampBytes.end());
    result.insert(result.end(), addrRecvBytes.begin(), addrRecvBytes.end());
    result.insert(result.end(), addrFromBytes.begin(), addrFromBytes.end());
    result.insert(result.end(), nonceBytes.begin(), nonceBytes.end());
    result.insert(result.end(), userAgentBytes.begin(), userAgentBytes.end());
    result.insert(result.end(), startHeightBytes.begin(), startHeightBytes.end());
    result.insert(result.end(), relayBytes.begin(), relayBytes.end());
    return result;
}

VersionMessage VersionMessage::deserialize(const std::vector<uint8_t>& data, size_t& offset) {
    VersionMessage msg;
    msg.version = Serialize::decodeUint32LE(data, offset);
    offset += 4;
    msg.services = Serialize::decodeUint64LE(data, offset);
    offset += 8;
    msg.timestamp = Serialize::decodeUint64LE(data, offset);
    offset += 8;
    msg.addrRecv = NetworkAddress::deserialize(data, offset);
    msg.addrFrom = NetworkAddress::deserialize(data, offset);
    msg.nonce = Serialize::decodeUint64LE(data, offset);
    offset += 8;
    auto userAgentResult = Serialize::decodeString(data, offset);
    msg.userAgent = userAgentResult.first;
    offset += userAgentResult.second;
    msg.startHeight = Serialize::decodeUint32LE(data, offset);
    offset += 4;
    // For simplicity, assuming encodeUint8LE exists or using a single byte
    msg.relay = (data[offset] != 0);
    offset += 1;
    return msg;
}

// InvMessage implementation
std::vector<uint8_t> InvMessage::serialize() const {
    std::vector<uint8_t> result;
    auto countBytes = Serialize::encodeVarInt(inventory.size());
    result.insert(result.end(), countBytes.begin(), countBytes.end());
    
    for (const auto& inv : inventory) {
        auto invBytes = inv.serialize();
        result.insert(result.end(), invBytes.begin(), invBytes.end());
    }
    return result;
}

InvMessage InvMessage::deserialize(const std::vector<uint8_t>& data, size_t& offset) {
    InvMessage msg;
    auto countResult = Serialize::decodeVarInt(data, offset);
    uint64_t count = countResult.first;
    offset += countResult.second;
    
    msg.inventory.reserve(count);
    for (uint64_t i = 0; i < count; ++i) {
        msg.inventory.push_back(InventoryVector::deserialize(data, offset));
    }
    return msg;
}

// GetDataMessage implementation
std::vector<uint8_t> GetDataMessage::serialize() const {
    std::vector<uint8_t> result;
    auto countBytes = Serialize::encodeVarInt(inventory.size());
    result.insert(result.end(), countBytes.begin(), countBytes.end());
    
    for (const auto& inv : inventory) {
        auto invBytes = inv.serialize();
        result.insert(result.end(), invBytes.begin(), invBytes.end());
    }
    return result;
}

GetDataMessage GetDataMessage::deserialize(const std::vector<uint8_t>& data, size_t& offset) {
    GetDataMessage msg;
    auto countResult = Serialize::decodeVarInt(data, offset);
    uint64_t count = countResult.first;
    offset += countResult.second;
    
    msg.inventory.reserve(count);
    for (uint64_t i = 0; i < count; ++i) {
        msg.inventory.push_back(InventoryVector::deserialize(data, offset));
    }
    return msg;
}

// TxMessage implementation
std::vector<uint8_t> TxMessage::serialize() const {
    // This would need to use Transaction's serialize method
    // For now, return empty vector as placeholder
    return std::vector<uint8_t>();
}

TxMessage TxMessage::deserialize(const std::vector<uint8_t>& data, size_t& offset) {
    TxMessage msg;
    // This would need to use Transaction's deserialize method
    // For now, return empty message as placeholder
    return msg;
}

// BlockMessage implementation
std::vector<uint8_t> BlockMessage::serialize() const {
    // This would need to use Block's serialize method
    // For now, return empty vector as placeholder
    return std::vector<uint8_t>();
}

BlockMessage BlockMessage::deserialize(const std::vector<uint8_t>& data, size_t& offset) {
    BlockMessage msg;
    // This would need to use Block's deserialize method
    // For now, return empty message as placeholder
    return msg;
}

// GetHeadersMessage implementation
std::vector<uint8_t> GetHeadersMessage::serialize() const {
    std::vector<uint8_t> result;
    auto versionBytes = Serialize::encodeUint32LE(version);
    auto countBytes = Serialize::encodeVarInt(locatorHashes.size());
    auto stopHashBytes = Serialize::encodeString(stopHash);
    
    result.insert(result.end(), versionBytes.begin(), versionBytes.end());
    result.insert(result.end(), countBytes.begin(), countBytes.end());
    
    for (const auto& hash : locatorHashes) {
        auto hashBytes = Serialize::encodeString(hash);
        result.insert(result.end(), hashBytes.begin(), hashBytes.end());
    }
    
    result.insert(result.end(), stopHashBytes.begin(), stopHashBytes.end());
    return result;
}

GetHeadersMessage GetHeadersMessage::deserialize(const std::vector<uint8_t>& data, size_t& offset) {
    GetHeadersMessage msg;
    msg.version = Serialize::decodeUint32LE(data, offset);
    offset += 4;
    
    auto countResult = Serialize::decodeVarInt(data, offset);
    uint64_t count = countResult.first;
    offset += countResult.second;
    
    msg.locatorHashes.reserve(count);
    for (uint64_t i = 0; i < count; ++i) {
        auto hashResult = Serialize::decodeString(data, offset);
        msg.locatorHashes.push_back(hashResult.first);
        offset += hashResult.second;
    }
    
    auto stopHashResult = Serialize::decodeString(data, offset);
    msg.stopHash = stopHashResult.first;
    offset += stopHashResult.second;
    
    return msg;
}

// HeadersMessage implementation
std::vector<uint8_t> HeadersMessage::serialize() const {
    std::vector<uint8_t> result;
    auto countBytes = Serialize::encodeVarInt(headers.size());
    result.insert(result.end(), countBytes.begin(), countBytes.end());
    
    // This would need to serialize block headers
    // For now, return basic structure
    return result;
}

HeadersMessage HeadersMessage::deserialize(const std::vector<uint8_t>& data, size_t& offset) {
    HeadersMessage msg;
    auto countResult = Serialize::decodeVarInt(data, offset);
    uint64_t count = countResult.first;
    offset += countResult.second;
    
    // This would need to deserialize block headers
    // For now, return empty message
    return msg;
}

// PingMessage implementation
std::vector<uint8_t> PingMessage::serialize() const {
    return Serialize::encodeUint64LE(nonce);
}

PingMessage PingMessage::deserialize(const std::vector<uint8_t>& data, size_t& offset) {
    PingMessage msg;
    msg.nonce = Serialize::decodeUint64LE(data, offset);
    offset += 8;
    return msg;
}

// PongMessage implementation
std::vector<uint8_t> PongMessage::serialize() const {
    return Serialize::encodeUint64LE(nonce);
}

PongMessage PongMessage::deserialize(const std::vector<uint8_t>& data, size_t& offset) {
    PongMessage msg;
    msg.nonce = Serialize::decodeUint64LE(data, offset);
    offset += 8;
    return msg;
}

// AddrMessage implementation
std::vector<uint8_t> AddrMessage::serialize() const {
    std::vector<uint8_t> result;
    auto countBytes = Serialize::encodeVarInt(addresses.size());
    result.insert(result.end(), countBytes.begin(), countBytes.end());
    
    for (const auto& addr : addresses) {
        auto addrBytes = addr.serialize();
        result.insert(result.end(), addrBytes.begin(), addrBytes.end());
    }
    return result;
}

AddrMessage AddrMessage::deserialize(const std::vector<uint8_t>& data, size_t& offset) {
    AddrMessage msg;
    auto countResult = Serialize::decodeVarInt(data, offset);
    uint64_t count = countResult.first;
    offset += countResult.second;
    
    msg.addresses.reserve(count);
    for (uint64_t i = 0; i < count; ++i) {
        msg.addresses.push_back(NetworkAddress::deserialize(data, offset));
    }
    return msg;
}

// RejectMessage implementation
std::vector<uint8_t> RejectMessage::serialize() const {
    std::vector<uint8_t> result;
    auto messageBytes = Serialize::encodeString(message);
    auto codeBytes = Serialize::encodeUint8LE(code);
    auto reasonBytes = Serialize::encodeString(reason);
    auto dataBytes = Serialize::encodeBytes(data);
    
    result.insert(result.end(), messageBytes.begin(), messageBytes.end());
    result.insert(result.end(), codeBytes.begin(), codeBytes.end());
    result.insert(result.end(), reasonBytes.begin(), reasonBytes.end());
    result.insert(result.end(), dataBytes.begin(), dataBytes.end());
    return result;
}

RejectMessage RejectMessage::deserialize(const std::vector<uint8_t>& data, size_t& offset) {
    RejectMessage msg;
    auto messageResult = Serialize::decodeString(data, offset);
    msg.message = messageResult.first;
    offset += messageResult.second;
    
    // For simplicity, assuming encodeUint8LE exists or using a single byte
    msg.code = data[offset];
    offset += 1;
    
    auto reasonResult = Serialize::decodeString(data, offset);
    msg.reason = reasonResult.first;
    offset += reasonResult.second;
    
    // For simplicity, reading remaining data
    msg.data.assign(data.begin() + offset, data.end());
    
    return msg;
}

// P2PMessage implementation
std::vector<uint8_t> P2PMessage::serialize() const {
    std::vector<uint8_t> payload;
    
    switch (header.command) {
        case MessageType::VERSION:
            if (auto* version = std::get_if<VersionMessage>(&data)) {
                payload = version->serialize();
            }
            break;
        case MessageType::VERACK:
            // Empty payload
            break;
        case MessageType::INV:
            if (auto* inv = std::get_if<InvMessage>(&data)) {
                payload = inv->serialize();
            }
            break;
        case MessageType::GETDATA:
            if (auto* getdata = std::get_if<GetDataMessage>(&data)) {
                payload = getdata->serialize();
            }
            break;
        case MessageType::TX:
            if (auto* tx = std::get_if<TxMessage>(&data)) {
                payload = tx->serialize();
            }
            break;
        case MessageType::BLOCK:
            if (auto* block = std::get_if<BlockMessage>(&data)) {
                payload = block->serialize();
            }
            break;
        case MessageType::GETHEADERS:
            if (auto* getheaders = std::get_if<GetHeadersMessage>(&data)) {
                payload = getheaders->serialize();
            }
            break;
        case MessageType::HEADERS:
            if (auto* headers = std::get_if<HeadersMessage>(&data)) {
                payload = headers->serialize();
            }
            break;
        case MessageType::PING:
            if (auto* ping = std::get_if<PingMessage>(&data)) {
                payload = ping->serialize();
            }
            break;
        case MessageType::PONG:
            if (auto* pong = std::get_if<PongMessage>(&data)) {
                payload = pong->serialize();
            }
            break;
        case MessageType::ADDR:
            if (auto* addr = std::get_if<AddrMessage>(&data)) {
                payload = addr->serialize();
            }
            break;
        case MessageType::REJECT:
            if (auto* reject = std::get_if<RejectMessage>(&data)) {
                payload = reject->serialize();
            }
            break;
    }
    
    // Update header with payload info
    MessageHeader updatedHeader = header;
    updatedHeader.length = static_cast<uint32_t>(payload.size());
    updatedHeader.checksum = Utils::calculateChecksum(payload);
    
    auto headerBytes = updatedHeader.serialize();
    headerBytes.insert(headerBytes.end(), payload.begin(), payload.end());
    
    return headerBytes;
}

P2PMessage P2PMessage::deserialize(const std::vector<uint8_t>& data, size_t& offset) {
    P2PMessage msg;
    msg.header = MessageHeader::deserialize(data, offset);
    
    // Extract payload
    std::vector<uint8_t> payload(data.begin() + offset, data.begin() + offset + msg.header.length);
    size_t payloadOffset = 0;
    
    switch (msg.header.command) {
        case MessageType::VERSION:
            msg.data = VersionMessage::deserialize(payload, payloadOffset);
            break;
        case MessageType::VERACK:
            // Empty payload
            break;
        case MessageType::INV:
            msg.data = InvMessage::deserialize(payload, payloadOffset);
            break;
        case MessageType::GETDATA:
            msg.data = GetDataMessage::deserialize(payload, payloadOffset);
            break;
        case MessageType::TX:
            msg.data = TxMessage::deserialize(payload, payloadOffset);
            break;
        case MessageType::BLOCK:
            msg.data = BlockMessage::deserialize(payload, payloadOffset);
            break;
        case MessageType::GETHEADERS:
            msg.data = GetHeadersMessage::deserialize(payload, payloadOffset);
            break;
        case MessageType::HEADERS:
            msg.data = HeadersMessage::deserialize(payload, payloadOffset);
            break;
        case MessageType::PING:
            msg.data = PingMessage::deserialize(payload, payloadOffset);
            break;
        case MessageType::PONG:
            msg.data = PongMessage::deserialize(payload, payloadOffset);
            break;
        case MessageType::ADDR:
            msg.data = AddrMessage::deserialize(payload, payloadOffset);
            break;
        case MessageType::REJECT:
            msg.data = RejectMessage::deserialize(payload, payloadOffset);
            break;
    }
    
    offset += msg.header.length;
    return msg;
}

} // namespace pragma
