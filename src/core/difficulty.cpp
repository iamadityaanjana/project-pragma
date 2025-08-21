#include "difficulty.h"
#include "../primitives/utils.h"
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace pragma {

// Define the adjustment factor constants
const double Difficulty::MAX_ADJUSTMENT_FACTOR = 4.0; // 4x max increase
const double Difficulty::MIN_ADJUSTMENT_FACTOR = 0.25; // 4x max decrease

std::string Difficulty::bitsToTarget(uint32_t bits) {
    // Extract exponent and coefficient from compact representation
    uint32_t exponent = bits >> 24;
    uint32_t coefficient = bits & 0x00ffffff;
    
    // Target = coefficient * 2^(8 * (exponent - 3))
    if (exponent <= 3) {
        coefficient >>= (8 * (3 - exponent));
        exponent = 3;
    }
    
    // Convert to hex string (64 characters for 32 bytes)
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    
    // Calculate the position where coefficient should be placed
    int pos = (exponent - 3) * 2; // 2 hex chars per byte
    
    // Create target string
    std::string target(64, '0');
    
    if (pos < 64) {
        // Convert coefficient to hex and place it at correct position
        std::stringstream coeffSs;
        coeffSs << std::hex << coefficient;
        std::string coeffHex = coeffSs.str();
        
        // Pad coefficient to 6 characters (3 bytes)
        while (coeffHex.length() < 6) {
            coeffHex = "0" + coeffHex;
        }
        
        // Place coefficient in target string
        if (pos + coeffHex.length() <= 64) {
            target.replace(pos, coeffHex.length(), coeffHex);
        }
    }
    
    return target;
}

uint32_t Difficulty::targetToBits(const std::string& target) {
    // Find first non-zero byte
    size_t firstNonZero = 0;
    while (firstNonZero < target.length() && target[firstNonZero] == '0') {
        firstNonZero++;
    }
    
    if (firstNonZero >= target.length()) {
        return MAX_BITS; // All zeros = maximum difficulty
    }
    
    // Calculate exponent (bytes from start)
    uint32_t exponent = 3 + (firstNonZero / 2);
    
    // Extract coefficient (first 3 bytes of significant data)
    std::string coeffStr = target.substr(firstNonZero, 6);
    if (coeffStr.length() < 6) {
        coeffStr += std::string(6 - coeffStr.length(), '0');
    }
    
    uint32_t coefficient = std::stoul(coeffStr, nullptr, 16);
    
    // Combine exponent and coefficient
    return (exponent << 24) | (coefficient & 0x00ffffff);
}

bool Difficulty::meetsTarget(const std::string& hash, uint32_t bits) {
    std::string target = bitsToTarget(bits);
    return meetsTarget(hash, target);
}

bool Difficulty::meetsTarget(const std::string& hash, const std::string& target) {
    // Compare hash with target (hash must be <= target)
    return !hexLessThan(target, hash);
}

std::string Difficulty::calculateWork(uint32_t bits) {
    // Work = 2^256 / (target + 1)
    // For simplicity, we'll return the target as inverse work measure
    std::string target = bitsToTarget(bits);
    
    // The actual work calculation would require big integer arithmetic
    // For now, return a simplified representation
    return target;
}

uint32_t Difficulty::adjustDifficulty(uint32_t currentBits, 
                                     uint64_t actualTimespan, 
                                     uint64_t targetTimespan) {
    return retargetBasic(currentBits, actualTimespan, targetTimespan);
}

uint32_t Difficulty::calculateNextWorkRequired(const std::vector<uint64_t>& blockTimes,
                                            const std::vector<uint32_t>& blockBits,
                                            uint32_t currentHeight) {
    if (blockTimes.empty() || blockBits.empty()) {
        return MAX_BITS; // Default to easiest difficulty
    }
    
    // Check if we're at a retargeting interval
    if (currentHeight % DIFFICULTY_ADJUSTMENT_INTERVAL != 0) {
        return blockBits.back(); // No adjustment needed
    }
    
    if (blockTimes.size() < RETARGET_WINDOW) {
        return blockBits.back(); // Not enough history
    }
    
    // Use the most recent block data for retargeting
    uint32_t currentBits = blockBits.back();
    
    // Calculate actual timespan for the retarget window
    size_t startIdx = blockTimes.size() >= RETARGET_WINDOW ? 
                     blockTimes.size() - RETARGET_WINDOW : 0;
    
    uint64_t actualTimespan = 0;
    for (size_t i = startIdx + 1; i < blockTimes.size(); ++i) {
        if (blockTimes[i] > blockTimes[i-1]) {
            actualTimespan += (blockTimes[i] - blockTimes[i-1]);
        }
    }
    
    uint64_t targetTimespan = TARGET_BLOCK_TIME * (blockTimes.size() - startIdx - 1);
    
    return retargetBasic(currentBits, actualTimespan, targetTimespan);
}

uint32_t Difficulty::retargetBasic(uint32_t currentBits, 
                                 uint64_t actualTimespan, 
                                 uint64_t targetTimespan) {
    if (targetTimespan == 0) {
        return currentBits;
    }
    
    // Clamp timespan to prevent extreme adjustments
    actualTimespan = clampTimespan(actualTimespan, targetTimespan);
    
    // Get current target
    std::string currentTarget = bitsToTarget(currentBits);
    
    // Calculate adjustment ratio
    double ratio = static_cast<double>(actualTimespan) / static_cast<double>(targetTimespan);
    
    // Apply adjustment limits
    if (ratio > MAX_ADJUSTMENT_FACTOR) {
        ratio = MAX_ADJUSTMENT_FACTOR;
    } else if (ratio < MIN_ADJUSTMENT_FACTOR) {
        ratio = MIN_ADJUSTMENT_FACTOR;
    }
    
    // For simplification, we'll adjust the bits value directly
    // In a real implementation, this would require proper big integer arithmetic
    if (ratio > 1.0) {
        // Make mining easier (increase target, increase bits)
        uint32_t adjustment = static_cast<uint32_t>((ratio - 1.0) * 0x1000);
        uint32_t newBits = std::min(currentBits + adjustment, MAX_BITS);
        return newBits;
    } else {
        // Make mining harder (decrease target, decrease bits)
        uint32_t adjustment = static_cast<uint32_t>((1.0 - ratio) * 0x1000);
        uint32_t newBits = currentBits > adjustment ? currentBits - adjustment : 1;
        return newBits;
    }
}

uint32_t Difficulty::retargetLinear(const std::vector<uint64_t>& blockTimes,
                                  uint32_t currentBits) {
    if (blockTimes.size() < 2) {
        return currentBits;
    }
    
    // Calculate average block time over the window
    size_t windowSize = std::min(static_cast<size_t>(RETARGET_WINDOW), blockTimes.size());
    size_t startIdx = blockTimes.size() - windowSize;
    
    uint64_t totalTime = 0;
    uint64_t intervals = 0;
    
    for (size_t i = startIdx + 1; i < blockTimes.size(); ++i) {
        if (blockTimes[i] > blockTimes[i-1]) {
            totalTime += (blockTimes[i] - blockTimes[i-1]);
            intervals++;
        }
    }
    
    if (intervals == 0) {
        return currentBits;
    }
    
    uint64_t avgBlockTime = totalTime / intervals;
    
    return retargetBasic(currentBits, avgBlockTime * intervals, TARGET_BLOCK_TIME * intervals);
}

uint32_t Difficulty::retargetEMA(const std::vector<uint64_t>& blockTimes,
                               const std::vector<uint32_t>& blockBits,
                               double alpha) {
    if (blockTimes.size() < 2 || blockBits.empty()) {
        return blockBits.empty() ? MAX_BITS : blockBits.back();
    }
    
    // Calculate exponential moving average of block times
    double ema = TARGET_BLOCK_TIME; // Start with target
    
    size_t windowSize = std::min(static_cast<size_t>(RETARGET_WINDOW), blockTimes.size());
    size_t startIdx = blockTimes.size() - windowSize;
    
    for (size_t i = startIdx + 1; i < blockTimes.size(); ++i) {
        if (blockTimes[i] > blockTimes[i-1]) {
            double blockTime = static_cast<double>(blockTimes[i] - blockTimes[i-1]);
            ema = alpha * blockTime + (1.0 - alpha) * ema;
        }
    }
    
    // Calculate adjustment based on EMA vs target
    uint64_t targetTimespan = TARGET_BLOCK_TIME * (windowSize - 1);
    uint64_t actualTimespan = static_cast<uint64_t>(ema * (windowSize - 1));
    
    return retargetBasic(blockBits.back(), actualTimespan, targetTimespan);
}

uint64_t Difficulty::clampTimespan(uint64_t timespan, uint64_t targetTimespan) {
    uint64_t minTimespan = targetTimespan / 4;
    uint64_t maxTimespan = targetTimespan * 4;
    
    if (timespan < minTimespan) {
        return minTimespan;
    }
    if (timespan > maxTimespan) {
        return maxTimespan;
    }
    
    return timespan;
}

double Difficulty::calculateHashrateChange(const std::vector<uint64_t>& blockTimes) {
    if (blockTimes.size() < RETARGET_WINDOW) {
        return 1.0; // No change
    }
    
    // Calculate average block time for first and second half of window
    size_t windowSize = std::min(static_cast<size_t>(RETARGET_WINDOW), blockTimes.size());
    size_t halfWindow = windowSize / 2;
    size_t startIdx = blockTimes.size() - windowSize;
    
    // First half average
    uint64_t firstHalfTime = 0;
    uint64_t firstHalfIntervals = 0;
    for (size_t i = startIdx + 1; i < startIdx + halfWindow; ++i) {
        if (blockTimes[i] > blockTimes[i-1]) {
            firstHalfTime += (blockTimes[i] - blockTimes[i-1]);
            firstHalfIntervals++;
        }
    }
    
    // Second half average
    uint64_t secondHalfTime = 0;
    uint64_t secondHalfIntervals = 0;
    for (size_t i = startIdx + halfWindow + 1; i < blockTimes.size(); ++i) {
        if (blockTimes[i] > blockTimes[i-1]) {
            secondHalfTime += (blockTimes[i] - blockTimes[i-1]);
            secondHalfIntervals++;
        }
    }
    
    if (firstHalfIntervals == 0 || secondHalfIntervals == 0) {
        return 1.0;
    }
    
    double firstAvg = static_cast<double>(firstHalfTime) / firstHalfIntervals;
    double secondAvg = static_cast<double>(secondHalfTime) / secondHalfIntervals;
    
    // Hashrate change is inverse of block time change
    return firstAvg / secondAvg;
}

bool Difficulty::isValidTarget(const std::string& target) {
    if (target.length() != 64) {
        return false;
    }
    
    return Utils::isValidHex(target);
}

int Difficulty::compareTargets(const std::string& hash, const std::string& target) {
    if (hash.length() != target.length()) {
        return hash.length() < target.length() ? -1 : 1;
    }
    
    return hash.compare(target);
}

bool Difficulty::hexLessThan(const std::string& a, const std::string& b) {
    if (a.length() != b.length()) {
        return a.length() < b.length();
    }
    
    return a < b;
}

// Simplified hex arithmetic functions for basic operations
std::string Difficulty::hexAdd(const std::string& a, const std::string& b) {
    // Simplified implementation - would need proper big integer arithmetic
    return a; // Placeholder
}

std::string Difficulty::hexMultiply(const std::string& hex, uint64_t multiplier) {
    // Simplified implementation - would need proper big integer arithmetic
    return hex; // Placeholder
}

std::string Difficulty::hexDivide(const std::string& hex, uint64_t divisor) {
    // Simplified implementation - would need proper big integer arithmetic
    return hex; // Placeholder
}

} // namespace pragma
