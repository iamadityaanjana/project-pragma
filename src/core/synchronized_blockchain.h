#pragma once

#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <functional>
#include "../core/chainstate.h"
#include "../core/mempool.h"
#include "../wallet/wallet.h"
#include "../network/p2p.h"

namespace pragma {

/**
 * Synchronized blockchain manager that ensures consistent state across all nodes
 * Implements Bitcoin-like functionality with proper consensus and synchronization
 */
class SynchronizedBlockchainManager {
public:
    struct NodeConfig {
        std::string nodeId;
        uint16_t rpcPort = 8332;
        uint16_t p2pPort = 8333;
        std::string dataDir = "./pragma_data";
        std::vector<std::string> seedNodes;
        bool enableMining = true;
        std::string minerAddress;
        double blockReward = 50.0;
    };

    struct NetworkStats {
        uint32_t connectedPeers = 0;
        uint32_t blockHeight = 0;
        std::string bestBlockHash;
        bool synchronized = false;
        uint64_t totalSupply = 0;
        uint32_t mempoolSize = 0;
    };

private:
    // Core components
    std::shared_ptr<ChainState> chainState_;
    std::shared_ptr<Mempool> mempool_;
    std::shared_ptr<WalletManager> walletManager_;
    std::shared_ptr<P2PNetwork> p2pNetwork_;
    
    // Configuration
    NodeConfig config_;
    
    // Synchronization
    mutable std::mutex stateMutex_;
    std::atomic<bool> running_{false};
    std::atomic<bool> synchronized_{false};
    std::atomic<bool> mining_{false};
    
    // Threading
    std::thread syncThread_;
    std::thread miningThread_;
    std::condition_variable syncCondition_;
    
    // Network state
    std::atomic<uint32_t> connectedPeers_{0};
    std::atomic<uint64_t> totalSupply_{0};
    
    // Mining
    std::atomic<bool> stopMining_{false};
    uint32_t miningDifficulty_ = 1;
    
    // Internal methods
    void syncLoop();
    void miningLoop();
    void processNewBlock(const Block& block);
    void processNewTransaction(const Transaction& tx);
    void broadcastBlock(const Block& block);
    void broadcastTransaction(const Transaction& tx);
    bool validateAndAddBlock(const Block& block);
    void updateTotalSupply();
    Block createCoinbaseBlock(const std::string& minerAddress, double reward);
    
public:
    explicit SynchronizedBlockchainManager(const NodeConfig& config);
    ~SynchronizedBlockchainManager();
    
    // Lifecycle management
    bool initialize();
    bool start();
    void stop();
    bool isRunning() const { return running_; }
    bool isSynchronized() const { return synchronized_; }
    
    // Blockchain operations
    bool addBlock(const Block& block);
    bool addTransaction(const Transaction& tx);
    std::string sendTransaction(const std::string& fromAddress, 
                               const std::string& toAddress, 
                               double amount);
    
    // Mining operations
    void startMining(const std::string& minerAddress);
    void stopMining();
    bool isMining() const { return mining_; }
    Block mineBlock(const std::string& minerAddress, double reward = 50.0);
    
    // Balance and query operations
    double getBalance(const std::string& address) const;
    std::vector<Transaction> getTransactionHistory(const std::string& address) const;
    std::vector<std::string> listWalletAddresses() const;
    std::string createNewAddress();
    
    // Network operations
    bool connectToPeer(const std::string& address, uint16_t port);
    void disconnectFromPeer(const std::string& peerId);
    NetworkStats getNetworkStats() const;
    std::vector<std::string> getConnectedPeers() const;
    
    // Synchronization operations
    void requestBlockchainSync();
    void syncWithPeers();
    bool isChainUpToDate() const;
    
    // Wallet operations
    std::string generateNewWallet(const std::string& name);
    bool loadWallet(const std::string& name);
    std::vector<std::string> listWallets() const;
    
    // Blockchain queries
    uint32_t getBlockHeight() const;
    std::string getBestBlockHash() const;
    Block getBlock(const std::string& hash) const;
    Block getBlockByHeight(uint32_t height) const;
    Transaction getTransaction(const std::string& txHash) const;
    
    // Network callbacks (for P2P events)
    void onPeerConnected(const std::string& peerId);
    void onPeerDisconnected(const std::string& peerId);
    void onBlockReceived(const Block& block, const std::string& peerId);
    void onTransactionReceived(const Transaction& tx, const std::string& peerId);
    
    // Configuration
    void setMinerAddress(const std::string& address) { config_.minerAddress = address; }
    void setBlockReward(double reward) { config_.blockReward = reward; }
    NodeConfig getConfig() const { return config_; }
    
    // Persistence
    bool saveState() const;
    bool loadState();
    
    // Debug and monitoring
    void printChainState() const;
    void printNetworkState() const;
    std::string getNodeInfo() const;
};

/**
 * Global synchronized blockchain instance manager
 * Ensures single instance per node and provides global access
 */
class GlobalBlockchainManager {
private:
    static std::unique_ptr<SynchronizedBlockchainManager> instance_;
    static std::mutex instanceMutex_;
    
public:
    static bool initialize(const SynchronizedBlockchainManager::NodeConfig& config);
    static SynchronizedBlockchainManager* getInstance();
    static void shutdown();
    static bool isInitialized();
};

} // namespace pragma
