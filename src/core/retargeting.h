#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <memory>
#include "block.h"
#include "difficulty.h"

namespace pragma {

// Forward declaration
class ChainState;

/**
 * Difficulty retargeting manager
 * Handles various difficulty adjustment algorithms and blockchain analysis
 */
class DifficultyRetargeting {
public:
    enum class Algorithm {
        BASIC,      // Simple timespan-based adjustment
        LINEAR,     // Linear averaging over window
        EMA,        // Exponential moving average
        ADAPTIVE    // Adaptive algorithm based on hashrate trends
    };
    
    struct RetargetData {
        uint32_t height;
        uint64_t timestamp;
        uint32_t bits;
        std::string blockHash;
        uint64_t blockTime; // Time since previous block
        double difficulty;  // Calculated difficulty
        double workRequired; // Estimated work for this block
    };
    
    struct RetargetResult {
        uint32_t newBits;
        double difficultyChange; // Ratio of new/old difficulty
        uint64_t expectedTimespan;
        uint64_t actualTimespan;
        Algorithm algorithmUsed;
        std::string reason;
        bool wasAdjusted;
    };
    
private:
    Algorithm currentAlgorithm;
    std::vector<RetargetData> blockHistory;
    uint32_t maxHistorySize;
    
    // Analysis helpers
    double calculateDifficulty(uint32_t bits) const;
    double calculateWork(uint32_t bits) const;
    bool shouldRetarget(uint32_t height) const;
    std::vector<uint64_t> extractBlockTimes() const;
    std::vector<uint32_t> extractBlockBits() const;
    
    // Algorithm implementations
    RetargetResult retargetBasic(uint32_t currentHeight) const;
    RetargetResult retargetLinear(uint32_t currentHeight) const;
    RetargetResult retargetEMA(uint32_t currentHeight) const;
    RetargetResult retargetAdaptive(uint32_t currentHeight) const;
    
public:
    DifficultyRetargeting(Algorithm algo = Algorithm::BASIC, uint32_t historySize = 100);
    ~DifficultyRetargeting() = default;
    
    // Core functionality
    void addBlock(const Block& block, uint32_t height);
    RetargetResult calculateRetarget(uint32_t currentHeight) const;
    uint32_t getNextWorkRequired(uint32_t currentHeight) const;
    
    // Algorithm management
    void setAlgorithm(Algorithm algo);
    Algorithm getAlgorithm() const;
    
    // Block history management
    void clearHistory();
    size_t getHistorySize() const;
    const std::vector<RetargetData>& getHistory() const;
    
    // Validation helpers - public for testing
    bool isValidAdjustment(uint32_t oldBits, uint32_t newBits) const;
    uint32_t applySafetyLimits(uint32_t oldBits, uint32_t newBits) const;
    
    // Chain analysis
    struct ChainAnalysis {
        double avgBlockTime;
        double currentHashrate;
        double hashrateChange;
        double difficultyTrend;
        uint32_t blocksToRetarget;
        bool isStable;
    };
    
    ChainAnalysis analyzeChain() const;
    
    // Statistics and reporting
    struct RetargetStats {
        uint32_t totalRetargets;
        uint32_t upwardAdjustments;
        uint32_t downwardAdjustments;
        double avgAdjustmentSize;
        double maxAdjustmentSize;
        double avgBlockTime;
        double targetAccuracy; // How close to target block time
    };
    
    RetargetStats getRetargetStats() const;
    
    // Simulation and testing
    std::vector<RetargetResult> simulateRetargeting(
        const std::vector<Block>& blocks,
        const std::vector<uint32_t>& heights,
        Algorithm algorithm = Algorithm::BASIC) const;
    
    // Validation helpers
    bool validateRetargetResult(const RetargetResult& result) const;
    bool isRetargetNeeded(uint32_t height) const;
    
    // Debug and logging
    void printHistory() const;
    void printRetargetResult(const RetargetResult& result) const;
    std::string algorithmToString(Algorithm algo) const;
    
    // Integration with ChainState
    void syncWithChainState(const ChainState& chainState);
    
    // Constants
    static const uint32_t DEFAULT_HISTORY_SIZE = 100;
    static constexpr double TARGET_ACCURACY_THRESHOLD = 0.1; // 10% tolerance
    static const uint32_t MIN_BLOCKS_FOR_ANALYSIS = 5;
};

} // namespace pragma
