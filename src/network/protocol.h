#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <chrono>
#include <functional>
#include <unordered_map>
#include <unordered_set>

namespace pragma {

// Forward declarations
class Block;
class Transaction;

/**
 * P2P Message types
 */
enum class MessageType : uint32_t {
    VERSION = 1,
    VERACK = 2,
    PING = 3,
    PONG = 4,
    INV = 5,
    GETDATA = 6,
    GETBLOCKS = 7,
    GETHEADERS = 8,
    TX = 9,
    BLOCK = 10,
    HEADERS = 11,
    ADDR = 12,
    GETADDR = 13,
    REJECT = 14,
    MEMPOOL = 15,
    NOTFOUND = 16
};

/**
 * Inventory types for INV messages
 */
enum class InventoryType : uint32_t {
    ERROR = 0,
    TX = 1,
    BLOCK = 2,
    FILTERED_BLOCK = 3,
    COMPACT_BLOCK = 4
};

/**
 * Inventory vector entry
 */
struct InventoryVector {
    InventoryType type;
    std::string hash;
    
    InventoryVector() = default;
    InventoryVector(InventoryType t, const std::string& h) : type(t), hash(h) {}
    
    std::string serialize() const;
    static InventoryVector deserialize(const std::string& data, size_t& offset);
};

/**
 * Network address structure
 */
struct NetworkAddress {
    uint64_t services;
    std::string ip;
    uint16_t port;
    uint64_t timestamp;
    
    NetworkAddress() = default;
    NetworkAddress(const std::string& addr, uint16_t p, uint64_t serv = 1)
        : services(serv), ip(addr), port(p), timestamp(0) {}
    
    std::string serialize() const;
    static NetworkAddress deserialize(const std::string& data, size_t& offset);
    std::string toString() const;
};

/**
 * P2P Message header
 */
struct MessageHeader {
    uint32_t magic;
    MessageType command;
    uint32_t length;
    uint32_t checksum;
    
    static const uint32_t MAGIC_MAIN = 0xD9B4BEF9;
    static const uint32_t HEADER_SIZE = 24;
    
    std::string serialize() const;
    static MessageHeader deserialize(const std::string& data);
    bool isValid() const;
};

/**
 * Version message payload
 */
struct VersionMessage {
    uint32_t version;
    uint64_t services;
    uint64_t timestamp;
    NetworkAddress addrRecv;
    NetworkAddress addrFrom;
    uint64_t nonce;
    std::string userAgent;
    uint32_t startHeight;
    bool relay;
    
    static const uint32_t PROTOCOL_VERSION = 70015;
    
    std::string serialize() const;
    static VersionMessage deserialize(const std::string& data);
};

/**
 * Ping/Pong message payload
 */
struct PingPongMessage {
    uint64_t nonce;
    
    std::string serialize() const;
    static PingPongMessage deserialize(const std::string& data);
};

/**
 * INV message payload
 */
struct InvMessage {
    std::vector<InventoryVector> inventory;
    
    std::string serialize() const;
    static InvMessage deserialize(const std::string& data);
};

/**
 * GetData message payload (same structure as INV)
 */
using GetDataMessage = InvMessage;

/**
 * GetHeaders message payload
 */
struct GetHeadersMessage {
    uint32_t version;
    std::vector<std::string> locatorHashes;
    std::string hashStop;
    
    std::string serialize() const;
    static GetHeadersMessage deserialize(const std::string& data);
};

/**
 * Headers message payload
 */
struct HeadersMessage {
    std::vector<std::string> headers; // Serialized block headers
    
    std::string serialize() const;
    static HeadersMessage deserialize(const std::string& data);
};

/**
 * Addr message payload
 */
struct AddrMessage {
    std::vector<NetworkAddress> addresses;
    
    std::string serialize() const;
    static AddrMessage deserialize(const std::string& data);
};

/**
 * Reject message payload
 */
struct RejectMessage {
    std::string message;
    uint8_t code;
    std::string reason;
    std::string data;
    
    enum RejectCode : uint8_t {
        MALFORMED = 0x01,
        INVALID = 0x10,
        OBSOLETE = 0x11,
        DUPLICATE = 0x12,
        NONSTANDARD = 0x40,
        DUST = 0x41,
        INSUFFICIENTFEE = 0x42,
        CHECKPOINT = 0x43
    };
    
    std::string serialize() const;
    static RejectMessage deserialize(const std::string& data);
};

/**
 * Generic P2P message container
 */
class P2PMessage {
public:
    MessageHeader header;
    std::string payload;
    
    P2PMessage() = default;
    P2PMessage(MessageType type, const std::string& data);
    
    std::string serialize() const;
    static std::shared_ptr<P2PMessage> deserialize(const std::string& data);
    
    bool isValid() const;
    uint32_t calculateChecksum(const std::string& data) const;
    
    // Helper methods for specific message types
    VersionMessage getVersionMessage() const;
    PingPongMessage getPingPongMessage() const;
    InvMessage getInvMessage() const;
    GetDataMessage getGetDataMessage() const;
    GetHeadersMessage getGetHeadersMessage() const;
    HeadersMessage getHeadersMessage() const;
    AddrMessage getAddrMessage() const;
    RejectMessage getRejectMessage() const;
    
    // Factory methods
    static std::shared_ptr<P2PMessage> createVersion(const VersionMessage& version);
    static std::shared_ptr<P2PMessage> createVerAck();
    static std::shared_ptr<P2PMessage> createPing(uint64_t nonce);
    static std::shared_ptr<P2PMessage> createPong(uint64_t nonce);
    static std::shared_ptr<P2PMessage> createInv(const std::vector<InventoryVector>& inv);
    static std::shared_ptr<P2PMessage> createGetData(const std::vector<InventoryVector>& inv);
    static std::shared_ptr<P2PMessage> createGetHeaders(const GetHeadersMessage& getHeaders);
    static std::shared_ptr<P2PMessage> createHeaders(const std::vector<std::string>& headers);
    static std::shared_ptr<P2PMessage> createTx(const Transaction& tx);
    static std::shared_ptr<P2PMessage> createBlock(const Block& block);
    static std::shared_ptr<P2PMessage> createAddr(const std::vector<NetworkAddress>& addresses);
    static std::shared_ptr<P2PMessage> createReject(const RejectMessage& reject);
};

/**
 * Message type utilities
 */
class MessageUtils {
public:
    static std::string messageTypeToString(MessageType type);
    static MessageType stringToMessageType(const std::string& str);
    static std::string inventoryTypeToString(InventoryType type);
    static InventoryType stringToInventoryType(const std::string& str);
    
    // Validation helpers
    static bool isValidMessageType(uint32_t type);
    static bool isValidInventoryType(uint32_t type);
    static size_t getExpectedPayloadSize(MessageType type);
    
    // Network utilities
    static std::string formatAddress(const std::string& ip, uint16_t port);
    static bool parseAddress(const std::string& address, std::string& ip, uint16_t& port);
    static bool isValidIP(const std::string& ip);
    static bool isPrivateIP(const std::string& ip);
    
    // Checksum and validation
    static uint32_t calculateChecksum(const std::string& data);
    static bool verifyChecksum(const std::string& data, uint32_t checksum);
};

} // namespace pragma
