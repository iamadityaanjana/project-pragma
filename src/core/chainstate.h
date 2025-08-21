#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include <optional>
#include "block.h"

namespace pragma {

/**
 * Represents a block in the chain with metadata
 */
struct ChainEntry {
    Block block;
    uint32_t height;
    std::string cumulativeWork; // Hex string representing cumulative work
    uint64_t totalWork;         // Simplified work counter for now
    
    ChainEntry() = default;
    ChainEntry(const Block& b, uint32_t h, const std::string& work, uint64_t total)
        : block(b), height(h), cumulativeWork(work), totalWork(total) {}
};

/**
 * Chain state management - tracks the blockchain and manages chain selection
 */
class ChainState {
private:
    std::unordered_map<std::string, std::shared_ptr<ChainEntry>> blocks; // hash -> entry
    std::shared_ptr<ChainEntry> bestChainTip;
    std::string genesisHash;
    uint64_t totalBlocks;
    
    // Chain validation helpers
    bool isValidChain(const std::vector<std::string>& hashes) const;
    uint64_t calculateBlockWork(uint32_t bits) const;
    std::string calculateCumulativeWork(const std::string& prevWork, uint32_t bits) const;
    
public:
    ChainState();
    ~ChainState() = default;
    
    // Core chain operations
    bool addBlock(const Block& block);
    bool connectBlock(const Block& block);
    bool disconnectBlock(const std::string& blockHash);
    
    // Chain queries
    std::shared_ptr<ChainEntry> getBlock(const std::string& hash) const;
    std::shared_ptr<ChainEntry> getBestTip() const;
    std::string getBestHash() const;
    uint32_t getBestHeight() const;
    uint64_t getTotalBlocks() const;
    
    // Chain validation
    bool isValidBlock(const Block& block) const;
    bool isValidConnection(const Block& block, const ChainEntry& prevEntry) const;
    
    // Chain reorganization
    bool reorganizeChain(const std::string& newTipHash);
    std::vector<std::string> findForkPoint(const std::string& hash1, const std::string& hash2) const;
    
    // Chain traversal
    std::vector<std::string> getChainPath(const std::string& fromHash, const std::string& toHash) const;
    std::vector<std::shared_ptr<ChainEntry>> getChain(uint32_t maxHeight = 0) const;
    const Block* getBlockByHeight(uint32_t height) const;
    const Block* getBlockByHash(const std::string& hash) const;
    
    // Genesis and initialization
    bool setGenesis(const Block& genesisBlock);
    bool hasGenesis() const;
    
    // Persistence (simplified for now)
    bool saveToFile(const std::string& filename) const;
    bool loadFromFile(const std::string& filename);
    
    // Chain statistics
    struct ChainStats {
        uint32_t height;
        uint64_t totalBlocks;
        std::string bestHash;
        std::string totalWork;
        double averageBlockTime;
        uint32_t difficulty;
    };
    
    ChainStats getChainStats() const;
    
    // Chain selection (heaviest chain rule)
    bool isChainBetter(const std::string& candidateHash) const;
    
    // Validation helpers
    bool validateTimestamp(const Block& block) const;
    bool validateDifficulty(const Block& block) const;
    bool validateBlockStructure(const Block& block) const;
    
    // Debug utilities
    void printChain() const;
    std::vector<std::string> getBlockHashes() const;
};

} // namespace pragma
