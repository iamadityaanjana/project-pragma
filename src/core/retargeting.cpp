#include "retargeting.h"
#include "chainstate.h"
#include "../primitives/utils.h"
#include <algorithm>
#include <numeric>
#include <iomanip>
#include <iostream>

namespace pragma {

DifficultyRetargeting::DifficultyRetargeting(Algorithm algo, uint32_t historySize)
    : currentAlgorithm(algo), maxHistorySize(historySize) {
    blockHistory.reserve(historySize);
}

void DifficultyRetargeting::addBlock(const Block& block, uint32_t height) {
    RetargetData data;
    data.height = height;
    data.timestamp = block.header.timestamp;
    data.bits = block.header.bits;
    data.blockHash = block.hash;
    data.difficulty = calculateDifficulty(block.header.bits);
    data.workRequired = calculateWork(block.header.bits);
    
    // Calculate block time (time since previous block)
    if (!blockHistory.empty()) {
        data.blockTime = data.timestamp - blockHistory.back().timestamp;
    } else {
        data.blockTime = Difficulty::TARGET_BLOCK_TIME; // Assume target for genesis
    }
    
    blockHistory.push_back(data);
    
    // Maintain history size limit
    if (blockHistory.size() > maxHistorySize) {
        blockHistory.erase(blockHistory.begin());
    }
}

DifficultyRetargeting::RetargetResult DifficultyRetargeting::calculateRetarget(uint32_t currentHeight) const {
    if (blockHistory.empty()) {
        return {Difficulty::MAX_BITS, 1.0, 0, 0, Algorithm::BASIC, "No block history", false};
    }
    
    if (!shouldRetarget(currentHeight)) {
        uint32_t currentBits = blockHistory.back().bits;
        return {currentBits, 1.0, 0, 0, currentAlgorithm, "No retarget needed", false};
    }
    
    switch (currentAlgorithm) {
        case Algorithm::BASIC:
            return retargetBasic(currentHeight);
        case Algorithm::LINEAR:
            return retargetLinear(currentHeight);
        case Algorithm::EMA:
            return retargetEMA(currentHeight);
        case Algorithm::ADAPTIVE:
            return retargetAdaptive(currentHeight);
        default:
            return retargetBasic(currentHeight);
    }
}

uint32_t DifficultyRetargeting::getNextWorkRequired(uint32_t currentHeight) const {
    RetargetResult result = calculateRetarget(currentHeight);
    return result.newBits;
}

DifficultyRetargeting::RetargetResult DifficultyRetargeting::retargetBasic(uint32_t currentHeight) const {
    if (blockHistory.size() < Difficulty::RETARGET_WINDOW) {
        uint32_t currentBits = blockHistory.back().bits;
        return {currentBits, 1.0, 0, 0, Algorithm::BASIC, "Insufficient history", false};
    }
    
    // Calculate actual timespan over retarget window
    size_t windowStart = blockHistory.size() - Difficulty::RETARGET_WINDOW;
    uint64_t actualTimespan = blockHistory.back().timestamp - blockHistory[windowStart].timestamp;
    uint64_t expectedTimespan = Difficulty::TARGET_BLOCK_TIME * (Difficulty::RETARGET_WINDOW - 1);
    
    uint32_t oldBits = blockHistory.back().bits;
    uint32_t newBits = Difficulty::retargetBasic(oldBits, actualTimespan, expectedTimespan);
    
    // Apply safety limits
    newBits = applySafetyLimits(oldBits, newBits);
    
    double oldDifficulty = calculateDifficulty(oldBits);
    double newDifficulty = calculateDifficulty(newBits);
    double difficultyChange = newDifficulty / oldDifficulty;
    
    std::string reason = actualTimespan > expectedTimespan ? "Blocks too slow - decrease difficulty" : 
                        actualTimespan < expectedTimespan ? "Blocks too fast - increase difficulty" : "Stable";
    
    return {newBits, difficultyChange, expectedTimespan, actualTimespan, Algorithm::BASIC, reason, true};
}

DifficultyRetargeting::RetargetResult DifficultyRetargeting::retargetLinear(uint32_t currentHeight) const {
    if (blockHistory.size() < Difficulty::RETARGET_WINDOW) {
        uint32_t currentBits = blockHistory.back().bits;
        return {currentBits, 1.0, 0, 0, Algorithm::LINEAR, "Insufficient history", false};
    }
    
    // Calculate average block time over window
    size_t windowStart = blockHistory.size() - Difficulty::RETARGET_WINDOW;
    uint64_t totalTime = 0;
    uint32_t intervals = 0;
    
    for (size_t i = windowStart + 1; i < blockHistory.size(); ++i) {
        totalTime += blockHistory[i].blockTime;
        intervals++;
    }
    
    uint64_t avgBlockTime = intervals > 0 ? totalTime / intervals : Difficulty::TARGET_BLOCK_TIME;
    uint64_t actualTimespan = avgBlockTime * intervals;
    uint64_t expectedTimespan = Difficulty::TARGET_BLOCK_TIME * intervals;
    
    uint32_t oldBits = blockHistory.back().bits;
    uint32_t newBits = Difficulty::retargetBasic(oldBits, actualTimespan, expectedTimespan);
    newBits = applySafetyLimits(oldBits, newBits);
    
    double difficultyChange = calculateDifficulty(newBits) / calculateDifficulty(oldBits);
    
    return {newBits, difficultyChange, expectedTimespan, actualTimespan, Algorithm::LINEAR, "Linear averaging", true};
}

DifficultyRetargeting::RetargetResult DifficultyRetargeting::retargetEMA(uint32_t currentHeight) const {
    if (blockHistory.size() < Difficulty::RETARGET_WINDOW) {
        uint32_t currentBits = blockHistory.back().bits;
        return {currentBits, 1.0, 0, 0, Algorithm::EMA, "Insufficient history", false};
    }
    
    std::vector<uint64_t> blockTimes = extractBlockTimes();
    std::vector<uint32_t> blockBits = extractBlockBits();
    
    uint32_t oldBits = blockHistory.back().bits;
    uint32_t newBits = Difficulty::retargetEMA(blockTimes, blockBits, 0.1);
    newBits = applySafetyLimits(oldBits, newBits);
    
    double difficultyChange = calculateDifficulty(newBits) / calculateDifficulty(oldBits);
    
    // Calculate representative timespans for display
    size_t windowStart = blockHistory.size() - Difficulty::RETARGET_WINDOW;
    uint64_t actualTimespan = blockHistory.back().timestamp - blockHistory[windowStart].timestamp;
    uint64_t expectedTimespan = Difficulty::TARGET_BLOCK_TIME * (Difficulty::RETARGET_WINDOW - 1);
    
    return {newBits, difficultyChange, expectedTimespan, actualTimespan, Algorithm::EMA, "Exponential moving average", true};
}

DifficultyRetargeting::RetargetResult DifficultyRetargeting::retargetAdaptive(uint32_t currentHeight) const {
    if (blockHistory.size() < Difficulty::RETARGET_WINDOW) {
        return retargetBasic(currentHeight);
    }
    
    // Analyze hashrate trend
    ChainAnalysis analysis = analyzeChain();
    
    // Choose algorithm based on chain stability
    if (analysis.isStable) {
        return retargetLinear(currentHeight);
    } else if (std::abs(analysis.hashrateChange - 1.0) > 0.5) {
        // High hashrate volatility - use EMA for smoother adjustment
        return retargetEMA(currentHeight);
    } else {
        return retargetBasic(currentHeight);
    }
}

bool DifficultyRetargeting::shouldRetarget(uint32_t height) const {
    return height % Difficulty::DIFFICULTY_ADJUSTMENT_INTERVAL == 0 && height > 0;
}

std::vector<uint64_t> DifficultyRetargeting::extractBlockTimes() const {
    std::vector<uint64_t> times;
    times.reserve(blockHistory.size());
    
    for (const auto& data : blockHistory) {
        times.push_back(data.timestamp);
    }
    
    return times;
}

std::vector<uint32_t> DifficultyRetargeting::extractBlockBits() const {
    std::vector<uint32_t> bits;
    bits.reserve(blockHistory.size());
    
    for (const auto& data : blockHistory) {
        bits.push_back(data.bits);
    }
    
    return bits;
}

double DifficultyRetargeting::calculateDifficulty(uint32_t bits) const {
    // Simple difficulty calculation: higher bits = lower difficulty
    // In real implementation, this would be max_target / current_target
    if (bits == 0) return 0.0;
    return static_cast<double>(Difficulty::MAX_BITS) / static_cast<double>(bits);
}

double DifficultyRetargeting::calculateWork(uint32_t bits) const {
    // Work is approximately 2^256 / target
    // Simplified calculation for demonstration
    return calculateDifficulty(bits) * 1000000.0; // Scale for readability
}

bool DifficultyRetargeting::isValidAdjustment(uint32_t oldBits, uint32_t newBits) const {
    if (newBits > Difficulty::MAX_BITS || newBits == 0) {
        return false;
    }
    
    double oldDiff = calculateDifficulty(oldBits);
    double newDiff = calculateDifficulty(newBits);
    double ratio = newDiff / oldDiff;
    
    return ratio >= Difficulty::MIN_ADJUSTMENT_FACTOR && ratio <= Difficulty::MAX_ADJUSTMENT_FACTOR;
}

uint32_t DifficultyRetargeting::applySafetyLimits(uint32_t oldBits, uint32_t newBits) const {
    if (newBits > Difficulty::MAX_BITS) {
        return Difficulty::MAX_BITS;
    }
    
    if (newBits == 0) {
        return 1;
    }
    
    // Ensure adjustment doesn't exceed maximum factors
    double oldDiff = calculateDifficulty(oldBits);
    double newDiff = calculateDifficulty(newBits);
    double ratio = newDiff / oldDiff;
    
    if (ratio > Difficulty::MAX_ADJUSTMENT_FACTOR) {
        // Limit to maximum increase
        double targetDiff = oldDiff * Difficulty::MAX_ADJUSTMENT_FACTOR;
        return static_cast<uint32_t>(Difficulty::MAX_BITS / targetDiff);
    } else if (ratio < Difficulty::MIN_ADJUSTMENT_FACTOR) {
        // Limit to maximum decrease
        double targetDiff = oldDiff * Difficulty::MIN_ADJUSTMENT_FACTOR;
        return static_cast<uint32_t>(Difficulty::MAX_BITS / targetDiff);
    }
    
    return newBits;
}

void DifficultyRetargeting::setAlgorithm(Algorithm algo) {
    currentAlgorithm = algo;
}

DifficultyRetargeting::Algorithm DifficultyRetargeting::getAlgorithm() const {
    return currentAlgorithm;
}

void DifficultyRetargeting::clearHistory() {
    blockHistory.clear();
}

size_t DifficultyRetargeting::getHistorySize() const {
    return blockHistory.size();
}

const std::vector<DifficultyRetargeting::RetargetData>& DifficultyRetargeting::getHistory() const {
    return blockHistory;
}

DifficultyRetargeting::ChainAnalysis DifficultyRetargeting::analyzeChain() const {
    ChainAnalysis analysis{};
    
    if (blockHistory.size() < MIN_BLOCKS_FOR_ANALYSIS) {
        analysis.avgBlockTime = Difficulty::TARGET_BLOCK_TIME;
        analysis.currentHashrate = 1.0;
        analysis.hashrateChange = 1.0;
        analysis.difficultyTrend = 1.0;
        analysis.blocksToRetarget = Difficulty::DIFFICULTY_ADJUSTMENT_INTERVAL;
        analysis.isStable = false;
        return analysis;
    }
    
    // Calculate average block time
    uint64_t totalTime = 0;
    for (size_t i = 1; i < blockHistory.size(); ++i) {
        totalTime += blockHistory[i].blockTime;
    }
    analysis.avgBlockTime = static_cast<double>(totalTime) / (blockHistory.size() - 1);
    
    // Estimate current hashrate (inverse of block time, scaled by difficulty)
    double currentDifficulty = blockHistory.back().difficulty;
    analysis.currentHashrate = currentDifficulty / analysis.avgBlockTime;
    
    // Calculate hashrate change over window
    if (blockHistory.size() >= Difficulty::RETARGET_WINDOW) {
        size_t midPoint = blockHistory.size() - Difficulty::RETARGET_WINDOW / 2;
        double oldHashrate = blockHistory[midPoint].difficulty / 
                           (blockHistory[midPoint].blockTime > 0 ? blockHistory[midPoint].blockTime : 1);
        analysis.hashrateChange = analysis.currentHashrate / oldHashrate;
    } else {
        analysis.hashrateChange = 1.0;
    }
    
    // Calculate difficulty trend
    if (blockHistory.size() >= 3) {
        double startDiff = blockHistory[blockHistory.size() - 3].difficulty;
        double endDiff = blockHistory.back().difficulty;
        analysis.difficultyTrend = endDiff / startDiff;
    } else {
        analysis.difficultyTrend = 1.0;
    }
    
    // Blocks until next retarget
    uint32_t lastHeight = blockHistory.back().height;
    analysis.blocksToRetarget = Difficulty::DIFFICULTY_ADJUSTMENT_INTERVAL - 
                               (lastHeight % Difficulty::DIFFICULTY_ADJUSTMENT_INTERVAL);
    
    // Determine stability
    double blockTimeVariance = std::abs(analysis.avgBlockTime - Difficulty::TARGET_BLOCK_TIME) / 
                              Difficulty::TARGET_BLOCK_TIME;
    analysis.isStable = blockTimeVariance < TARGET_ACCURACY_THRESHOLD && 
                       std::abs(analysis.hashrateChange - 1.0) < 0.2;
    
    return analysis;
}

DifficultyRetargeting::RetargetStats DifficultyRetargeting::getRetargetStats() const {
    RetargetStats stats{};
    
    if (blockHistory.size() < 2) {
        return stats;
    }
    
    uint32_t retargets = 0;
    uint32_t upward = 0;
    uint32_t downward = 0;
    double totalAdjustment = 0.0;
    double maxAdjustment = 0.0;
    uint64_t totalBlockTime = 0;
    
    for (size_t i = 1; i < blockHistory.size(); ++i) {
        totalBlockTime += blockHistory[i].blockTime;
        
        if (blockHistory[i].height % Difficulty::DIFFICULTY_ADJUSTMENT_INTERVAL == 0) {
            retargets++;
            
            double oldDiff = blockHistory[i-1].difficulty;
            double newDiff = blockHistory[i].difficulty;
            double ratio = newDiff / oldDiff;
            
            if (ratio > 1.0) {
                upward++;
            } else if (ratio < 1.0) {
                downward++;
            }
            
            double adjustment = std::abs(ratio - 1.0);
            totalAdjustment += adjustment;
            maxAdjustment = std::max(maxAdjustment, adjustment);
        }
    }
    
    stats.totalRetargets = retargets;
    stats.upwardAdjustments = upward;
    stats.downwardAdjustments = downward;
    stats.avgAdjustmentSize = retargets > 0 ? totalAdjustment / retargets : 0.0;
    stats.maxAdjustmentSize = maxAdjustment;
    stats.avgBlockTime = static_cast<double>(totalBlockTime) / (blockHistory.size() - 1);
    
    // Calculate target accuracy
    double targetDeviation = std::abs(stats.avgBlockTime - Difficulty::TARGET_BLOCK_TIME) / 
                            Difficulty::TARGET_BLOCK_TIME;
    stats.targetAccuracy = 1.0 - targetDeviation;
    
    return stats;
}

std::string DifficultyRetargeting::algorithmToString(Algorithm algo) const {
    switch (algo) {
        case Algorithm::BASIC: return "Basic";
        case Algorithm::LINEAR: return "Linear";
        case Algorithm::EMA: return "EMA";
        case Algorithm::ADAPTIVE: return "Adaptive";
        default: return "Unknown";
    }
}

void DifficultyRetargeting::printRetargetResult(const RetargetResult& result) const {
    std::cout << "\n=== Difficulty Retarget Result ===" << std::endl;
    std::cout << "Algorithm: " << algorithmToString(result.algorithmUsed) << std::endl;
    std::cout << "Was Adjusted: " << (result.wasAdjusted ? "Yes" : "No") << std::endl;
    std::cout << "New Bits: 0x" << std::hex << result.newBits << std::dec << std::endl;
    std::cout << "Difficulty Change: " << std::fixed << std::setprecision(4) 
              << result.difficultyChange << "x" << std::endl;
    std::cout << "Expected Timespan: " << result.expectedTimespan << "s" << std::endl;
    std::cout << "Actual Timespan: " << result.actualTimespan << "s" << std::endl;
    std::cout << "Reason: " << result.reason << std::endl;
}

void DifficultyRetargeting::printHistory() const {
    std::cout << "\n=== Difficulty History ===" << std::endl;
    std::cout << "Height | Timestamp | Bits | Block Time | Difficulty" << std::endl;
    std::cout << "-------|-----------|------|------------|----------" << std::endl;
    
    for (const auto& data : blockHistory) {
        std::cout << std::setw(6) << data.height << " | "
                  << std::setw(9) << data.timestamp << " | "
                  << "0x" << std::hex << std::setw(6) << data.bits << std::dec << " | "
                  << std::setw(10) << data.blockTime << " | "
                  << std::fixed << std::setprecision(2) << data.difficulty << std::endl;
    }
}

} // namespace pragma
