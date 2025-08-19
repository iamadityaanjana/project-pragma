#include "difficulty.h"
#include "../primitives/utils.h"
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace pragma {

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
    // Clamp timespan to prevent extreme adjustments
    uint64_t minTimespan = targetTimespan / 4;
    uint64_t maxTimespan = targetTimespan * 4;
    
    if (actualTimespan < minTimespan) {
        actualTimespan = minTimespan;
    }
    if (actualTimespan > maxTimespan) {
        actualTimespan = maxTimespan;
    }
    
    // Get current target
    std::string currentTarget = bitsToTarget(currentBits);
    
    // Adjust target: newTarget = currentTarget * actualTimespan / targetTimespan
    // For simplicity, we'll do basic multiplication/division
    
    if (actualTimespan > targetTimespan) {
        // Make mining easier (larger target)
        // This is a simplified adjustment - real implementation needs big integer math
        if (currentBits < MAX_BITS) {
            return std::min(currentBits + 1, MAX_BITS);
        }
    } else {
        // Make mining harder (smaller target)
        if (currentBits > 1) {
            return currentBits - 1;
        }
    }
    
    return currentBits;
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
