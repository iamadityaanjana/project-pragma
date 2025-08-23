#pragma once

#include "protocol.h"
#include <memory>
#include <chrono>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <atomic>
#include <functional>
#include <atomic>
#include <mutex>

namespace pragma {

// Forward declarations
struct Block;
struct Transaction;

/**
 * Peer connection state
 */
enum class PeerState {
    CONNECTING,
    CONNECTED,
    HANDSHAKING,
    HANDSHAKE_COMPLETE,
    SYNCING,
    READY,
    DISCONNECTING,
    DISCONNECTED,
    BANNED
};

/**
 * Peer statistics
 */
struct PeerStats {
    uint64_t bytesReceived;
    uint64_t bytesSent;
    uint64_t messagesReceived;
    uint64_t messagesSent;
    uint64_t invalidMessages;
    uint64_t lastSeen;
    uint64_t connectionTime;
    double latency; // milliseconds
    uint32_t banScore;
    
    PeerStats() : bytesReceived(0), bytesSent(0), messagesReceived(0), messagesSent(0),
                  invalidMessages(0), lastSeen(0), connectionTime(0), latency(0.0), banScore(0) {}
};

/**
 * Peer information and management
 */
class Peer {
private:
    std::string id;
    NetworkAddress address;
    std::atomic<PeerState> state;
    PeerStats stats;
    
    // Protocol information
    uint32_t version;
    uint64_t services;
    uint32_t startHeight;
    std::string userAgent;
    uint64_t nonce;
    bool relay;
    
    // Handshake tracking
    bool versionSent;
    bool versionReceived;
    bool verackSent;
    bool verackReceived;
    
    // Message queues
    std::queue<std::shared_ptr<P2PMessage>> outboundQueue;
    std::queue<std::shared_ptr<P2PMessage>> inboundQueue;
    
    // Rate limiting
    std::chrono::steady_clock::time_point lastMessageTime;
    uint32_t messageRate; // messages per second
    
    // Ping tracking
    std::unordered_map<uint64_t, std::chrono::steady_clock::time_point> pendingPings;
    
    // Inventory tracking
    std::unordered_set<std::string> knownInventory;
    std::unordered_set<std::string> requestedInventory;
    
    mutable std::mutex peerMutex;
    
public:
    Peer(const std::string& peerId, const NetworkAddress& addr);
    ~Peer() = default;
    
    // Basic getters
    const std::string& getId() const { return id; }
    const NetworkAddress& getAddress() const { return address; }
    PeerState getState() const { return state.load(); }
    // Get peer statistics (return by value to avoid mutex issues)
    PeerStats getStats() const { 
        std::lock_guard<std::mutex> lock(peerMutex); 
        return stats; 
    }
    
    // Protocol information
    uint32_t getVersion() const { return version; }
    uint64_t getServices() const { return services; }
    uint32_t getStartHeight() const { return startHeight; }
    const std::string& getUserAgent() const { return userAgent; }
    bool getRelay() const { return relay; }
    
    // State management
    void setState(PeerState newState);
    bool isConnected() const;
    bool isHandshakeComplete() const;
    bool isReady() const;
    bool isBanned() const;
    
    // Handshake management
    void setVersionInfo(const VersionMessage& version);
    void markVersionSent() { versionSent = true; }
    void markVersionReceived() { versionReceived = true; }
    void markVerackSent() { verackSent = true; }
    void markVerackReceived() { verackReceived = true; }
    bool needsVersionSent() const { return !versionSent; }
    bool needsVerackSent() const { return versionReceived && !verackSent; }
    bool isHandshakeReady() const { return versionSent && versionReceived && verackSent && verackReceived; }
    
    // Message handling
    void queueOutboundMessage(std::shared_ptr<P2PMessage> message);
    std::shared_ptr<P2PMessage> getNextOutboundMessage();
    void queueInboundMessage(std::shared_ptr<P2PMessage> message);
    std::shared_ptr<P2PMessage> getNextInboundMessage();
    bool hasOutboundMessages() const;
    bool hasInboundMessages() const;
    size_t getOutboundQueueSize() const;
    size_t getInboundQueueSize() const;
    
    // Statistics
    void updateStats(uint64_t bytesReceived, uint64_t bytesSent);
    void incrementMessageCount(bool inbound);
    void incrementInvalidMessages();
    void updateLastSeen();
    void increaseBanScore(uint32_t points);
    void resetBanScore();
    
    // Rate limiting
    bool isRateLimited() const;
    void updateMessageRate();
    
    // Ping management
    void addPendingPing(uint64_t nonce);
    bool handlePong(uint64_t nonce);
    double getLatency() const { return stats.latency; }
    
    // Inventory management
    bool hasInventory(const std::string& hash) const;
    void addKnownInventory(const std::string& hash);
    void addRequestedInventory(const std::string& hash);
    void removeRequestedInventory(const std::string& hash);
    bool isInventoryRequested(const std::string& hash) const;
    size_t getKnownInventorySize() const;
    size_t getRequestedInventorySize() const;
    void clearOldInventory(size_t maxSize = 10000);
    
    // Utility
    std::string toString() const;
    double getConnectionDuration() const;
    bool shouldBan() const;
};

/**
 * Peer manager - handles multiple peer connections
 */
class PeerManager {
private:
    std::unordered_map<std::string, std::shared_ptr<Peer>> peers;
    std::unordered_map<std::string, std::string> addressToPeerId;
    
    // Configuration
    size_t maxPeers;
    size_t maxInboundPeers;
    size_t maxOutboundPeers;
    uint32_t banThreshold;
    std::chrono::seconds banDuration;
    uint32_t maxMessageRate;
    
    // Connection tracking
    std::atomic<size_t> connectedPeers{0};
    std::atomic<size_t> inboundConnections{0};
    std::atomic<size_t> outboundConnections{0};
    
    // Banned peers
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> bannedPeers;
    
    mutable std::mutex managerMutex;
    
    // Helper methods
    std::string generatePeerId(const NetworkAddress& address);
    void cleanupBannedPeers();
    void enforceConnectionLimits();
    
public:
    PeerManager(size_t maxPeers = 125, size_t maxInbound = 100, size_t maxOutbound = 25);
    ~PeerManager() = default;
    
    // Peer lifecycle
    std::shared_ptr<Peer> addPeer(const NetworkAddress& address, bool inbound = false);
    bool removePeer(const std::string& peerId);
    void disconnectPeer(const std::string& peerId);
    void banPeer(const std::string& peerId, const std::string& reason = "");
    bool isBanned(const NetworkAddress& address) const;
    
    // Peer access
    std::shared_ptr<Peer> getPeer(const std::string& peerId);
    std::shared_ptr<Peer> getPeerByAddress(const NetworkAddress& address);
    std::vector<std::shared_ptr<Peer>> getAllPeers() const;
    std::vector<std::shared_ptr<Peer>> getReadyPeers() const;
    std::vector<std::shared_ptr<Peer>> getConnectedPeers() const;
    
    // Connection management
    size_t getConnectedCount() const { return connectedPeers.load(); }
    size_t getInboundCount() const { return inboundConnections.load(); }
    size_t getOutboundCount() const { return outboundConnections.load(); }
    bool canAcceptInbound() const;
    bool canMakeOutbound() const;
    bool hasRoom() const;
    
    // Broadcast operations
    void broadcastMessage(std::shared_ptr<P2PMessage> message, const std::string& excludePeer = "");
    void broadcastToReady(std::shared_ptr<P2PMessage> message, const std::string& excludePeer = "");
    void sendToRandomPeers(std::shared_ptr<P2PMessage> message, size_t count);
    
    // Inventory management
    void broadcastInventory(const std::vector<InventoryVector>& inventory, const std::string& excludePeer = "");
    std::vector<std::shared_ptr<Peer>> getPeersWithoutInventory(const std::string& hash);
    
    // Statistics and monitoring
    struct ManagerStats {
        size_t totalPeers;
        size_t connectedPeers;
        size_t readyPeers;
        size_t bannedPeers;
        uint64_t totalBytesReceived;
        uint64_t totalBytesSent;
        uint64_t totalMessagesReceived;
        uint64_t totalMessagesSent;
        double averageLatency;
    };
    
    ManagerStats getStats() const;
    void cleanupPeers();
    void updatePeerStates();
    
    // Configuration
    void setMaxPeers(size_t max) { maxPeers = max; }
    void setBanThreshold(uint32_t threshold) { banThreshold = threshold; }
    void setBanDuration(std::chrono::seconds duration) { banDuration = duration; }
    void setMaxMessageRate(uint32_t rate) { maxMessageRate = rate; }
    
    size_t getMaxPeers() const { return maxPeers; }
    uint32_t getBanThreshold() const { return banThreshold; }
    std::chrono::seconds getBanDuration() const { return banDuration; }
    uint32_t getMaxMessageRate() const { return maxMessageRate; }
    
    // Debug and logging
    void printPeerStatus() const;
    std::vector<std::string> getPeerIds() const;
    std::string getManagerStatus() const;
    
private:
    // Internal helper methods (don't acquire locks)
    bool isBannedNoLock(const NetworkAddress& address) const;
    std::shared_ptr<Peer> getPeerNoLock(const std::string& peerId);
};

/**
 * Network event handler interface
 */
class NetworkEventHandler {
public:
    virtual ~NetworkEventHandler() = default;
    
    // Peer events
    virtual void onPeerConnected(const std::string& peerId) {}
    virtual void onPeerDisconnected(const std::string& peerId) {}
    virtual void onPeerHandshakeComplete(const std::string& peerId) {}
    virtual void onPeerBanned(const std::string& peerId, const std::string& reason) {}
    
    // Message events
    virtual void onMessageReceived(const std::string& peerId, std::shared_ptr<P2PMessage> message) {}
    virtual void onInvalidMessage(const std::string& peerId, const std::string& reason) {}
    
    // Inventory events
    virtual void onInventoryReceived(const std::string& peerId, const std::vector<InventoryVector>& inventory) {}
    virtual void onTransactionReceived(const std::string& peerId, const Transaction& tx) {}
    virtual void onBlockReceived(const std::string& peerId, const Block& block) {}
    
    // Sync events
    virtual void onHeadersReceived(const std::string& peerId, const std::vector<std::string>& headers) {}
    virtual void onSyncStarted(const std::string& peerId) {}
    virtual void onSyncCompleted(const std::string& peerId) {}
};

} // namespace pragma
