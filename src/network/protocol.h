#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <variant>
#include <memory>

namespace pragma {

// Forward declarations
struct Transaction;
struct Block;

/**
 * P2P Message types
 */
enum class MessageType : uint32_t {
    VERSION = 1,
    VERACK = 2,
    GETHEADERS = 3,
    HEADERS = 4,
    GETDATA = 5,
    INV = 6,
    PING = 7,
    PONG = 8,
    TX = 9,
    BLOCK = 10,
    HEADERS_NEW = 11,
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
 * Inventory vector for referencing objects
 */
struct InventoryVector {
    InventoryType type;
    std::string hash;
    
    InventoryVector() = default;
    InventoryVector(InventoryType t, const std::string& h) : type(t), hash(h) {}
    
    std::vector<uint8_t> serialize() const;
    static InventoryVector deserialize(const std::vector<uint8_t>& data, size_t& offset);
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
    
    std::vector<uint8_t> serialize() const;
    static NetworkAddress deserialize(const std::vector<uint8_t>& data, size_t& offset);
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
    
    MessageHeader() = default;
    MessageHeader(uint32_t m, MessageType cmd, uint32_t len = 0, uint32_t cs = 0)
        : magic(m), command(cmd), length(len), checksum(cs) {}
    
    std::vector<uint8_t> serialize() const;
    static MessageHeader deserialize(const std::vector<uint8_t>& data, size_t& offset);
};

/**
 * Version message for protocol negotiation
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
    
    VersionMessage() = default;
    
    std::vector<uint8_t> serialize() const;
    static VersionMessage deserialize(const std::vector<uint8_t>& data, size_t& offset);
};

/**
 * Ping/Pong message for keep-alive
 */
struct PingMessage {
    uint64_t nonce;
    
    PingMessage() = default;
    PingMessage(uint64_t n) : nonce(n) {}
    
    std::vector<uint8_t> serialize() const;
    static PingMessage deserialize(const std::vector<uint8_t>& data, size_t& offset);
};

struct PongMessage {
    uint64_t nonce;
    
    PongMessage() = default;
    PongMessage(uint64_t n) : nonce(n) {}
    
    std::vector<uint8_t> serialize() const;
    static PongMessage deserialize(const std::vector<uint8_t>& data, size_t& offset);
};

/**
 * Inventory message
 */
struct InvMessage {
    std::vector<InventoryVector> inventory;
    
    InvMessage() = default;
    
    std::vector<uint8_t> serialize() const;
    static InvMessage deserialize(const std::vector<uint8_t>& data, size_t& offset);
};

/**
 * GetData message
 */
struct GetDataMessage {
    std::vector<InventoryVector> inventory;
    
    GetDataMessage() = default;
    
    std::vector<uint8_t> serialize() const;
    static GetDataMessage deserialize(const std::vector<uint8_t>& data, size_t& offset);
};

/**
 * Transaction message
 */
struct TxMessage {
    std::shared_ptr<Transaction> transaction;
    
    TxMessage() = default;
    
    std::vector<uint8_t> serialize() const;
    static TxMessage deserialize(const std::vector<uint8_t>& data, size_t& offset);
};

/**
 * Block message
 */
struct BlockMessage {
    std::shared_ptr<Block> block;
    
    BlockMessage() = default;
    
    std::vector<uint8_t> serialize() const;
    static BlockMessage deserialize(const std::vector<uint8_t>& data, size_t& offset);
};

/**
 * GetHeaders message
 */
struct GetHeadersMessage {
    uint32_t version;
    std::vector<std::string> locatorHashes;
    std::string stopHash;
    
    GetHeadersMessage() = default;
    
    std::vector<uint8_t> serialize() const;
    static GetHeadersMessage deserialize(const std::vector<uint8_t>& data, size_t& offset);
};

/**
 * Headers message
 */
struct HeadersMessage {
    std::vector<std::shared_ptr<Block>> headers;
    
    HeadersMessage() = default;
    
    std::vector<uint8_t> serialize() const;
    static HeadersMessage deserialize(const std::vector<uint8_t>& data, size_t& offset);
};

/**
 * Addr message for peer address sharing
 */
struct AddrMessage {
    std::vector<NetworkAddress> addresses;
    
    AddrMessage() = default;
    
    std::vector<uint8_t> serialize() const;
    static AddrMessage deserialize(const std::vector<uint8_t>& data, size_t& offset);
};

/**
 * Reject message for protocol violations
 */
struct RejectMessage {
    std::string message;
    uint8_t code;
    std::string reason;
    std::vector<uint8_t> data;
    
    RejectMessage() = default;
    
    std::vector<uint8_t> serialize() const;
    static RejectMessage deserialize(const std::vector<uint8_t>& data, size_t& offset);
};

/**
 * Main P2P message container
 */
struct P2PMessage {
    MessageHeader header;
    std::variant<
        VersionMessage,
        std::monostate,  // For VERACK (no payload)
        InvMessage,
        GetDataMessage,
        TxMessage,
        BlockMessage,
        GetHeadersMessage,
        HeadersMessage,
        PingMessage,
        PongMessage,
        AddrMessage,
        RejectMessage
    > data;
    
    P2PMessage() = default;
    
    std::vector<uint8_t> serialize() const;
    static P2PMessage deserialize(const std::vector<uint8_t>& data, size_t& offset);
    
    // Factory methods for creating specific message types
    static std::shared_ptr<P2PMessage> createVersion(const VersionMessage& version);
    static std::shared_ptr<P2PMessage> createVerack();
    static std::shared_ptr<P2PMessage> createInv(const std::vector<InventoryVector>& inv);
    static std::shared_ptr<P2PMessage> createGetData(const std::vector<InventoryVector>& inv);
    static std::shared_ptr<P2PMessage> createTx(std::shared_ptr<Transaction> tx);
    static std::shared_ptr<P2PMessage> createBlock(std::shared_ptr<Block> block);
    static std::shared_ptr<P2PMessage> createGetHeaders(const GetHeadersMessage& getHeaders);
    static std::shared_ptr<P2PMessage> createHeaders(const std::vector<std::shared_ptr<Block>>& headers);
    static std::shared_ptr<P2PMessage> createPing(uint64_t nonce);
    static std::shared_ptr<P2PMessage> createPong(uint64_t nonce);
    static std::shared_ptr<P2PMessage> createAddr(const std::vector<NetworkAddress>& addresses);
    static std::shared_ptr<P2PMessage> createReject(const RejectMessage& reject);
    
    // Message type checking
    bool isVersion() const;
    bool isVerack() const;
    bool isInv() const;
    bool isGetData() const;
    bool isTx() const;
    bool isBlock() const;
    bool isGetHeaders() const;
    bool isHeaders() const;
    bool isPing() const;
    bool isPong() const;
    bool isAddr() const;
    bool isReject() const;
    
    // Payload extraction (with type safety)
    const VersionMessage* getVersion() const;
    const InvMessage* getInv() const;
    const GetDataMessage* getGetData() const;
    const TxMessage* getTx() const;
    const BlockMessage* getBlock() const;
    const GetHeadersMessage* getGetHeaders() const;
    const HeadersMessage* getHeaders() const;
    const PingMessage* getPing() const;
    const PongMessage* getPong() const;
    const AddrMessage* getAddr() const;
    const RejectMessage* getReject() const;
};

/**
 * P2P Protocol constants
 */
namespace Protocol {
    constexpr uint32_t MAGIC_BYTES = 0xF9BEB4D9;
    constexpr uint32_t PROTOCOL_VERSION = 70015;
    constexpr uint64_t NODE_NETWORK = 1;
    constexpr size_t MAX_MESSAGE_SIZE = 32 * 1024 * 1024; // 32MB
    constexpr size_t MAX_INV_SIZE = 50000;
    constexpr size_t MAX_ADDR_SIZE = 1000;
    
    // Message commands as strings
    const std::string CMD_VERSION = "version";
    const std::string CMD_VERACK = "verack";
    const std::string CMD_INV = "inv";
    const std::string CMD_GETDATA = "getdata";
    const std::string CMD_TX = "tx";
    const std::string CMD_BLOCK = "block";
    const std::string CMD_GETHEADERS = "getheaders";
    const std::string CMD_HEADERS = "headers";
    const std::string CMD_PING = "ping";
    const std::string CMD_PONG = "pong";
    const std::string CMD_ADDR = "addr";
    const std::string CMD_GETADDR = "getaddr";
    const std::string CMD_REJECT = "reject";
}

} // namespace pragma
