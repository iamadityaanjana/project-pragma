#include "src/core/synchronized_blockchain.h"
#include "src/rpc/synchronized_rpc.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>
#include <atomic>
#include <vector>

using namespace pragma;

std::atomic<bool> running{true};

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    running = false;
}

void printUsage() {
    std::cout << "\nPragma Bitcoin-like Blockchain Node\n";
    std::cout << "===================================\n";
    std::cout << "Available RPC commands:\n\n";
    
    std::cout << "Blockchain Info:\n";
    std::cout << "  getblockchaininfo    - Get blockchain information\n";
    std::cout << "  getnetworkinfo       - Get network information\n";
    std::cout << "  getpeerinfo          - Get connected peers\n";
    std::cout << "  getinfo              - Get general node information\n";
    std::cout << "  ping                 - Ping the node\n\n";
    
    std::cout << "Wallet Operations:\n";
    std::cout << "  getnewaddress        - Generate new wallet address\n";
    std::cout << "  getbalance [account] - Get balance (all addresses if no account)\n";
    std::cout << "  getaddressbalance <addr> - Get balance for specific address\n";
    std::cout << "  listaddresses        - List all addresses with balances\n\n";
    
    std::cout << "Transactions:\n";
    std::cout << "  sendtoaddress <from> <to> <amount> - Send transaction\n";
    std::cout << "  gettransaction <txhash>            - Get transaction details\n\n";
    
    std::cout << "Mining:\n";
    std::cout << "  startmining <address>  - Start mining to address\n";
    std::cout << "  stopmining            - Stop mining\n";
    std::cout << "  getmininginfo         - Get mining information\n\n";
    
    std::cout << "Blocks:\n";
    std::cout << "  getblock <hash>       - Get block by hash\n";
    std::cout << "  getblockhash <height> - Get block hash by height\n";
    std::cout << "  getbestblockhash      - Get best block hash\n";
    std::cout << "  getblockcount         - Get current block height\n\n";
    
    std::cout << "Network:\n";
    std::cout << "  addnode <address:port> add - Connect to peer\n\n";
    
    std::cout << "Example RPC calls:\n";
    std::cout << "  curl -u pragma:local -d '{\"method\":\"getinfo\"}' http://localhost:8332\n";
    std::cout << "  curl -u pragma:local -d '{\"method\":\"getnewaddress\"}' http://localhost:8332\n";
    std::cout << "  curl -u pragma:local -d '{\"method\":\"startmining\",\"params\":[\"<address>\"]}' http://localhost:8332\n\n";
}

void runNode(const std::string& nodeId, uint16_t rpcPort, uint16_t p2pPort, 
             const std::vector<std::string>& seedNodes = {}) {
    
    std::cout << "Starting Pragma Node: " << nodeId << std::endl;
    std::cout << "RPC Port: " << rpcPort << std::endl;
    std::cout << "P2P Port: " << p2pPort << std::endl;
    std::cout << "Seed Nodes: ";
    for (const auto& seed : seedNodes) {
        std::cout << seed << " ";
    }
    std::cout << std::endl << std::endl;
    
    try {
        // Configure blockchain
        SynchronizedBlockchainManager::NodeConfig config;
        config.nodeId = nodeId;
        config.rpcPort = rpcPort;
        config.p2pPort = p2pPort;
        config.dataDir = "./pragma_data_" + nodeId;
        config.seedNodes = seedNodes;
        config.enableMining = false; // Start with mining disabled
        config.blockReward = 50.0;
        
        // Initialize global blockchain manager
        if (!GlobalBlockchainManager::initialize(config)) {
            std::cerr << "Failed to initialize blockchain manager" << std::endl;
            return;
        }
        
        auto* blockchain = GlobalBlockchainManager::getInstance();
        if (!blockchain) {
            std::cerr << "Failed to get blockchain instance" << std::endl;
            return;
        }
        
        // Start blockchain
        if (!blockchain->start()) {
            std::cerr << "Failed to start blockchain" << std::endl;
            return;
        }
        
        // Start RPC server
        SynchronizedRPCServer rpcServer(rpcPort, nodeId);
        if (!rpcServer.start()) {
            std::cerr << "Failed to start RPC server" << std::endl;
            return;
        }
        
        std::cout << "Node " << nodeId << " started successfully!" << std::endl;
        std::cout << "RPC server listening on port " << rpcPort << std::endl;
        std::cout << "P2P server listening on port " << p2pPort << std::endl;
        
        // Create some initial addresses for demonstration
        std::string aliceAddr = blockchain->createNewAddress();
        std::string bobAddr = blockchain->createNewAddress();
        std::string charlieAddr = blockchain->createNewAddress();
        
        std::cout << "\nCreated demo addresses:" << std::endl;
        std::cout << "Alice:   " << aliceAddr << std::endl;
        std::cout << "Bob:     " << bobAddr << std::endl;
        std::cout << "Charlie: " << charlieAddr << std::endl;
        
        // Start mining to Alice's address if this is the first node
        if (seedNodes.empty()) {
            std::cout << "\nStarting mining to Alice's address..." << std::endl;
            blockchain->startMining(aliceAddr);
        }
        
        printUsage();
        
        // Main loop
        auto lastUpdate = std::chrono::steady_clock::now();
        while (running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            // Print status every 30 seconds
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - lastUpdate).count() >= 30) {
                blockchain->printChainState();
                lastUpdate = now;
            }
        }
        
        std::cout << "Stopping node " << nodeId << "..." << std::endl;
        rpcServer.stop();
        GlobalBlockchainManager::shutdown();
        
    } catch (const std::exception& e) {
        std::cerr << "Node error: " << e.what() << std::endl;
    }
}

int main(int argc, char* argv[]) {
    // Set up signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    std::cout << "Pragma Bitcoin-like Blockchain System" << std::endl;
    std::cout << "=====================================" << std::endl;
    
    if (argc < 2) {
        std::cout << "\nUsage: " << argv[0] << " <mode> [options]" << std::endl;
        std::cout << "\nModes:" << std::endl;
        std::cout << "  single                    - Run single node on port 8332" << std::endl;
        std::cout << "  multi                     - Run two connected nodes on ports 8332 and 8333" << std::endl;
        std::cout << "  node <id> <rpc> <p2p>     - Run custom node" << std::endl;
        std::cout << "  connect <id> <rpc> <p2p> <seed1:port1> [seed2:port2] ... - Connect to existing network" << std::endl;
        std::cout << "\nExamples:" << std::endl;
        std::cout << "  " << argv[0] << " single" << std::endl;
        std::cout << "  " << argv[0] << " multi" << std::endl;
        std::cout << "  " << argv[0] << " node node1 8334 8335" << std::endl;
        std::cout << "  " << argv[0] << " connect node2 8336 8337 127.0.0.1:8333" << std::endl;
        return 1;
    }
    
    std::string mode = argv[1];
    
    if (mode == "single") {
        // Run single node
        runNode("node1", 8332, 8333);
        
    } else if (mode == "multi") {
        // Run two connected nodes
        std::cout << "Starting multi-node demonstration..." << std::endl;
        
        // Start first node in a separate thread
        std::thread node1Thread([&]() {
            runNode("node1", 8332, 8333);
        });
        
        // Wait a moment for first node to start
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        // Start second node connected to first
        std::thread node2Thread([&]() {
            runNode("node2", 8334, 8335, {"127.0.0.1:8333"});
        });
        
        // Wait for threads
        node1Thread.join();
        node2Thread.join();
        
    } else if (mode == "node" && argc >= 5) {
        // Run custom node
        std::string nodeId = argv[2];
        uint16_t rpcPort = static_cast<uint16_t>(std::stoi(argv[3]));
        uint16_t p2pPort = static_cast<uint16_t>(std::stoi(argv[4]));
        
        runNode(nodeId, rpcPort, p2pPort);
        
    } else if (mode == "connect" && argc >= 6) {
        // Connect to existing network
        std::string nodeId = argv[2];
        uint16_t rpcPort = static_cast<uint16_t>(std::stoi(argv[3]));
        uint16_t p2pPort = static_cast<uint16_t>(std::stoi(argv[4]));
        
        std::vector<std::string> seedNodes;
        for (int i = 5; i < argc; i++) {
            seedNodes.push_back(argv[i]);
        }
        
        runNode(nodeId, rpcPort, p2pPort, seedNodes);
        
    } else {
        std::cerr << "Invalid mode or insufficient arguments" << std::endl;
        return 1;
    }
    
    std::cout << "Pragma blockchain shutdown complete." << std::endl;
    return 0;
}
