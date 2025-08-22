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
std::string InventoryVector::serialize() const {
    std::string result;
    result += Serialize::writeUint32(static_cast<uint32_t>(type));
    result += Serialize::writeString(hash);
    return result;
}

InventoryVector InventoryVector::deserialize(const std::string& data, size_t& offset) {
    InventoryVector inv;
    inv.type = static_cast<InventoryType>(Serialize::readUint32(data, offset));
    inv.hash = Serialize::readString(data, offset);
    return inv;
}

// NetworkAddress implementation
std::string NetworkAddress::serialize() const {
    std::string result;
    result += Serialize::writeUint64(services);
    result += Serialize::writeString(ip);
    result += Serialize::writeUint16(port);
    result += Serialize::writeUint64(timestamp);
    return result;
}

NetworkAddress NetworkAddress::deserialize(const std::string& data, size_t& offset) {
    NetworkAddress addr;
    addr.services = Serialize::readUint64(data, offset);
    addr.ip = Serialize::readString(data, offset);
    addr.port = Serialize::readUint16(data, offset);
    addr.timestamp = Serialize::readUint64(data, offset);
    return addr;
}

std::string NetworkAddress::toString() const {
    return ip + ":" + std::to_string(port);
}

// MessageHeader implementation
std::string MessageHeader::serialize() const {
    std::string result;
    result += Serialize::writeUint32(magic);
    result += Serialize::writeUint32(static_cast<uint32_t>(command));
    result += Serialize::writeUint32(length);
    result += Serialize::writeUint32(checksum);
    return result;
}

MessageHeader MessageHeader::deserialize(const std::string& data) {
    MessageHeader header;
    size_t offset = 0;
    header.magic = Serialize::readUint32(data, offset);
    header.command = static_cast<MessageType>(Serialize::readUint32(data, offset));
    header.length = Serialize::readUint32(data, offset);
    header.checksum = Serialize::readUint32(data, offset);
    return header;
}

bool MessageHeader::isValid() const {
    return magic == MAGIC_MAIN && MessageUtils::isValidMessageType(static_cast<uint32_t>(command));
}

// VersionMessage implementation
std::string VersionMessage::serialize() const {
    std::string result;
    result += Serialize::writeUint32(version);
    result += Serialize::writeUint64(services);
    result += Serialize::writeUint64(timestamp);
    result += addrRecv.serialize();
    result += addrFrom.serialize();
    result += Serialize::writeUint64(nonce);
    result += Serialize::writeString(userAgent);
    result += Serialize::writeUint32(startHeight);
    result += Serialize::writeBool(relay);
    return result;
}

VersionMessage VersionMessage::deserialize(const std::string& data) {
    VersionMessage version;
    size_t offset = 0;
    version.version = Serialize::readUint32(data, offset);
    version.services = Serialize::readUint64(data, offset);
    version.timestamp = Serialize::readUint64(data, offset);
    version.addrRecv = NetworkAddress::deserialize(data, offset);
    version.addrFrom = NetworkAddress::deserialize(data, offset);
    version.nonce = Serialize::readUint64(data, offset);
    version.userAgent = Serialize::readString(data, offset);
    version.startHeight = Serialize::readUint32(data, offset);
    version.relay = Serialize::readBool(data, offset);
    return version;
}

// PingPongMessage implementation
std::string PingPongMessage::serialize() const {
    return Serialize::writeUint64(nonce);
}

PingPongMessage PingPongMessage::deserialize(const std::string& data) {
    PingPongMessage ping;
    size_t offset = 0;
    ping.nonce = Serialize::readUint64(data, offset);
    return ping;
}

// InvMessage implementation
std::string InvMessage::serialize() const {
    std::string result;
    result += Serialize::writeVarInt(inventory.size());
    for (const auto& inv : inventory) {
        result += inv.serialize();
    }
    return result;
}

InvMessage InvMessage::deserialize(const std::string& data) {
    InvMessage inv;
    size_t offset = 0;
    uint64_t count = Serialize::readVarInt(data, offset);
    inv.inventory.reserve(count);
    
    for (uint64_t i = 0; i < count; ++i) {
        inv.inventory.push_back(InventoryVector::deserialize(data, offset));
    }
    
    return inv;
}

// GetHeadersMessage implementation
std::string GetHeadersMessage::serialize() const {
    std::string result;
    result += Serialize::writeUint32(version);
    result += Serialize::writeVarInt(locatorHashes.size());
    for (const auto& hash : locatorHashes) {
        result += Serialize::writeString(hash);
    }
    result += Serialize::writeString(hashStop);
    return result;
}

GetHeadersMessage GetHeadersMessage::deserialize(const std::string& data) {
    GetHeadersMessage getHeaders;
    size_t offset = 0;
    getHeaders.version = Serialize::readUint32(data, offset);
    uint64_t count = Serialize::readVarInt(data, offset);
    getHeaders.locatorHashes.reserve(count);
    
    for (uint64_t i = 0; i < count; ++i) {
        getHeaders.locatorHashes.push_back(Serialize::readString(data, offset));
    }
    
    getHeaders.hashStop = Serialize::readString(data, offset);
    return getHeaders;
}

// HeadersMessage implementation
std::string HeadersMessage::serialize() const {
    std::string result;
    result += Serialize::writeVarInt(headers.size());
    for (const auto& header : headers) {
        result += Serialize::writeString(header);
    }
    return result;
}

HeadersMessage HeadersMessage::deserialize(const std::string& data) {
    HeadersMessage headers;
    size_t offset = 0;
    uint64_t count = Serialize::readVarInt(data, offset);
    headers.headers.reserve(count);
    
    for (uint64_t i = 0; i < count; ++i) {
        headers.headers.push_back(Serialize::readString(data, offset));
    }
    
    return headers;
}

// AddrMessage implementation
std::string AddrMessage::serialize() const {
    std::string result;
    result += Serialize::writeVarInt(addresses.size());
    for (const auto& addr : addresses) {
        result += addr.serialize();
    }
    return result;
}

AddrMessage AddrMessage::deserialize(const std::string& data) {
    AddrMessage addr;
    size_t offset = 0;
    uint64_t count = Serialize::readVarInt(data, offset);
    addr.addresses.reserve(count);
    
    for (uint64_t i = 0; i < count; ++i) {
        addr.addresses.push_back(NetworkAddress::deserialize(data, offset));
    }
    
    return addr;
}

// RejectMessage implementation
std::string RejectMessage::serialize() const {
    std::string result;
    result += Serialize::writeString(message);
    result += Serialize::writeUint8(code);
    result += Serialize::writeString(reason);
    result += Serialize::writeString(data);
    return result;
}

RejectMessage RejectMessage::deserialize(const std::string& data) {
    RejectMessage reject;
    size_t offset = 0;
    reject.message = Serialize::readString(data, offset);
    reject.code = Serialize::readUint8(data, offset);
    reject.reason = Serialize::readString(data, offset);
    reject.data = Serialize::readString(data, offset);
    return reject;
}

// P2PMessage implementation
P2PMessage::P2PMessage(MessageType type, const std::string& data) {
    header.magic = MessageHeader::MAGIC_MAIN;
    header.command = type;
    header.length = data.length();
    header.checksum = calculateChecksum(data);
    payload = data;
}

std::string P2PMessage::serialize() const {
    return header.serialize() + payload;
}

std::shared_ptr<P2PMessage> P2PMessage::deserialize(const std::string& data) {
    if (data.length() < MessageHeader::HEADER_SIZE) {
        return nullptr;
    }
    
    auto message = std::make_shared<P2PMessage>();
    message->header = MessageHeader::deserialize(data.substr(0, MessageHeader::HEADER_SIZE));
    
    if (!message->header.isValid()) {
        return nullptr;
    }
    
    if (data.length() < MessageHeader::HEADER_SIZE + message->header.length) {
        return nullptr;
    }
    
    message->payload = data.substr(MessageHeader::HEADER_SIZE, message->header.length);
    
    if (message->calculateChecksum(message->payload) != message->header.checksum) {
        return nullptr;
    }
    
    return message;
}

bool P2PMessage::isValid() const {
    return header.isValid() && calculateChecksum(payload) == header.checksum;
}

uint32_t P2PMessage::calculateChecksum(const std::string& data) const {
    return MessageUtils::calculateChecksum(data);
}

// Message getter methods
VersionMessage P2PMessage::getVersionMessage() const {
    return VersionMessage::deserialize(payload);
}

PingPongMessage P2PMessage::getPingPongMessage() const {
    return PingPongMessage::deserialize(payload);
}

InvMessage P2PMessage::getInvMessage() const {
    return InvMessage::deserialize(payload);
}

GetDataMessage P2PMessage::getGetDataMessage() const {
    return GetDataMessage::deserialize(payload);
}

GetHeadersMessage P2PMessage::getGetHeadersMessage() const {
    return GetHeadersMessage::deserialize(payload);
}

HeadersMessage P2PMessage::getHeadersMessage() const {
    return HeadersMessage::deserialize(payload);
}

AddrMessage P2PMessage::getAddrMessage() const {
    return AddrMessage::deserialize(payload);
}

RejectMessage P2PMessage::getRejectMessage() const {
    return RejectMessage::deserialize(payload);
}

// Factory methods
std::shared_ptr<P2PMessage> P2PMessage::createVersion(const VersionMessage& version) {
    return std::make_shared<P2PMessage>(MessageType::VERSION, version.serialize());
}

std::shared_ptr<P2PMessage> P2PMessage::createVerAck() {
    return std::make_shared<P2PMessage>(MessageType::VERACK, "");
}

std::shared_ptr<P2PMessage> P2PMessage::createPing(uint64_t nonce) {
    PingPongMessage ping;
    ping.nonce = nonce;
    return std::make_shared<P2PMessage>(MessageType::PING, ping.serialize());
}

std::shared_ptr<P2PMessage> P2PMessage::createPong(uint64_t nonce) {
    PingPongMessage pong;
    pong.nonce = nonce;
    return std::make_shared<P2PMessage>(MessageType::PONG, pong.serialize());
}

std::shared_ptr<P2PMessage> P2PMessage::createInv(const std::vector<InventoryVector>& inv) {
    InvMessage invMsg;
    invMsg.inventory = inv;
    return std::make_shared<P2PMessage>(MessageType::INV, invMsg.serialize());
}

std::shared_ptr<P2PMessage> P2PMessage::createGetData(const std::vector<InventoryVector>& inv) {
    GetDataMessage getDataMsg;
    getDataMsg.inventory = inv;
    return std::make_shared<P2PMessage>(MessageType::GETDATA, getDataMsg.serialize());
}

std::shared_ptr<P2PMessage> P2PMessage::createGetHeaders(const GetHeadersMessage& getHeaders) {
    return std::make_shared<P2PMessage>(MessageType::GETHEADERS, getHeaders.serialize());
}

std::shared_ptr<P2PMessage> P2PMessage::createHeaders(const std::vector<std::string>& headers) {
    HeadersMessage headerMsg;
    headerMsg.headers = headers;
    return std::make_shared<P2PMessage>(MessageType::HEADERS, headerMsg.serialize());
}

std::shared_ptr<P2PMessage> P2PMessage::createTx(const Transaction& tx) {
    return std::make_shared<P2PMessage>(MessageType::TX, tx.serialize());
}

std::shared_ptr<P2PMessage> P2PMessage::createBlock(const Block& block) {
    return std::make_shared<P2PMessage>(MessageType::BLOCK, block.serialize());
}

std::shared_ptr<P2PMessage> P2PMessage::createAddr(const std::vector<NetworkAddress>& addresses) {
    AddrMessage addrMsg;
    addrMsg.addresses = addresses;
    return std::make_shared<P2PMessage>(MessageType::ADDR, addrMsg.serialize());
}

std::shared_ptr<P2PMessage> P2PMessage::createReject(const RejectMessage& reject) {
    return std::make_shared<P2PMessage>(MessageType::REJECT, reject.serialize());
}

// MessageUtils implementation
std::string MessageUtils::messageTypeToString(MessageType type) {
    switch (type) {
        case MessageType::VERSION: return "version";
        case MessageType::VERACK: return "verack";
        case MessageType::PING: return "ping";
        case MessageType::PONG: return "pong";
        case MessageType::INV: return "inv";
        case MessageType::GETDATA: return "getdata";
        case MessageType::GETBLOCKS: return "getblocks";
        case MessageType::GETHEADERS: return "getheaders";
        case MessageType::TX: return "tx";
        case MessageType::BLOCK: return "block";
        case MessageType::HEADERS: return "headers";
        case MessageType::ADDR: return "addr";
        case MessageType::GETADDR: return "getaddr";
        case MessageType::REJECT: return "reject";
        case MessageType::MEMPOOL: return "mempool";
        case MessageType::NOTFOUND: return "notfound";
        default: return "unknown";
    }
}

MessageType MessageUtils::stringToMessageType(const std::string& str) {
    if (str == "version") return MessageType::VERSION;
    if (str == "verack") return MessageType::VERACK;
    if (str == "ping") return MessageType::PING;
    if (str == "pong") return MessageType::PONG;
    if (str == "inv") return MessageType::INV;
    if (str == "getdata") return MessageType::GETDATA;
    if (str == "getblocks") return MessageType::GETBLOCKS;
    if (str == "getheaders") return MessageType::GETHEADERS;
    if (str == "tx") return MessageType::TX;
    if (str == "block") return MessageType::BLOCK;
    if (str == "headers") return MessageType::HEADERS;
    if (str == "addr") return MessageType::ADDR;
    if (str == "getaddr") return MessageType::GETADDR;
    if (str == "reject") return MessageType::REJECT;
    if (str == "mempool") return MessageType::MEMPOOL;
    if (str == "notfound") return MessageType::NOTFOUND;
    return static_cast<MessageType>(0);
}

std::string MessageUtils::inventoryTypeToString(InventoryType type) {
    switch (type) {
        case InventoryType::ERROR: return "error";
        case InventoryType::TX: return "tx";
        case InventoryType::BLOCK: return "block";
        case InventoryType::FILTERED_BLOCK: return "filtered_block";
        case InventoryType::COMPACT_BLOCK: return "compact_block";
        default: return "unknown";
    }
}

InventoryType MessageUtils::stringToInventoryType(const std::string& str) {
    if (str == "error") return InventoryType::ERROR;
    if (str == "tx") return InventoryType::TX;
    if (str == "block") return InventoryType::BLOCK;
    if (str == "filtered_block") return InventoryType::FILTERED_BLOCK;
    if (str == "compact_block") return InventoryType::COMPACT_BLOCK;
    return InventoryType::ERROR;
}

bool MessageUtils::isValidMessageType(uint32_t type) {
    return type >= 1 && type <= 16;
}

bool MessageUtils::isValidInventoryType(uint32_t type) {
    return type <= 4;
}

size_t MessageUtils::getExpectedPayloadSize(MessageType type) {
    switch (type) {
        case MessageType::VERACK: return 0;
        case MessageType::PING:
        case MessageType::PONG: return 8;
        case MessageType::GETADDR: return 0;
        case MessageType::MEMPOOL: return 0;
        default: return SIZE_MAX; // Variable size
    }
}

std::string MessageUtils::formatAddress(const std::string& ip, uint16_t port) {
    return ip + ":" + std::to_string(port);
}

bool MessageUtils::parseAddress(const std::string& address, std::string& ip, uint16_t& port) {
    size_t colonPos = address.find_last_of(':');
    if (colonPos == std::string::npos) {
        return false;
    }
    
    ip = address.substr(0, colonPos);
    try {
        port = static_cast<uint16_t>(std::stoul(address.substr(colonPos + 1)));
        return isValidIP(ip);
    } catch (...) {
        return false;
    }
}

bool MessageUtils::isValidIP(const std::string& ip) {
    // Simple IPv4 validation
    size_t dots = 0;
    size_t start = 0;
    
    for (size_t i = 0; i <= ip.length(); ++i) {
        if (i == ip.length() || ip[i] == '.') {
            if (i == start) return false; // Empty segment
            
            std::string segment = ip.substr(start, i - start);
            if (segment.length() > 3) return false;
            
            try {
                int value = std::stoi(segment);
                if (value < 0 || value > 255) return false;
            } catch (...) {
                return false;
            }
            
            start = i + 1;
            dots++;
        }
    }
    
    return dots == 4;
}

bool MessageUtils::isPrivateIP(const std::string& ip) {
    // Check for common private IP ranges
    return ip.substr(0, 4) == "192." || 
           ip.substr(0, 3) == "10." || 
           ip.substr(0, 8) == "172.16." ||
           ip.substr(0, 9) == "127.0.0.";
}

uint32_t MessageUtils::calculateChecksum(const std::string& data) {
    std::string hash = Hash::sha256(Hash::sha256(data));
    if (hash.length() < 8) return 0;
    
    // Take first 4 bytes of double SHA256
    uint32_t checksum = 0;
    for (int i = 0; i < 4; ++i) {
        checksum |= (static_cast<uint32_t>(static_cast<unsigned char>(hash[i])) << (i * 8));
    }
    return checksum;
}

bool MessageUtils::verifyChecksum(const std::string& data, uint32_t checksum) {
    return calculateChecksum(data) == checksum;
}

} // namespace pragma
