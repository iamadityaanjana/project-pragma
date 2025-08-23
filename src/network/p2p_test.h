#pragma once

#include "../core/chainstate.h"
#include "../core/mempool.h"
#include "../core/transaction.h"
#include "../core/block.h"
#include "../core/utxo.h"
#include "../core/validator.h"
#include "../network/p2p.h"
#include "../primitives/utils.h"
#include <memory>
#include <thread>
#include <atomic>
#include <string>
#include <vector>

namespace pragma {

/**
 * P2P Test Node - represents a single blockchain node for testing
 */
class P2PTestNode {
private:
    uint16_t port;
    std::string nodeId;
    std::atomic<bool> running;
    
    // Core blockchain components
    std::unique_ptr<ChainState> chainState;
    std::unique_ptr<Mempool> mempool;
    std::unique_ptr<P2PNetwork> p2pNetwork;
    
    // Threading
    std::thread networkThread;

public:
    P2PTestNode(uint16_t port, const std::string& nodeId);
    ~P2PTestNode();
    
    // Node lifecycle
    void start();
    void stop();
    bool isRunning() const { return running; }
    
    // Network operations
    void connectToPeer(const std::string& address, uint16_t peerPort);
    void broadcastTransaction(const Transaction& tx);
    void mineBlock();
    
    // Status and info
    P2PNetwork::NetworkInfo getNetworkInfo() const;
    std::string getStatus() const;
    
    // Getters for testing
    ChainState* getChainState() const { return chainState.get(); }
    Mempool* getMempool() const { return mempool.get(); }
    P2PNetwork* getP2PNetwork() const { return p2pNetwork.get(); }
    uint16_t getPort() const { return port; }
    const std::string& getNodeId() const { return nodeId; }
};

/**
 * P2P Test Suite - comprehensive testing of P2P functionality
 */
class P2PTestSuite {
public:
    // Individual test scenarios
    static void runBasicConnectivityTest();
    static void runTransactionBroadcastTest();
    static void runBlockSyncTest();
    static void runStressTest();
    
    // Run all tests
    static void runAllTests();
    
private:
    // Helper methods
    static void waitForConnection(P2PTestNode& node1, P2PTestNode& node2, int timeoutSeconds = 10);
    static void waitForSync(P2PTestNode& node1, P2PTestNode& node2, int timeoutSeconds = 15);
    static bool checkTransactionPropagation(const std::vector<P2PTestNode*>& nodes, const std::string& txid);
};

/**
 * Network Simulation Helpers
 */
class NetworkSimulator {
public:
    // Simulate network latency
    static void simulateLatency(int milliseconds);
    
    // Simulate packet loss
    static bool simulatePacketLoss(double lossRate = 0.05);
    
    // Create network topologies
    static void createMeshNetwork(std::vector<P2PTestNode*>& nodes);
    static void createRingNetwork(std::vector<P2PTestNode*>& nodes);
    static void createStarNetwork(std::vector<P2PTestNode*>& nodes);
    
    // Network partitioning
    static void partitionNetwork(std::vector<P2PTestNode*>& groupA, std::vector<P2PTestNode*>& groupB);
    static void reconnectNetwork(std::vector<P2PTestNode*>& groupA, std::vector<P2PTestNode*>& groupB);
};

} // namespace pragma
