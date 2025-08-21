#pragma once

#include "transaction.h"
#include "utxo.h"
#include "validator.h"
#include "merkle.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <queue>
#include <memory>
#include <functional>

namespace pragma {

/**
 * Transaction entry in the mempool with metadata
 */
struct MempoolEntry {
    Transaction transaction;    // The transaction
    uint64_t fee;              // Transaction fee in satoshis
    uint64_t feeRate;          // Fee rate (satoshis per byte)
    uint64_t entryTime;        // Time when transaction entered mempool
    uint32_t entryHeight;      // Block height when transaction entered
    size_t txSize;             // Transaction size in bytes
    std::vector<std::string> depends; // Transaction IDs this tx depends on
    
    MempoolEntry(const Transaction& tx, uint64_t f, uint64_t height)
        : transaction(tx), fee(f), entryTime(std::time(nullptr)), 
          entryHeight(height), txSize(tx.serialize().size()) {
        feeRate = txSize > 0 ? fee / txSize : 0;
    }
};

/**
 * Transaction priority comparator for fee-based ordering
 */
struct TxPriorityComparator {
    bool operator()(const std::shared_ptr<MempoolEntry>& a, 
                   const std::shared_ptr<MempoolEntry>& b) const {
        // Higher fee rate = higher priority (reverse for priority queue)
        if (a->feeRate != b->feeRate) {
            return a->feeRate < b->feeRate;
        }
        // Earlier entry time = higher priority if fee rates are equal
        return a->entryTime > b->entryTime;
    }
};

/**
 * Memory pool for unconfirmed transactions
 */
class Mempool {
private:
    // Transaction storage
    std::unordered_map<std::string, std::shared_ptr<MempoolEntry>> transactions; // txid -> entry
    std::unordered_map<std::string, std::unordered_set<std::string>> spentOutputs; // outpoint -> txids
    std::unordered_map<std::string, std::unordered_set<std::string>> dependencies; // txid -> dependent txids
    
    // Priority queue for transaction selection
    std::priority_queue<std::shared_ptr<MempoolEntry>, 
                       std::vector<std::shared_ptr<MempoolEntry>>, 
                       TxPriorityComparator> priorityQueue;
    
    // Configuration
    size_t maxSize;            // Maximum number of transactions
    uint64_t maxMemory;        // Maximum memory usage in bytes
    uint64_t minFeeRate;       // Minimum fee rate (satoshis per byte)
    uint32_t expireTime;       // Transaction expiry time in seconds
    
    // Current state
    size_t currentSize;        // Current number of transactions
    uint64_t currentMemory;    // Current memory usage
    uint64_t totalFees;        // Total fees in mempool
    
    // Validation components
    UTXOSet* utxoSet;
    BlockValidator* validator;
    
    // Helper methods
    bool hasConflicts(const Transaction& tx) const;
    bool hasDependencies(const Transaction& tx) const;
    std::vector<std::string> findDependencies(const Transaction& tx) const;
    void updateDependencies(const std::string& txid);
    void removeDependents(const std::string& txid);
    void rebuildPriorityQueue();
    void evictLowPriorityTransactions();
    bool isExpired(const MempoolEntry& entry, uint32_t currentHeight) const;
    std::string outpointToString(const OutPoint& outpoint) const;
    
public:
    Mempool(UTXOSet* utxos, BlockValidator* val, 
            size_t maxTxs = 50000, uint64_t maxMem = 300000000, uint64_t minFee = 1);
    ~Mempool() = default;
    
    // Core mempool operations
    bool addTransaction(const Transaction& tx, uint32_t currentHeight);
    bool removeTransaction(const std::string& txid);
    void removeTransactions(const std::vector<std::string>& txids);
    bool hasTransaction(const std::string& txid) const;
    std::shared_ptr<MempoolEntry> getTransaction(const std::string& txid) const;
    
    // Transaction selection for mining
    std::vector<Transaction> selectTransactions(uint64_t maxBlockSize, uint32_t currentHeight) const;
    std::vector<Transaction> selectTransactionsByFee(size_t maxCount) const;
    std::vector<Transaction> selectTransactionsByValue(uint64_t minValue) const;
    
    // Mempool maintenance
    void removeExpiredTransactions(uint32_t currentHeight);
    void removeConflictingTransactions(const std::vector<Transaction>& confirmedTxs);
    void updateForNewBlock(const std::vector<Transaction>& confirmedTxs, uint32_t newHeight);
    void clear();
    
    // Statistics and information
    struct MempoolStats {
        size_t transactionCount;
        uint64_t totalMemoryUsage;
        uint64_t totalFees;
        uint64_t averageFeeRate;
        uint64_t minFeeRate;
        uint64_t maxFeeRate;
        size_t dependentTransactions;
        uint32_t oldestTransactionTime;
        
        MempoolStats() : transactionCount(0), totalMemoryUsage(0), totalFees(0),
                        averageFeeRate(0), minFeeRate(0), maxFeeRate(0),
                        dependentTransactions(0), oldestTransactionTime(0) {}
    };
    
    MempoolStats getStats() const;
    void printStats() const;
    void printTransactions() const;
    
    // Configuration
    void setMaxSize(size_t size) { maxSize = size; }
    void setMaxMemory(uint64_t memory) { maxMemory = memory; }
    void setMinFeeRate(uint64_t rate) { minFeeRate = rate; }
    void setExpireTime(uint32_t time) { expireTime = time; }
    
    // Getters
    size_t size() const { return currentSize; }
    uint64_t getMemoryUsage() const { return currentMemory; }
    uint64_t getTotalFees() const { return totalFees; }
    bool isEmpty() const { return currentSize == 0; }
    bool isFull() const { return currentSize >= maxSize || currentMemory >= maxMemory; }
    
    // Transaction validation
    bool validateTransaction(const Transaction& tx, uint32_t currentHeight) const;
    bool isDoubleSpend(const Transaction& tx) const;
    
    // Dependency management
    std::vector<std::string> getDependents(const std::string& txid) const;
    std::vector<std::string> getDependencies(const std::string& txid) const;
    bool canBeIncluded(const std::string& txid, const std::unordered_set<std::string>& included) const;
    
    // Fee estimation
    uint64_t estimateFeeRate(uint32_t targetBlocks) const;
    uint64_t getMinimumFeeRate() const { return minFeeRate; }
};

/**
 * Block template for mining
 */
struct BlockTemplate {
    BlockHeader header;
    std::vector<Transaction> transactions;
    uint64_t totalFees;
    uint64_t blockReward;
    uint64_t blockSize;
    uint32_t transactionCount;
    
    BlockTemplate() : totalFees(0), blockReward(0), blockSize(0), transactionCount(0) {}
    
    Block createBlock() const {
        return Block(header, transactions);
    }
    
    void updateMerkleRoot() {
        std::vector<std::string> txids;
        for (const auto& tx : transactions) {
            txids.push_back(tx.txid);
        }
        header.merkleRoot = MerkleTree::buildMerkleRoot(txids);
    }
};

/**
 * Mining interface and block template creation
 */
class Miner {
private:
    Mempool* mempool;
    UTXOSet* utxoSet;
    ChainState* chainState;
    BlockValidator* validator;
    
    // Mining configuration
    std::string minerAddress;
    uint64_t maxBlockSize;
    uint32_t maxTransactions;
    
    // Helper methods
    uint64_t calculateBlockSubsidy(uint32_t height) const;
    bool isValidBlockTemplate(const BlockTemplate& blockTemplate) const;
    
public:
    Miner(Mempool* pool, UTXOSet* utxos, ChainState* chain, BlockValidator* val, 
          const std::string& address);
    ~Miner() = default;
    
    // Block template creation
    BlockTemplate createBlockTemplate(uint32_t currentHeight) const;
    BlockTemplate createBlockTemplate(const std::vector<Transaction>& specificTxs, uint32_t currentHeight) const;
    
    // Mining operations
    Block mineBlock(uint32_t maxIterations = 1000000) const;
    Block mineBlockTemplate(const BlockTemplate& blockTemplate, uint32_t maxIterations = 1000000) const;
    bool mineBlockInPlace(Block& block, uint32_t maxIterations = 1000000) const;
    
    // Configuration
    void setMinerAddress(const std::string& address) { minerAddress = address; }
    void setMaxBlockSize(uint64_t size) { maxBlockSize = size; }
    void setMaxTransactions(uint32_t count) { maxTransactions = count; }
    
    // Getters
    const std::string& getMinerAddress() const { return minerAddress; }
    uint64_t getMaxBlockSize() const { return maxBlockSize; }
    
    // Mining statistics
    struct MiningStats {
        uint32_t blocksCreated;
        uint32_t blocksMined;
        uint64_t totalHashAttempts;
        uint64_t totalMiningTime;
        uint64_t totalFeesEarned;
        uint64_t totalSubsidyEarned;
        
        MiningStats() : blocksCreated(0), blocksMined(0), totalHashAttempts(0),
                       totalMiningTime(0), totalFeesEarned(0), totalSubsidyEarned(0) {}
    };
    
    MiningStats getMiningStats() const { return miningStats; }
    void resetMiningStats() { miningStats = MiningStats(); }
    void printMiningStats() const;
    
private:
    mutable MiningStats miningStats;
};

} // namespace pragma
