#pragma once

#include "peer.h"
#include "protocol.h"
#include "../core/block.h"
#include "../core/transaction.h"
#include "../core/chainstate.h"
#include "../core/mempool.h"
#include <thread>
#include <atomic>
#include <condition_variable>
#include <functional>

namespace pragma {

/**
 * Synchronization state
 */
enum class SyncState {
    IDLE,
    HEADERS_SYNC,
    BLOCKS_SYNC,
    MEMPOOL_SYNC,
    SYNCED
};

/**
 * Network configuration
 */
struct NetworkConfig {
    uint16_t port = 8333;
    std::string userAgent = "/Pragma:1.0.0/";
    uint32_t protocolVersion = 70015;
    uint64_t services = 1; // NODE_NETWORK
    bool listen = true;
    bool relay = true;
    size_t maxConnections = 125;
    size_t maxInbound = 100;
    size_t maxOutbound = 25;
    std::chrono::seconds connectTimeout{30};
    std::chrono::seconds handshakeTimeout{60};
    std::chrono::seconds pingInterval{60};
    std::vector<std::string> seedNodes;
    std::vector<std::string> addNodes;
    std::vector<std::string> trustedNodes;
};

/**
 * Sync manager - handles blockchain synchronization
 */
class SyncManager {
private:
    ChainState* chainState;
    PeerManager* peerManager;
    SyncState state;
    std::string syncPeer;
    uint32_t targetHeight;
    uint32_t currentHeight;
    std::chrono::steady_clock::time_point syncStartTime;
    
    mutable std::mutex syncMutex;
    
public:
    SyncManager(ChainState* chain, PeerManager* peers);
    ~SyncManager() = default;
    
    // Sync control
    void startSync();
    void stopSync();
    bool isSyncing() const;
    bool isSynced() const;
    SyncState getState() const;
    
    // Sync operations
    void requestHeaders(const std::string& peerId);
    void requestBlocks(const std::string& peerId, const std::vector<std::string>& hashes);
    void handleHeaders(const std::string& peerId, const std::vector<std::string>& headers);
    void handleBlock(const std::string& peerId, const Block& block);
    
    // Sync status
    double getSyncProgress() const;
    std::string getSyncPeer() const;
    uint32_t getTargetHeight() const;
    std::chrono::seconds getSyncDuration() const;
    
    // Peer selection for sync
    std::string selectSyncPeer();
    void updateSyncPeer();
};

/**
 * Main P2P Network Manager
 */
class P2PNetwork : public NetworkEventHandler {
private:
    NetworkConfig config;
    std::unique_ptr<PeerManager> peerManager;
    std::unique_ptr<SyncManager> syncManager;
    
    // Core components (external references)
    ChainState* chainState;
    Mempool* mempool;
    
    // Network state
    std::atomic<bool> running{false};
    std::atomic<bool> listening{false};
    
    // Threading
    std::thread networkThread;
    std::thread syncThread;
    std::thread messageThread;
    std::condition_variable networkCV;
    std::mutex networkMutex;
    
    // Message handling
    std::queue<std::pair<std::string, std::shared_ptr<P2PMessage>>> messageQueue;
    std::mutex messageQueueMutex;
    std::condition_variable messageCV;
    
    // Ping management
    std::chrono::steady_clock::time_point lastPingTime;
    
    // Connection management
    void networkLoop();
    void syncLoop();
    void messageLoop();
    void connectToPeers();
    void maintainConnections();
    void sendPings();
    
    // Message processing
    void processMessage(const std::string& peerId, std::shared_ptr<P2PMessage> message);
    void handleVersionMessage(const std::string& peerId, const VersionMessage& version);
    void handleVerackMessage(const std::string& peerId);
    void handlePingMessage(const std::string& peerId, const PingMessage& ping);
    void handlePongMessage(const std::string& peerId, const PongMessage& pong);
    void handleInvMessage(const std::string& peerId, const InvMessage& inv);
    void handleGetDataMessage(const std::string& peerId, const GetDataMessage& getData);
    void handleTxMessage(const std::string& peerId, const std::string& txData);
    void handleBlockMessage(const std::string& peerId, const std::string& blockData);
    void handleGetHeadersMessage(const std::string& peerId, const GetHeadersMessage& getHeaders);
    void handleHeadersMessage(const std::string& peerId, const HeadersMessage& headers);
    void handleAddrMessage(const std::string& peerId, const AddrMessage& addr);
    void handleRejectMessage(const std::string& peerId, const RejectMessage& reject);
    
    // Handshake management
    void initiateHandshake(const std::string& peerId);
    void completeHandshake(const std::string& peerId);
    VersionMessage createVersionMessage(const NetworkAddress& remoteAddr);
    
    // Inventory management
    void announceTransaction(const Transaction& tx);
    void announceBlock(const Block& block);
    void requestInventory(const std::string& peerId, const std::vector<InventoryVector>& inventory);
    
public:
    P2PNetwork(const NetworkConfig& cfg, ChainState* chain, Mempool* mp);
    ~P2PNetwork();
    
    // Network lifecycle
    bool start();
    void stop();
    bool isRunning() const { return running.load(); }
    bool isListening() const { return listening.load(); }
    
    // Configuration
    const NetworkConfig& getConfig() const { return config; }
    void updateConfig(const NetworkConfig& newConfig);
    
    // Peer management
    PeerManager* getPeerManager() { return peerManager.get(); }
    const PeerManager* getPeerManager() const { return peerManager.get(); }
    
    // Sync management
    SyncManager* getSyncManager() { return syncManager.get(); }
    const SyncManager* getSyncManager() const { return syncManager.get(); }
    
    // Manual peer operations
    bool connectToPeer(const std::string& address);
    void disconnectPeer(const std::string& peerId);
    void banPeer(const std::string& peerId, const std::string& reason = "");
    
    // Broadcasting
    void broadcastTransaction(const Transaction& tx);
    void broadcastBlock(const Block& block);
    void relayInventory(const std::vector<InventoryVector>& inventory, const std::string& excludePeer = "");
    
    // Network information
    struct NetworkInfo {
        bool running;
        bool listening;
        uint16_t port;
        size_t connections;
        size_t inbound;
        size_t outbound;
        SyncState syncState;
        double syncProgress;
        std::string bestPeer;
        uint64_t bytesReceived;
        uint64_t bytesSent;
        uint64_t messagesReceived;
        uint64_t messagesSent;
    };
    
    NetworkInfo getNetworkInfo() const;
    
    // Address management
    void addAddress(const NetworkAddress& address);
    std::vector<NetworkAddress> getKnownAddresses() const;
    void shareAddresses(const std::string& peerId);
    
    // Events (NetworkEventHandler implementation)
    void onPeerConnected(const std::string& peerId) override;
    void onPeerDisconnected(const std::string& peerId) override;
    void onPeerHandshakeComplete(const std::string& peerId) override;
    void onPeerBanned(const std::string& peerId, const std::string& reason) override;
    void onMessageReceived(const std::string& peerId, std::shared_ptr<P2PMessage> message) override;
    void onInvalidMessage(const std::string& peerId, const std::string& reason) override;
    void onInventoryReceived(const std::string& peerId, const std::vector<InventoryVector>& inventory) override;
    void onTransactionReceived(const std::string& peerId, const Transaction& tx) override;
    void onBlockReceived(const std::string& peerId, const Block& block) override;
    void onHeadersReceived(const std::string& peerId, const std::vector<std::string>& headers) override;
    void onSyncStarted(const std::string& peerId) override;
    void onSyncCompleted(const std::string& peerId) override;
    
    // Statistics and debugging
    void printNetworkStatus() const;
    void printPeerList() const;
    std::string getNetworkStatusString() const;
    
    // Simulation helpers (for testing)
    void simulateConnect(const std::string& address);
    void simulateMessage(const std::string& peerId, std::shared_ptr<P2PMessage> message);
    void simulateHandshake(const std::string& peerId);
    
private:
    // Address book for peer discovery
    std::vector<NetworkAddress> knownAddresses;
    mutable std::mutex addressMutex;
    
    // Utility methods
    NetworkAddress stringToAddress(const std::string& addressStr);
    bool isValidPeerAddress(const NetworkAddress& address);
    void cleanupNetwork();
};

/**
 * P2P Network Factory - creates configured network instances
 */
class P2PNetworkFactory {
public:
    static std::unique_ptr<P2PNetwork> createMainnet(ChainState* chainState, Mempool* mempool);
    static std::unique_ptr<P2PNetwork> createTestnet(ChainState* chainState, Mempool* mempool);
    static std::unique_ptr<P2PNetwork> createRegtest(ChainState* chainState, Mempool* mempool);
    static std::unique_ptr<P2PNetwork> createSimulation(ChainState* chainState, Mempool* mempool);
    
    static NetworkConfig getMainnetConfig();
    static NetworkConfig getTestnetConfig();
    static NetworkConfig getRegtestConfig();
    static NetworkConfig getSimulationConfig();
};

} // namespace pragma
