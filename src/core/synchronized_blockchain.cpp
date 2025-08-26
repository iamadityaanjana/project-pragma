#include "synchronized_blockchain.h"
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>
#include <fstream>

namespace pragma {

// Global instance management
std::unique_ptr<SynchronizedBlockchainManager> GlobalBlockchainManager::instance_ = nullptr;
std::mutex GlobalBlockchainManager::instanceMutex_;

SynchronizedBlockchainManager::SynchronizedBlockchainManager(const NodeConfig& config)
    : config_(config) {
    // Initialize components
    chainState_ = std::make_shared<ChainState>();
    mempool_ = std::make_shared<Mempool>();
    walletManager_ = std::make_shared<WalletManager>();
    p2pNetwork_ = std::make_shared<P2PNetwork>();
}

SynchronizedBlockchainManager::~SynchronizedBlockchainManager() {
    stop();
}

bool SynchronizedBlockchainManager::initialize() {
    std::lock_guard<std::mutex> lock(stateMutex_);
    
    try {
        // Initialize chain state
        if (!chainState_->initialize(config_.dataDir)) {
            return false;
        }
        
        // Initialize mempool
        if (!mempool_->initialize()) {
            return false;
        }
        
        // Initialize wallet manager
        if (!walletManager_->initialize(config_.dataDir)) {
            return false;
        }
        
        // Initialize P2P network
        P2PNetwork::Config p2pConfig;
        p2pConfig.port = config_.p2pPort;
        p2pConfig.nodeId = config_.nodeId;
        p2pConfig.dataDir = config_.dataDir;
        
        if (!p2pNetwork_->initialize(p2pConfig)) {
            return false;
        }
        
        // Set up network callbacks
        p2pNetwork_->setBlockCallback([this](const Block& block, const std::string& peerId) {
            onBlockReceived(block, peerId);
        });
        
        p2pNetwork_->setTransactionCallback([this](const Transaction& tx, const std::string& peerId) {
            onTransactionReceived(tx, peerId);
        });
        
        p2pNetwork_->setPeerConnectedCallback([this](const std::string& peerId) {
            onPeerConnected(peerId);
        });
        
        p2pNetwork_->setPeerDisconnectedCallback([this](const std::string& peerId) {
            onPeerDisconnected(peerId);
        });
        
        // Load saved state if exists
        loadState();
        
        // Update total supply
        updateTotalSupply();
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize synchronized blockchain: " << e.what() << std::endl;
        return false;
    }
}

bool SynchronizedBlockchainManager::start() {
    std::lock_guard<std::mutex> lock(stateMutex_);
    
    if (running_) {
        return true;
    }
    
    try {
        // Start P2P network
        if (!p2pNetwork_->start()) {
            return false;
        }
        
        // Connect to seed nodes
        for (const auto& seedNode : config_.seedNodes) {
            size_t colonPos = seedNode.find(':');
            if (colonPos != std::string::npos) {
                std::string address = seedNode.substr(0, colonPos);
                uint16_t port = static_cast<uint16_t>(std::stoi(seedNode.substr(colonPos + 1)));
                connectToPeer(address, port);
            }
        }
        
        running_ = true;
        
        // Start synchronization thread
        syncThread_ = std::thread(&SynchronizedBlockchainManager::syncLoop, this);
        
        // Start mining if enabled
        if (config_.enableMining && !config_.minerAddress.empty()) {
            startMining(config_.minerAddress);
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to start synchronized blockchain: " << e.what() << std::endl;
        return false;
    }
}

void SynchronizedBlockchainManager::stop() {
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (!running_) {
            return;
        }
        running_ = false;
    }
    
    // Stop mining
    stopMining();
    
    // Stop synchronization
    syncCondition_.notify_all();
    if (syncThread_.joinable()) {
        syncThread_.join();
    }
    
    // Stop P2P network
    if (p2pNetwork_) {
        p2pNetwork_->stop();
    }
    
    // Save state
    saveState();
}

void SynchronizedBlockchainManager::syncLoop() {
    while (running_) {
        try {
            // Check if we need to sync
            if (!synchronized_ && connectedPeers_ > 0) {
                syncWithPeers();
            }
            
            // Regular sync check every 10 seconds
            std::unique_lock<std::mutex> lock(stateMutex_);
            syncCondition_.wait_for(lock, std::chrono::seconds(10), [this] { return !running_; });
            
        } catch (const std::exception& e) {
            std::cerr << "Sync loop error: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
}

void SynchronizedBlockchainManager::miningLoop() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 5000); // Random mining delay
    
    while (mining_ && !stopMining_) {
        try {
            // Mine a new block
            Block newBlock = mineBlock(config_.minerAddress, config_.blockReward);
            
            {
                std::lock_guard<std::mutex> lock(stateMutex_);
                if (validateAndAddBlock(newBlock)) {
                    std::cout << "Mined new block: " << newBlock.hash << " at height " 
                              << chainState_->getBestTip().height + 1 << std::endl;
                    
                    // Broadcast to network
                    broadcastBlock(newBlock);
                    
                    // Update total supply
                    updateTotalSupply();
                }
            }
            
            // Random delay between mining attempts
            std::this_thread::sleep_for(std::chrono::milliseconds(dis(gen)));
            
        } catch (const std::exception& e) {
            std::cerr << "Mining error: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

Block SynchronizedBlockchainManager::mineBlock(const std::string& minerAddress, double reward) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    
    Block block;
    block.height = chainState_->getBestTip().height + 1;
    block.previousHash = chainState_->getBestBlockHash();
    block.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    // Add coinbase transaction (mining reward)
    Transaction coinbaseTx;
    coinbaseTx.hash = "coinbase_" + std::to_string(block.height);
    coinbaseTx.inputs.clear(); // No inputs for coinbase
    
    TransactionOutput rewardOutput;
    rewardOutput.address = minerAddress;
    rewardOutput.amount = reward;
    rewardOutput.scriptPubKey = "OP_DUP OP_HASH160 " + minerAddress + " OP_EQUALVERIFY OP_CHECKSIG";
    coinbaseTx.outputs.push_back(rewardOutput);
    
    block.transactions.push_back(coinbaseTx);
    
    // Add pending transactions from mempool
    auto pendingTxs = mempool_->getPendingTransactions();
    for (const auto& tx : pendingTxs) {
        if (block.transactions.size() < 1000) { // Block size limit
            block.transactions.push_back(tx);
        }
    }
    
    // Calculate merkle root
    block.merkleRoot = calculateMerkleRoot(block.transactions);
    
    // Simple proof of work
    block.nonce = 0;
    std::string target(miningDifficulty_, '0');
    
    do {
        block.nonce++;
        block.hash = calculateBlockHash(block);
    } while (block.hash.substr(0, miningDifficulty_) != target);
    
    return block;
}

bool SynchronizedBlockchainManager::addBlock(const Block& block) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return validateAndAddBlock(block);
}

bool SynchronizedBlockchainManager::validateAndAddBlock(const Block& block) {
    try {
        // Validate block
        if (!chainState_->validateBlock(block)) {
            return false;
        }
        
        // Add to chain
        if (!chainState_->addBlock(block)) {
            return false;
        }
        
        // Remove confirmed transactions from mempool
        for (const auto& tx : block.transactions) {
            if (tx.hash.find("coinbase_") != 0) { // Skip coinbase transactions
                mempool_->removeTransaction(tx.hash);
            }
        }
        
        // Update balances in wallet manager
        for (const auto& tx : block.transactions) {
            walletManager_->processTransaction(tx);
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Block validation error: " << e.what() << std::endl;
        return false;
    }
}

bool SynchronizedBlockchainManager::addTransaction(const Transaction& tx) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    
    try {
        // Validate transaction
        if (!mempool_->validateTransaction(tx)) {
            return false;
        }
        
        // Add to mempool
        if (!mempool_->addTransaction(tx)) {
            return false;
        }
        
        // Broadcast to network
        broadcastTransaction(tx);
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Transaction error: " << e.what() << std::endl;
        return false;
    }
}

std::string SynchronizedBlockchainManager::sendTransaction(const std::string& fromAddress, 
                                                          const std::string& toAddress, 
                                                          double amount) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    
    try {
        // Create transaction
        Transaction tx = walletManager_->createTransaction(fromAddress, toAddress, amount);
        
        // Add to mempool and broadcast
        if (mempool_->addTransaction(tx)) {
            broadcastTransaction(tx);
            return tx.hash;
        }
        
        return "";
    } catch (const std::exception& e) {
        std::cerr << "Send transaction error: " << e.what() << std::endl;
        return "";
    }
}

double SynchronizedBlockchainManager::getBalance(const std::string& address) const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return walletManager_->getBalance(address);
}

void SynchronizedBlockchainManager::startMining(const std::string& minerAddress) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    
    if (mining_) {
        return;
    }
    
    config_.minerAddress = minerAddress;
    mining_ = true;
    stopMining_ = false;
    
    miningThread_ = std::thread(&SynchronizedBlockchainManager::miningLoop, this);
    
    std::cout << "Started mining for address: " << minerAddress << std::endl;
}

void SynchronizedBlockchainManager::stopMining() {
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (!mining_) {
            return;
        }
        stopMining_ = true;
        mining_ = false;
    }
    
    if (miningThread_.joinable()) {
        miningThread_.join();
    }
    
    std::cout << "Stopped mining" << std::endl;
}

void SynchronizedBlockchainManager::onPeerConnected(const std::string& peerId) {
    connectedPeers_++;
    std::cout << "Peer connected: " << peerId << " (Total: " << connectedPeers_ << ")" << std::endl;
    
    // Request sync when first peer connects
    if (connectedPeers_ == 1) {
        requestBlockchainSync();
    }
}

void SynchronizedBlockchainManager::onPeerDisconnected(const std::string& peerId) {
    if (connectedPeers_ > 0) {
        connectedPeers_--;
    }
    std::cout << "Peer disconnected: " << peerId << " (Total: " << connectedPeers_ << ")" << std::endl;
}

void SynchronizedBlockchainManager::onBlockReceived(const Block& block, const std::string& peerId) {
    std::cout << "Received block " << block.hash << " from peer " << peerId << std::endl;
    
    if (addBlock(block)) {
        std::cout << "Added block " << block.hash << " to chain" << std::endl;
        updateTotalSupply();
    }
}

void SynchronizedBlockchainManager::onTransactionReceived(const Transaction& tx, const std::string& peerId) {
    std::cout << "Received transaction " << tx.hash << " from peer " << peerId << std::endl;
    addTransaction(tx);
}

void SynchronizedBlockchainManager::broadcastBlock(const Block& block) {
    if (p2pNetwork_) {
        p2pNetwork_->broadcastBlock(block);
    }
}

void SynchronizedBlockchainManager::broadcastTransaction(const Transaction& tx) {
    if (p2pNetwork_) {
        p2pNetwork_->broadcastTransaction(tx);
    }
}

SynchronizedBlockchainManager::NetworkStats SynchronizedBlockchainManager::getNetworkStats() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    
    NetworkStats stats;
    stats.connectedPeers = connectedPeers_;
    stats.blockHeight = chainState_->getBestTip().height;
    stats.bestBlockHash = chainState_->getBestBlockHash();
    stats.synchronized = synchronized_;
    stats.totalSupply = totalSupply_;
    stats.mempoolSize = mempool_->size();
    
    return stats;
}

void SynchronizedBlockchainManager::updateTotalSupply() {
    // Calculate total supply from all coinbase transactions
    uint64_t supply = 0;
    
    // Simple calculation: block_height * block_reward
    uint32_t height = chainState_->getBestTip().height;
    supply = static_cast<uint64_t>(height * config_.blockReward);
    
    totalSupply_ = supply;
}

std::string SynchronizedBlockchainManager::createNewAddress() {
    return walletManager_->generateNewAddress();
}

uint32_t SynchronizedBlockchainManager::getBlockHeight() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return chainState_->getBestTip().height;
}

std::string SynchronizedBlockchainManager::getBestBlockHash() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return chainState_->getBestBlockHash();
}

void SynchronizedBlockchainManager::syncWithPeers() {
    if (p2pNetwork_) {
        p2pNetwork_->requestBlockchainSync();
        synchronized_ = true;
    }
}

void SynchronizedBlockchainManager::requestBlockchainSync() {
    syncCondition_.notify_one();
}

bool SynchronizedBlockchainManager::saveState() const {
    try {
        // Save chain state
        chainState_->save();
        
        // Save wallet state
        walletManager_->save();
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to save state: " << e.what() << std::endl;
        return false;
    }
}

bool SynchronizedBlockchainManager::loadState() {
    try {
        // Load chain state
        chainState_->load();
        
        // Load wallet state
        walletManager_->load();
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to load state: " << e.what() << std::endl;
        return false;
    }
}

void SynchronizedBlockchainManager::printChainState() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    
    auto stats = getNetworkStats();
    std::cout << "\n=== Blockchain State ===" << std::endl;
    std::cout << "Block Height: " << stats.blockHeight << std::endl;
    std::cout << "Best Block: " << stats.bestBlockHash << std::endl;
    std::cout << "Total Supply: " << stats.totalSupply << " BTC" << std::endl;
    std::cout << "Mempool Size: " << stats.mempoolSize << " transactions" << std::endl;
    std::cout << "Connected Peers: " << stats.connectedPeers << std::endl;
    std::cout << "Synchronized: " << (stats.synchronized ? "Yes" : "No") << std::endl;
    std::cout << "Mining: " << (mining_ ? "Active" : "Inactive") << std::endl;
    if (mining_) {
        std::cout << "Miner Address: " << config_.minerAddress << std::endl;
    }
    std::cout << "======================\n" << std::endl;
}

// Global instance management
bool GlobalBlockchainManager::initialize(const SynchronizedBlockchainManager::NodeConfig& config) {
    std::lock_guard<std::mutex> lock(instanceMutex_);
    
    if (instance_) {
        return false; // Already initialized
    }
    
    instance_ = std::make_unique<SynchronizedBlockchainManager>(config);
    return instance_->initialize();
}

SynchronizedBlockchainManager* GlobalBlockchainManager::getInstance() {
    std::lock_guard<std::mutex> lock(instanceMutex_);
    return instance_.get();
}

void GlobalBlockchainManager::shutdown() {
    std::lock_guard<std::mutex> lock(instanceMutex_);
    
    if (instance_) {
        instance_->stop();
        instance_.reset();
    }
}

bool GlobalBlockchainManager::isInitialized() {
    std::lock_guard<std::mutex> lock(instanceMutex_);
    return instance_ != nullptr;
}

} // namespace pragma
