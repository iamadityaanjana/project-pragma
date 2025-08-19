#include "chainstate.h"
#include "difficulty.h"
#include "../primitives/utils.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <sstream>

namespace pragma {

ChainState::ChainState() 
    : bestChainTip(nullptr), totalBlocks(0) {
}

bool ChainState::setGenesis(const Block& genesisBlock) {
    if (hasGenesis()) {
        Utils::logWarning("Genesis block already set");
        return false;
    }
    
    if (!genesisBlock.isValid()) {
        Utils::logError("Invalid genesis block");
        return false;
    }
    
    if (genesisBlock.header.prevHash != "0000000000000000000000000000000000000000000000000000000000000000") {
        Utils::logError("Genesis block must have null previous hash");
        return false;
    }
    
    // Create genesis chain entry
    uint64_t work = calculateBlockWork(genesisBlock.header.bits);
    std::string cumulativeWork = std::to_string(work);
    
    auto genesisEntry = std::make_shared<ChainEntry>(genesisBlock, 0, cumulativeWork, work);
    
    blocks[genesisBlock.hash] = genesisEntry;
    bestChainTip = genesisEntry;
    genesisHash = genesisBlock.hash;
    totalBlocks = 1;
    
    Utils::logInfo("Genesis block set: " + genesisBlock.hash);
    return true;
}

bool ChainState::hasGenesis() const {
    return !genesisHash.empty() && blocks.find(genesisHash) != blocks.end();
}

bool ChainState::addBlock(const Block& block) {
    // Check if we already have this block
    if (blocks.find(block.hash) != blocks.end()) {
        Utils::logWarning("Block already exists: " + block.hash);
        return false;
    }
    
    // Validate block structure
    if (!isValidBlock(block)) {
        Utils::logError("Invalid block structure: " + block.hash);
        return false;
    }
    
    // Find previous block
    auto prevIt = blocks.find(block.header.prevHash);
    if (prevIt == blocks.end()) {
        Utils::logError("Previous block not found: " + block.header.prevHash);
        return false;
    }
    
    auto prevEntry = prevIt->second;
    
    // Validate connection to previous block
    if (!isValidConnection(block, *prevEntry)) {
        Utils::logError("Invalid block connection: " + block.hash);
        return false;
    }
    
    // Calculate work and height
    uint32_t newHeight = prevEntry->height + 1;
    uint64_t blockWork = calculateBlockWork(block.header.bits);
    uint64_t totalWork = prevEntry->totalWork + blockWork;
    std::string cumulativeWork = calculateCumulativeWork(prevEntry->cumulativeWork, block.header.bits);
    
    // Create new chain entry
    auto newEntry = std::make_shared<ChainEntry>(block, newHeight, cumulativeWork, totalWork);
    blocks[block.hash] = newEntry;
    totalBlocks++;
    
    Utils::logInfo("Added block " + block.hash + " at height " + std::to_string(newHeight));
    
    // Check if this extends the best chain or creates a better chain
    if (!bestChainTip || totalWork > bestChainTip->totalWork) {
        Utils::logInfo("New best chain tip: " + block.hash);
        bestChainTip = newEntry;
        return true;
    }
    
    // Check if we need to reorganize
    if (isChainBetter(block.hash)) {
        return reorganizeChain(block.hash);
    }
    
    return true;
}

bool ChainState::connectBlock(const Block& block) {
    return addBlock(block);
}

bool ChainState::disconnectBlock(const std::string& blockHash) {
    auto it = blocks.find(blockHash);
    if (it == blocks.end()) {
        Utils::logError("Block not found for disconnect: " + blockHash);
        return false;
    }
    
    auto entry = it->second;
    
    // Cannot disconnect genesis
    if (entry->height == 0) {
        Utils::logError("Cannot disconnect genesis block");
        return false;
    }
    
    // If this is the best tip, need to find new tip
    if (bestChainTip && bestChainTip->block.hash == blockHash) {
        auto prevEntry = blocks.find(entry->block.header.prevHash);
        if (prevEntry != blocks.end()) {
            bestChainTip = prevEntry->second;
            Utils::logInfo("New best tip after disconnect: " + bestChainTip->block.hash);
        }
    }
    
    // Remove block (for now, keep it but could implement proper rollback)
    Utils::logInfo("Disconnected block: " + blockHash);
    return true;
}

std::shared_ptr<ChainEntry> ChainState::getBlock(const std::string& hash) const {
    auto it = blocks.find(hash);
    return (it != blocks.end()) ? it->second : nullptr;
}

std::shared_ptr<ChainEntry> ChainState::getBestTip() const {
    return bestChainTip;
}

std::string ChainState::getBestHash() const {
    return bestChainTip ? bestChainTip->block.hash : "";
}

uint32_t ChainState::getBestHeight() const {
    return bestChainTip ? bestChainTip->height : 0;
}

uint64_t ChainState::getTotalBlocks() const {
    return totalBlocks;
}

bool ChainState::isValidBlock(const Block& block) const {
    // Basic structural validation
    if (!validateBlockStructure(block)) {
        return false;
    }
    
    // Validate timestamp
    if (!validateTimestamp(block)) {
        return false;
    }
    
    // Validate difficulty
    if (!validateDifficulty(block)) {
        return false;
    }
    
    // Validate proof of work
    if (!block.meetsTarget()) {
        Utils::logError("Block does not meet difficulty target");
        return false;
    }
    
    return true;
}

bool ChainState::isValidConnection(const Block& block, const ChainEntry& prevEntry) const {
    // Check height progression
    if (block.header.timestamp <= prevEntry.block.header.timestamp) {
        Utils::logError("Block timestamp not increasing");
        return false;
    }
    
    // Check difficulty transition (simplified)
    // In full implementation, would check retarget rules
    
    return true;
}

bool ChainState::validateBlockStructure(const Block& block) const {
    return block.isValid();
}

bool ChainState::validateTimestamp(const Block& block) const {
    // Check timestamp is not too far in the future
    uint64_t currentTime = Utils::getCurrentTimestamp();
    uint64_t maxFutureTime = currentTime + 7200; // 2 hours drift tolerance
    
    if (block.header.timestamp > maxFutureTime) {
        Utils::logError("Block timestamp too far in future");
        return false;
    }
    
    // For non-genesis blocks, check against median of last 11 blocks
    if (bestChainTip && bestChainTip->height >= 11) {
        // Simplified: just check it's after previous block
        if (block.header.timestamp <= bestChainTip->block.header.timestamp) {
            Utils::logError("Block timestamp not after previous");
            return false;
        }
    }
    
    return true;
}

bool ChainState::validateDifficulty(const Block& block) const {
    // Simplified difficulty validation
    // In full implementation, would check retarget rules every 2016 blocks
    
    if (!bestChainTip) {
        return true; // Genesis block
    }
    
    // For now, just ensure reasonable difficulty
    if (block.header.bits < 0x1d007fff || block.header.bits > 0x207fffff) {
        Utils::logError("Block difficulty out of valid range");
        return false;
    }
    
    return true;
}

uint64_t ChainState::calculateBlockWork(uint32_t bits) const {
    // Simplified work calculation
    // Real Bitcoin uses: work = 2^256 / (target + 1)
    // We'll use a simplified version for now
    
    std::string target = Difficulty::bitsToTarget(bits);
    
    // Count leading zeros as a simple work metric
    uint64_t work = 0;
    for (char c : target) {
        if (c == '0') {
            work += 1;
        } else {
            break;
        }
    }
    
    // Base work to ensure all blocks contribute
    return 1000000 + work * 10000;
}

std::string ChainState::calculateCumulativeWork(const std::string& prevWork, uint32_t bits) const {
    uint64_t prev = std::stoull(prevWork);
    uint64_t blockWork = calculateBlockWork(bits);
    return std::to_string(prev + blockWork);
}

bool ChainState::isChainBetter(const std::string& candidateHash) const {
    auto candidate = getBlock(candidateHash);
    if (!candidate || !bestChainTip) {
        return false;
    }
    
    return candidate->totalWork > bestChainTip->totalWork;
}

bool ChainState::reorganizeChain(const std::string& newTipHash) {
    auto newTip = getBlock(newTipHash);
    if (!newTip) {
        Utils::logError("Cannot reorganize to unknown block: " + newTipHash);
        return false;
    }
    
    if (!bestChainTip) {
        bestChainTip = newTip;
        Utils::logInfo("Set initial chain tip: " + newTipHash);
        return true;
    }
    
    Utils::logInfo("Reorganizing chain from " + bestChainTip->block.hash + " to " + newTipHash);
    
    // Find fork point
    auto forkPath = findForkPoint(bestChainTip->block.hash, newTipHash);
    
    if (forkPath.empty()) {
        Utils::logError("No fork point found during reorganization");
        return false;
    }
    
    // For simplified implementation, just switch tip
    // In full implementation, would disconnect old blocks and connect new ones
    bestChainTip = newTip;
    
    Utils::logInfo("Chain reorganization complete. New tip: " + newTipHash);
    return true;
}

std::vector<std::string> ChainState::findForkPoint(const std::string& hash1, const std::string& hash2) const {
    std::vector<std::string> path1, path2;
    
    // Build paths back to genesis
    std::string current = hash1;
    while (!current.empty() && current != genesisHash) {
        path1.push_back(current);
        auto entry = getBlock(current);
        if (!entry) break;
        current = entry->block.header.prevHash;
    }
    
    current = hash2;
    while (!current.empty() && current != genesisHash) {
        path2.push_back(current);
        auto entry = getBlock(current);
        if (!entry) break;
        current = entry->block.header.prevHash;
    }
    
    // Find common point
    std::reverse(path1.begin(), path1.end());
    std::reverse(path2.begin(), path2.end());
    
    size_t forkPoint = 0;
    while (forkPoint < path1.size() && forkPoint < path2.size() && 
           path1[forkPoint] == path2[forkPoint]) {
        forkPoint++;
    }
    
    if (forkPoint > 0) {
        return {path1[forkPoint - 1]}; // Return fork point
    }
    
    return {}; // No common point found
}

std::vector<std::string> ChainState::getChainPath(const std::string& fromHash, const std::string& toHash) const {
    std::vector<std::string> path;
    
    std::string current = toHash;
    while (!current.empty() && current != fromHash) {
        path.push_back(current);
        auto entry = getBlock(current);
        if (!entry) break;
        current = entry->block.header.prevHash;
    }
    
    if (current == fromHash) {
        path.push_back(fromHash);
        std::reverse(path.begin(), path.end());
    }
    
    return path;
}

std::vector<std::shared_ptr<ChainEntry>> ChainState::getChain(uint32_t maxHeight) const {
    std::vector<std::shared_ptr<ChainEntry>> chain;
    
    if (!bestChainTip) {
        return chain;
    }
    
    std::string current = bestChainTip->block.hash;
    while (!current.empty()) {
        auto entry = getBlock(current);
        if (!entry) break;
        
        chain.push_back(entry);
        
        if (entry->height == 0 || (maxHeight > 0 && chain.size() >= maxHeight)) {
            break;
        }
        
        current = entry->block.header.prevHash;
    }
    
    std::reverse(chain.begin(), chain.end());
    return chain;
}

ChainState::ChainStats ChainState::getChainStats() const {
    ChainStats stats = {};
    
    if (bestChainTip) {
        stats.height = bestChainTip->height;
        stats.bestHash = bestChainTip->block.hash;
        stats.totalWork = std::to_string(bestChainTip->totalWork);
        stats.difficulty = bestChainTip->block.header.bits;
        
        // Calculate average block time (simplified)
        if (stats.height > 0 && hasGenesis()) {
            auto genesis = getBlock(genesisHash);
            if (genesis) {
                uint64_t timeDiff = bestChainTip->block.header.timestamp - genesis->block.header.timestamp;
                stats.averageBlockTime = static_cast<double>(timeDiff) / stats.height;
            }
        }
    }
    
    stats.totalBlocks = totalBlocks;
    return stats;
}

void ChainState::printChain() const {
    auto chain = getChain();
    
    std::cout << "\n=== Current Chain ===\n";
    std::cout << "Total blocks: " << totalBlocks << std::endl;
    std::cout << "Best height: " << getBestHeight() << std::endl;
    std::cout << "Best hash: " << getBestHash() << std::endl;
    
    for (const auto& entry : chain) {
        std::cout << "Height " << entry->height 
                  << ": " << entry->block.hash.substr(0, 16) << "..."
                  << " (work: " << entry->totalWork << ")"
                  << " (txs: " << entry->block.transactions.size() << ")"
                  << std::endl;
    }
}

std::vector<std::string> ChainState::getBlockHashes() const {
    std::vector<std::string> hashes;
    for (const auto& pair : blocks) {
        hashes.push_back(pair.first);
    }
    return hashes;
}

bool ChainState::saveToFile(const std::string& filename) const {
    try {
        std::ofstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            Utils::logError("Cannot open file for writing: " + filename);
            return false;
        }
        
        // Save basic metadata
        file.write(reinterpret_cast<const char*>(&totalBlocks), sizeof(totalBlocks));
        
        // Save genesis hash
        size_t genesisSize = genesisHash.size();
        file.write(reinterpret_cast<const char*>(&genesisSize), sizeof(genesisSize));
        file.write(genesisHash.c_str(), genesisSize);
        
        // Save best tip hash
        std::string bestHash = getBestHash();
        size_t bestSize = bestHash.size();
        file.write(reinterpret_cast<const char*>(&bestSize), sizeof(bestSize));
        file.write(bestHash.c_str(), bestSize);
        
        // Save all blocks
        size_t blockCount = blocks.size();
        file.write(reinterpret_cast<const char*>(&blockCount), sizeof(blockCount));
        
        for (const auto& pair : blocks) {
            const auto& entry = pair.second;
            
            // Serialize block
            auto blockData = entry->block.serialize();
            size_t blockSize = blockData.size();
            file.write(reinterpret_cast<const char*>(&blockSize), sizeof(blockSize));
            file.write(reinterpret_cast<const char*>(blockData.data()), blockSize);
            
            // Save metadata
            file.write(reinterpret_cast<const char*>(&entry->height), sizeof(entry->height));
            file.write(reinterpret_cast<const char*>(&entry->totalWork), sizeof(entry->totalWork));
            
            size_t workSize = entry->cumulativeWork.size();
            file.write(reinterpret_cast<const char*>(&workSize), sizeof(workSize));
            file.write(entry->cumulativeWork.c_str(), workSize);
        }
        
        file.close();
        Utils::logInfo("Chain state saved to: " + filename);
        return true;
        
    } catch (const std::exception& e) {
        Utils::logError("Error saving chain state: " + std::string(e.what()));
        return false;
    }
}

bool ChainState::loadFromFile(const std::string& filename) {
    try {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            Utils::logWarning("Cannot open file for reading: " + filename);
            return false;
        }
        
        // Clear current state
        blocks.clear();
        bestChainTip = nullptr;
        genesisHash.clear();
        totalBlocks = 0;
        
        // Load basic metadata
        file.read(reinterpret_cast<char*>(&totalBlocks), sizeof(totalBlocks));
        
        // Load genesis hash
        size_t genesisSize;
        file.read(reinterpret_cast<char*>(&genesisSize), sizeof(genesisSize));
        genesisHash.resize(genesisSize);
        file.read(&genesisHash[0], genesisSize);
        
        // Load best tip hash
        size_t bestSize;
        file.read(reinterpret_cast<char*>(&bestSize), sizeof(bestSize));
        std::string bestHash(bestSize, '\0');
        file.read(&bestHash[0], bestSize);
        
        // Load all blocks
        size_t blockCount;
        file.read(reinterpret_cast<char*>(&blockCount), sizeof(blockCount));
        
        for (size_t i = 0; i < blockCount; ++i) {
            // Load block data
            size_t blockSize;
            file.read(reinterpret_cast<char*>(&blockSize), sizeof(blockSize));
            
            std::vector<uint8_t> blockData(blockSize);
            file.read(reinterpret_cast<char*>(blockData.data()), blockSize);
            
            Block block = Block::deserialize(blockData);
            
            // Load metadata
            uint32_t height;
            uint64_t totalWork;
            file.read(reinterpret_cast<char*>(&height), sizeof(height));
            file.read(reinterpret_cast<char*>(&totalWork), sizeof(totalWork));
            
            size_t workSize;
            file.read(reinterpret_cast<char*>(&workSize), sizeof(workSize));
            std::string cumulativeWork(workSize, '\0');
            file.read(&cumulativeWork[0], workSize);
            
            // Create entry
            auto entry = std::make_shared<ChainEntry>(block, height, cumulativeWork, totalWork);
            blocks[block.hash] = entry;
            
            // Set best tip if this is it
            if (block.hash == bestHash) {
                bestChainTip = entry;
            }
        }
        
        file.close();
        Utils::logInfo("Chain state loaded from: " + filename);
        Utils::logInfo("Loaded " + std::to_string(blockCount) + " blocks");
        return true;
        
    } catch (const std::exception& e) {
        Utils::logError("Error loading chain state: " + std::string(e.what()));
        return false;
    }
}

} // namespace pragma
