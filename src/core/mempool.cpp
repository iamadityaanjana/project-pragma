#include "mempool.h"
#include "merkle.h"
#include "difficulty.h"
#include "primitives/hash.h"
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <ctime>

namespace pragma {

Mempool::Mempool(UTXOSet* utxos, BlockValidator* val, 
                 size_t maxTxs, uint64_t maxMem, uint64_t minFee)
    : maxSize(maxTxs), maxMemory(maxMem), minFeeRate(minFee), expireTime(86400), // 24 hours
      currentSize(0), currentMemory(0), totalFees(0),
      utxoSet(utxos), validator(val) {
}

bool Mempool::addTransaction(const Transaction& tx, uint32_t currentHeight) {
    std::string txid = tx.txid;
    
    // Check if transaction already exists
    if (hasTransaction(txid)) {
        return false;
    }
    
    // Validate transaction
    if (!validateTransaction(tx, currentHeight)) {
        return false;
    }
    
    // Check for double spending
    if (isDoubleSpend(tx)) {
        return false;
    }
    
    // Calculate fee
    uint64_t inputValue = 0;
    uint64_t outputValue = 0;
    
    // Calculate input value
    for (const auto& input : tx.vin) {
        OutPoint outpoint = input.prevout;
        UTXO* utxo = utxoSet->getUTXO(outpoint);
        if (utxo) {
            inputValue += utxo->output.value;
        }
    }
    
    // Calculate output value
    for (const auto& output : tx.vout) {
        outputValue += output.value;
    }
    
    // Ensure fee is non-negative and meets minimum
    if (inputValue < outputValue) {
        return false; // Invalid transaction
    }
    
    uint64_t fee = inputValue - outputValue;
    size_t txSize = tx.serialize().size();
    uint64_t feeRate = txSize > 0 ? fee / txSize : 0;
    
    if (feeRate < minFeeRate) {
        return false; // Fee too low
    }
    
    // Check mempool capacity
    if (currentSize >= maxSize || currentMemory + txSize > maxMemory) {
        // Try to evict low priority transactions
        evictLowPriorityTransactions();
        
        // Check again
        if (currentSize >= maxSize || currentMemory + txSize > maxMemory) {
            return false; // Still no space
        }
    }
    
    // Create mempool entry
    auto entry = std::make_shared<MempoolEntry>(tx, fee, currentHeight);
    
    // Find dependencies
    entry->depends = findDependencies(tx);
    
    // Add to mempool
    transactions[txid] = entry;
    
    // Track spent outputs
    for (const auto& input : tx.vin) {
        std::string outpointStr = outpointToString(input.prevout);
        spentOutputs[outpointStr].insert(txid);
    }
    
    // Update dependencies
    updateDependencies(txid);
    
    // Add to priority queue
    priorityQueue.push(entry);
    
    // Update statistics
    currentSize++;
    currentMemory += txSize;
    totalFees += fee;
    
    return true;
}

bool Mempool::removeTransaction(const std::string& txid) {
    auto it = transactions.find(txid);
    if (it == transactions.end()) {
        return false;
    }
    
    auto entry = it->second;
    
    // Remove dependents first
    removeDependents(txid);
    
    // Remove from spent outputs tracking
    for (const auto& input : entry->transaction.vin) {
        std::string outpointStr = outpointToString(input.prevout);
        spentOutputs[outpointStr].erase(txid);
        if (spentOutputs[outpointStr].empty()) {
            spentOutputs.erase(outpointStr);
        }
    }
    
    // Remove from dependencies
    for (const auto& depTxid : entry->depends) {
        dependencies[depTxid].erase(txid);
        if (dependencies[depTxid].empty()) {
            dependencies.erase(depTxid);
        }
    }
    
    // Update statistics
    currentSize--;
    currentMemory -= entry->txSize;
    totalFees -= entry->fee;
    
    // Remove from transactions map
    transactions.erase(it);
    
    // Rebuild priority queue (inefficient but simple)
    rebuildPriorityQueue();
    
    return true;
}

void Mempool::removeTransactions(const std::vector<std::string>& txids) {
    for (const auto& txid : txids) {
        removeTransaction(txid);
    }
}

bool Mempool::hasTransaction(const std::string& txid) const {
    return transactions.find(txid) != transactions.end();
}

std::shared_ptr<MempoolEntry> Mempool::getTransaction(const std::string& txid) const {
    auto it = transactions.find(txid);
    return it != transactions.end() ? it->second : nullptr;
}

std::vector<Transaction> Mempool::selectTransactions(uint64_t maxBlockSize, uint32_t currentHeight) const {
    std::vector<Transaction> selected;
    std::unordered_set<std::string> included;
    uint64_t currentBlockSize = 80; // Block header size
    
    // Create a copy of priority queue for selection
    auto pq = priorityQueue;
    
    while (!pq.empty() && currentBlockSize < maxBlockSize) {
        auto entry = pq.top();
        pq.pop();
        
        // Check if transaction can be included (dependencies satisfied)
        if (!canBeIncluded(entry->transaction.txid, included)) {
            continue;
        }
        
        // Check if transaction fits in block
        if (currentBlockSize + entry->txSize > maxBlockSize) {
            continue;
        }
        
        // Check if transaction hasn't expired
        if (isExpired(*entry, currentHeight)) {
            continue;
        }
        
        selected.push_back(entry->transaction);
        included.insert(entry->transaction.txid);
        currentBlockSize += entry->txSize;
    }
    
    return selected;
}

std::vector<Transaction> Mempool::selectTransactionsByFee(size_t maxCount) const {
    std::vector<Transaction> selected;
    std::unordered_set<std::string> included;
    
    auto pq = priorityQueue;
    
    while (!pq.empty() && selected.size() < maxCount) {
        auto entry = pq.top();
        pq.pop();
        
        if (canBeIncluded(entry->transaction.txid, included)) {
            selected.push_back(entry->transaction);
            included.insert(entry->transaction.txid);
        }
    }
    
    return selected;
}

std::vector<Transaction> Mempool::selectTransactionsByValue(uint64_t minValue) const {
    std::vector<Transaction> selected;
    
    for (const auto& [txid, entry] : transactions) {
        if (entry->fee >= minValue) {
            selected.push_back(entry->transaction);
        }
    }
    
    // Sort by fee rate (highest first)
    std::sort(selected.begin(), selected.end(), 
              [this](const Transaction& a, const Transaction& b) {
                  auto entryA = getTransaction(a.txid);
                  auto entryB = getTransaction(b.txid);
                  return entryA->feeRate > entryB->feeRate;
              });
    
    return selected;
}

void Mempool::removeExpiredTransactions(uint32_t currentHeight) {
    std::vector<std::string> expiredTxids;
    
    for (const auto& [txid, entry] : transactions) {
        if (isExpired(*entry, currentHeight)) {
            expiredTxids.push_back(txid);
        }
    }
    
    removeTransactions(expiredTxids);
}

void Mempool::removeConflictingTransactions(const std::vector<Transaction>& confirmedTxs) {
    std::unordered_set<std::string> conflictingTxids;
    
    // Find transactions that spend the same outputs as confirmed transactions
    for (const auto& confirmedTx : confirmedTxs) {
        for (const auto& input : confirmedTx.vin) {
            std::string outpointStr = outpointToString(input.prevout);
            auto it = spentOutputs.find(outpointStr);
            if (it != spentOutputs.end()) {
                for (const auto& txid : it->second) {
                    conflictingTxids.insert(txid);
                }
            }
        }
    }
    
    // Remove conflicting transactions
    for (const auto& txid : conflictingTxids) {
        removeTransaction(txid);
    }
}

void Mempool::updateForNewBlock(const std::vector<Transaction>& confirmedTxs, uint32_t newHeight) {
    // Remove confirmed transactions from mempool
    std::vector<std::string> confirmedTxids;
    for (const auto& tx : confirmedTxs) {
        confirmedTxids.push_back(tx.txid);
    }
    removeTransactions(confirmedTxids);
    
    // Remove conflicting transactions
    removeConflictingTransactions(confirmedTxs);
    
    // Remove expired transactions
    removeExpiredTransactions(newHeight);
}

void Mempool::clear() {
    transactions.clear();
    spentOutputs.clear();
    dependencies.clear();
    while (!priorityQueue.empty()) {
        priorityQueue.pop();
    }
    currentSize = 0;
    currentMemory = 0;
    totalFees = 0;
}

Mempool::MempoolStats Mempool::getStats() const {
    MempoolStats stats;
    stats.transactionCount = currentSize;
    stats.totalMemoryUsage = currentMemory;
    stats.totalFees = totalFees;
    
    if (currentSize > 0) {
        stats.averageFeeRate = totalFees / currentMemory;
        
        uint64_t minRate = UINT64_MAX;
        uint64_t maxRate = 0;
        uint32_t oldestTime = UINT32_MAX;
        
        for (const auto& [txid, entry] : transactions) {
            minRate = std::min(minRate, entry->feeRate);
            maxRate = std::max(maxRate, entry->feeRate);
            oldestTime = std::min(oldestTime, static_cast<uint32_t>(entry->entryTime));
        }
        
        stats.minFeeRate = minRate;
        stats.maxFeeRate = maxRate;
        stats.oldestTransactionTime = oldestTime;
    }
    
    stats.dependentTransactions = dependencies.size();
    
    return stats;
}

void Mempool::printStats() const {
    auto stats = getStats();
    std::cout << "\n=== Mempool Statistics ===" << std::endl;
    std::cout << "Transactions: " << stats.transactionCount << std::endl;
    std::cout << "Memory Usage: " << stats.totalMemoryUsage << " bytes" << std::endl;
    std::cout << "Total Fees: " << stats.totalFees << " satoshis" << std::endl;
    std::cout << "Average Fee Rate: " << stats.averageFeeRate << " sat/byte" << std::endl;
    std::cout << "Min Fee Rate: " << stats.minFeeRate << " sat/byte" << std::endl;
    std::cout << "Max Fee Rate: " << stats.maxFeeRate << " sat/byte" << std::endl;
    std::cout << "Dependent Transactions: " << stats.dependentTransactions << std::endl;
    
    if (stats.oldestTransactionTime > 0) {
        uint32_t age = std::time(nullptr) - stats.oldestTransactionTime;
        std::cout << "Oldest Transaction Age: " << age << " seconds" << std::endl;
    }
}

void Mempool::printTransactions() const {
    std::cout << "\n=== Mempool Transactions ===" << std::endl;
    for (const auto& [txid, entry] : transactions) {
        std::cout << "TxID: " << txid.substr(0, 16) << "..." << std::endl;
        std::cout << "  Fee: " << entry->fee << " satoshis" << std::endl;
        std::cout << "  Fee Rate: " << entry->feeRate << " sat/byte" << std::endl;
        std::cout << "  Size: " << entry->txSize << " bytes" << std::endl;
        std::cout << "  Dependencies: " << entry->depends.size() << std::endl;
    }
}

bool Mempool::validateTransaction(const Transaction& tx, uint32_t currentHeight) const {
    if (!validator) {
        return false;
    }
    
    auto result = validator->validateTransactionOnly(tx, currentHeight);
    return result.isValid;
}

bool Mempool::isDoubleSpend(const Transaction& tx) const {
    for (const auto& input : tx.vin) {
        std::string outpointStr = outpointToString(input.prevout);
        if (spentOutputs.find(outpointStr) != spentOutputs.end()) {
            return true;
        }
    }
    return false;
}

// Helper method implementations
bool Mempool::hasConflicts(const Transaction& tx) const {
    return isDoubleSpend(tx);
}

bool Mempool::hasDependencies(const Transaction& tx) const {
    for (const auto& input : tx.vin) {
        std::string prevTxid = input.prevout.txid;
        if (hasTransaction(prevTxid)) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> Mempool::findDependencies(const Transaction& tx) const {
    std::vector<std::string> deps;
    for (const auto& input : tx.vin) {
        std::string prevTxid = input.prevout.txid;
        if (hasTransaction(prevTxid)) {
            deps.push_back(prevTxid);
        }
    }
    return deps;
}

void Mempool::updateDependencies(const std::string& txid) {
    auto entry = getTransaction(txid);
    if (!entry) return;
    
    for (const auto& depTxid : entry->depends) {
        dependencies[depTxid].insert(txid);
    }
}

void Mempool::removeDependents(const std::string& txid) {
    auto it = dependencies.find(txid);
    if (it == dependencies.end()) {
        return;
    }
    
    std::vector<std::string> dependents(it->second.begin(), it->second.end());
    for (const auto& depTxid : dependents) {
        removeTransaction(depTxid);
    }
}

void Mempool::rebuildPriorityQueue() {
    while (!priorityQueue.empty()) {
        priorityQueue.pop();
    }
    
    for (const auto& [txid, entry] : transactions) {
        priorityQueue.push(entry);
    }
}

void Mempool::evictLowPriorityTransactions() {
    if (currentSize == 0) return;
    
    // Find lowest fee rate transaction
    auto lowestEntry = *std::min_element(
        transactions.begin(), transactions.end(),
        [](const auto& a, const auto& b) {
            return a.second->feeRate < b.second->feeRate;
        });
    
    removeTransaction(lowestEntry.first);
}

bool Mempool::isExpired(const MempoolEntry& entry, uint32_t currentHeight) const {
    uint32_t currentTime = std::time(nullptr);
    return (currentTime - entry.entryTime) > expireTime;
}

std::string Mempool::outpointToString(const OutPoint& outpoint) const {
    return outpoint.txid + ":" + std::to_string(outpoint.index);
}

std::vector<std::string> Mempool::getDependents(const std::string& txid) const {
    auto it = dependencies.find(txid);
    if (it != dependencies.end()) {
        return std::vector<std::string>(it->second.begin(), it->second.end());
    }
    return {};
}

std::vector<std::string> Mempool::getDependencies(const std::string& txid) const {
    auto entry = getTransaction(txid);
    return entry ? entry->depends : std::vector<std::string>();
}

bool Mempool::canBeIncluded(const std::string& txid, const std::unordered_set<std::string>& included) const {
    auto entry = getTransaction(txid);
    if (!entry) return false;
    
    for (const auto& depTxid : entry->depends) {
        if (included.find(depTxid) == included.end()) {
            return false;
        }
    }
    return true;
}

uint64_t Mempool::estimateFeeRate(uint32_t targetBlocks) const {
    if (transactions.empty()) {
        return minFeeRate;
    }
    
    std::vector<uint64_t> feeRates;
    for (const auto& [txid, entry] : transactions) {
        feeRates.push_back(entry->feeRate);
    }
    
    std::sort(feeRates.rbegin(), feeRates.rend()); // Highest first
    
    // Estimate based on target confirmation time
    size_t index = std::min(static_cast<size_t>(targetBlocks * 100), feeRates.size() - 1);
    return feeRates[index];
}

// Miner implementation
Miner::Miner(Mempool* pool, UTXOSet* utxos, ChainState* chain, BlockValidator* val,
             const std::string& address)
    : mempool(pool), utxoSet(utxos), chainState(chain), validator(val),
      minerAddress(address), maxBlockSize(1000000), maxTransactions(2000) {
}

BlockTemplate Miner::createBlockTemplate(uint32_t currentHeight) const {
    BlockTemplate blockTemplate;
    
    // Get transactions from mempool
    std::vector<Transaction> mempoolTxs = mempool->selectTransactions(maxBlockSize - 1000, currentHeight);
    
    // Calculate total fees
    uint64_t totalFees = 0;
    for (const auto& tx : mempoolTxs) {
        auto entry = mempool->getTransaction(tx.txid);
        if (entry) {
            totalFees += entry->fee;
        }
    }
    
    // Create coinbase transaction
    Transaction coinbaseTx;
    coinbaseTx.isCoinbase = true;
    
    // Coinbase input
    TxIn coinbaseInput;
    coinbaseInput.prevout = OutPoint{"", UINT32_MAX};
    coinbaseInput.sig = "CoinbaseScript" + std::to_string(currentHeight);
    coinbaseInput.pubKey = "";
    coinbaseTx.vin.push_back(coinbaseInput);
    
    // Coinbase output
    uint64_t blockReward = calculateBlockSubsidy(currentHeight);
    TxOut coinbaseOutput;
    coinbaseOutput.value = blockReward + totalFees;
    coinbaseOutput.pubKeyHash = minerAddress;
    coinbaseTx.vout.push_back(coinbaseOutput);
    
    // Update coinbase transaction ID
    coinbaseTx.txid = Hash::sha256(coinbaseTx.serialize());
    
    // Build transaction list (coinbase first)
    blockTemplate.transactions.clear();
    blockTemplate.transactions.push_back(coinbaseTx);
    blockTemplate.transactions.insert(blockTemplate.transactions.end(), 
                                     mempoolTxs.begin(), mempoolTxs.end());
    
    // Create block header
    blockTemplate.header.version = 1;
    blockTemplate.header.prevHash = chainState->getBestHash();
    blockTemplate.header.timestamp = std::time(nullptr);
    blockTemplate.header.bits = 0x1d00ffff; // Default difficulty for now
    blockTemplate.header.nonce = 0;
    
    // Calculate merkle root
    blockTemplate.updateMerkleRoot();
    
    // Set template properties
    blockTemplate.totalFees = totalFees;
    blockTemplate.blockReward = blockReward;
    blockTemplate.transactionCount = blockTemplate.transactions.size();
    
    // Calculate block size
    blockTemplate.blockSize = 80; // Header size
    for (const auto& tx : blockTemplate.transactions) {
        blockTemplate.blockSize += tx.serialize().size();
    }
    
    miningStats.blocksCreated++;
    
    return blockTemplate;
}

BlockTemplate Miner::createBlockTemplate(const std::vector<Transaction>& specificTxs, uint32_t currentHeight) const {
    BlockTemplate blockTemplate;
    
    // Calculate total fees
    uint64_t totalFees = 0;
    for (const auto& tx : specificTxs) {
        auto entry = mempool->getTransaction(tx.txid);
        if (entry) {
            totalFees += entry->fee;
        }
    }
    
    // Create coinbase transaction (similar to above)
    Transaction coinbaseTx;
    coinbaseTx.isCoinbase = true;
    
    TxIn coinbaseInput;
    coinbaseInput.prevout = OutPoint{"", UINT32_MAX};
    coinbaseInput.sig = "CoinbaseScript" + std::to_string(currentHeight);
    coinbaseInput.pubKey = "";
    coinbaseTx.vin.push_back(coinbaseInput);
    
    uint64_t blockReward = calculateBlockSubsidy(currentHeight);
    TxOut coinbaseOutput;
    coinbaseOutput.value = blockReward + totalFees;
    coinbaseOutput.pubKeyHash = minerAddress;
    coinbaseTx.vout.push_back(coinbaseOutput);
    
    coinbaseTx.txid = Hash::sha256(coinbaseTx.serialize());
    
    // Build transaction list
    blockTemplate.transactions.push_back(coinbaseTx);
    blockTemplate.transactions.insert(blockTemplate.transactions.end(), 
                                     specificTxs.begin(), specificTxs.end());
    
    // Create block header
    blockTemplate.header.version = 1;
    blockTemplate.header.prevHash = chainState->getBestHash();
    blockTemplate.header.timestamp = std::time(nullptr);
    blockTemplate.header.bits = 0x1d00ffff; // Default difficulty for now
    blockTemplate.header.nonce = 0;
    
    blockTemplate.updateMerkleRoot();
    
    blockTemplate.totalFees = totalFees;
    blockTemplate.blockReward = blockReward;
    blockTemplate.transactionCount = blockTemplate.transactions.size();
    
    blockTemplate.blockSize = 80;
    for (const auto& tx : blockTemplate.transactions) {
        blockTemplate.blockSize += tx.serialize().size();
    }
    
    miningStats.blocksCreated++;
    
    return blockTemplate;
}

Block Miner::mineBlock(uint32_t maxIterations) const {
    auto blockTemplate = createBlockTemplate(chainState->getBestHeight() + 1);
    return mineBlockTemplate(blockTemplate, maxIterations);
}

Block Miner::mineBlockTemplate(const BlockTemplate& blockTemplate, uint32_t maxIterations) const {
    Block block = blockTemplate.createBlock();
    
    auto startTime = std::time(nullptr);
    uint32_t hashAttempts = 0;
    
    for (uint32_t nonce = 0; nonce < maxIterations; nonce++) {
        block.header.nonce = nonce;
        hashAttempts++;
        
        std::string hash = block.header.computeHash();
        if (Difficulty::meetsTarget(hash, block.header.bits)) {
            auto endTime = std::time(nullptr);
            miningStats.blocksMined++;
            miningStats.totalHashAttempts += hashAttempts;
            miningStats.totalMiningTime += (endTime - startTime);
            miningStats.totalFeesEarned += blockTemplate.totalFees;
            miningStats.totalSubsidyEarned += blockTemplate.blockReward;
            
            return block;
        }
    }
    
    // Mining failed - return block with highest nonce attempted
    miningStats.totalHashAttempts += hashAttempts;
    return block;
}

bool Miner::mineBlockInPlace(Block& block, uint32_t maxIterations) const {
    auto startTime = std::time(nullptr);
    uint32_t hashAttempts = 0;
    
    for (uint32_t nonce = 0; nonce < maxIterations; nonce++) {
        block.header.nonce = nonce;
        hashAttempts++;
        
        std::string hash = block.header.computeHash();
        if (Difficulty::meetsTarget(hash, block.header.bits)) {
            auto endTime = std::time(nullptr);
            miningStats.blocksMined++;
            miningStats.totalHashAttempts += hashAttempts;
            miningStats.totalMiningTime += (endTime - startTime);
            
            return true;
        }
    }
    
    miningStats.totalHashAttempts += hashAttempts;
    return false;
}

uint64_t Miner::calculateBlockSubsidy(uint32_t height) const {
    uint64_t subsidy = 5000000000; // 50 coins in satoshis
    
    // Halving every 210,000 blocks (like Bitcoin)
    uint32_t halvings = height / 210000;
    
    // Ensure subsidy doesn't become too small
    if (halvings >= 32) {
        return 0;
    }
    
    return subsidy >> halvings;
}

bool Miner::isValidBlockTemplate(const BlockTemplate& blockTemplate) const {
    if (!validator) {
        return false;
    }
    
    Block block = blockTemplate.createBlock();
    auto result = validator->validateBlock(block, chainState->getBestHeight() + 1);
    return result.isValid;
}

void Miner::printMiningStats() const {
    std::cout << "\n=== Mining Statistics ===" << std::endl;
    std::cout << "Blocks Created: " << miningStats.blocksCreated << std::endl;
    std::cout << "Blocks Mined: " << miningStats.blocksMined << std::endl;
    std::cout << "Total Hash Attempts: " << miningStats.totalHashAttempts << std::endl;
    std::cout << "Total Mining Time: " << miningStats.totalMiningTime << " seconds" << std::endl;
    std::cout << "Total Fees Earned: " << miningStats.totalFeesEarned << " satoshis" << std::endl;
    std::cout << "Total Subsidy Earned: " << miningStats.totalSubsidyEarned << " satoshis" << std::endl;
    
    if (miningStats.blocksMined > 0) {
        std::cout << "Average Hashes per Block: " << 
                     (miningStats.totalHashAttempts / miningStats.blocksMined) << std::endl;
        std::cout << "Average Mining Time per Block: " << 
                     (miningStats.totalMiningTime / miningStats.blocksMined) << " seconds" << std::endl;
    }
}

} // namespace pragma
