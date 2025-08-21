#pragma once

#include <string>
#include <cstdint>
#include <vector>

namespace pragma {

/**
 * Difficulty and target management for proof-of-work
 */
class Difficulty {
public:
    // Convert compact bits representation to target hex string
    static std::string bitsToTarget(uint32_t bits);
    
    // Convert target hex string to compact bits representation
    static uint32_t targetToBits(const std::string& target);
    
    // Check if a hash meets the difficulty target
    static bool meetsTarget(const std::string& hash, uint32_t bits);
    static bool meetsTarget(const std::string& hash, const std::string& target);
    
    // Calculate work for a given target (for chain selection)
    static std::string calculateWork(uint32_t bits);
    
    // Difficulty adjustment (Bitcoin-style)
    static uint32_t adjustDifficulty(uint32_t currentBits, 
                                   uint64_t actualTimespan, 
                                   uint64_t targetTimespan);
    
    // Advanced difficulty retargeting with block history
    static uint32_t calculateNextWorkRequired(const std::vector<uint64_t>& blockTimes,
                                            const std::vector<uint32_t>& blockBits,
                                            uint32_t currentHeight);
    
    // Retargeting algorithms
    static uint32_t retargetBasic(uint32_t currentBits, 
                                uint64_t actualTimespan, 
                                uint64_t targetTimespan);
    
    static uint32_t retargetLinear(const std::vector<uint64_t>& blockTimes,
                                 uint32_t currentBits);
    
    static uint32_t retargetEMA(const std::vector<uint64_t>& blockTimes,
                              const std::vector<uint32_t>& blockBits,
                              double alpha = 0.1);
    
    // Helper functions for target comparison
    static bool isValidTarget(const std::string& target);
    static int compareTargets(const std::string& hash, const std::string& target);
    
    // Timespan validation and clamping
    static uint64_t clampTimespan(uint64_t timespan, uint64_t targetTimespan);
    static double calculateHashrateChange(const std::vector<uint64_t>& blockTimes);
    
    // Constants
    static const uint32_t MAX_BITS = 0x207fffff;  // Maximum difficulty (minimum work)
    static const uint64_t TARGET_BLOCK_TIME = 30; // 30 seconds per block
    static const uint32_t DIFFICULTY_ADJUSTMENT_INTERVAL = 10; // Adjust every 10 blocks
    static const uint32_t RETARGET_WINDOW = 10; // Number of blocks to consider for retargeting
    static const double MAX_ADJUSTMENT_FACTOR; // Maximum difficulty change per adjustment
    static const double MIN_ADJUSTMENT_FACTOR; // Minimum difficulty change per adjustment
    
private:
    // Helper for big integer operations on hex strings
    static std::string hexAdd(const std::string& a, const std::string& b);
    static std::string hexMultiply(const std::string& hex, uint64_t multiplier);
    static std::string hexDivide(const std::string& hex, uint64_t divisor);
    static bool hexLessThan(const std::string& a, const std::string& b);
};

} // namespace pragma
