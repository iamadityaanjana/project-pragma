#include <gtest/gtest.h>
#include "core/difficulty.h"
#include <vector>

using namespace pragma;

class DifficultyTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(DifficultyTest, BitsToTargetConversionTest) {
    // Test standard Bitcoin difficulty bits
    uint32_t bits = 0x1d00ffff;
    std::string target = Difficulty::bitsToTarget(bits);
    
    EXPECT_FALSE(target.empty());
    EXPECT_EQ(target.length(), 64); // 32 bytes as hex string
    
    // Target should start with zeros for high difficulty
    EXPECT_EQ(target.substr(0, 6), "000000");
}

TEST_F(DifficultyTest, TargetToBitsConversionTest) {
    // Test round-trip conversion
    uint32_t originalBits = 0x1d00ffff;
    std::string target = Difficulty::bitsToTarget(originalBits);
    uint32_t convertedBits = Difficulty::targetToBits(target);
    
    EXPECT_EQ(originalBits, convertedBits);
}

TEST_F(DifficultyTest, MaxTargetTest) {
    uint32_t maxBits = 0x1d00ffff;
    std::string maxTarget = Difficulty::bitsToTarget(maxBits);
    
    // Max target should be achievable
    std::string hash = "00000ffff0000000000000000000000000000000000000000000000000000000";
    EXPECT_TRUE(Difficulty::meetsTarget(hash, maxTarget));
    
    // Hash with more leading zeros should also meet target
    hash = "0000000000000000000000000000000000000000000000000000000000000001";
    EXPECT_TRUE(Difficulty::meetsTarget(hash, maxTarget));
}

TEST_F(DifficultyTest, MeetsTargetTest) {
    std::string target = "0000ffff00000000000000000000000000000000000000000000000000000000";
    
    // Hash that meets target (smaller value)
    std::string validHash = "0000aaaa00000000000000000000000000000000000000000000000000000000";
    EXPECT_TRUE(Difficulty::meetsTarget(validHash, target));
    
    // Hash that doesn't meet target (larger value)
    std::string invalidHash = "0001000000000000000000000000000000000000000000000000000000000000";
    EXPECT_FALSE(Difficulty::meetsTarget(invalidHash, target));
    
    // Edge case: exactly equal
    EXPECT_TRUE(Difficulty::meetsTarget(target, target));
}

TEST_F(DifficultyTest, DifficultyAdjustmentTest) {
    uint32_t currentBits = 0x1d00ffff;
    uint32_t targetTimespan = 1209600; // 2 weeks in seconds
    
    // Test case: blocks took half the expected time (increase difficulty)
    uint32_t actualTimespan = targetTimespan / 2;
    uint32_t newBits = Difficulty::adjustDifficulty(currentBits, actualTimespan, targetTimespan);
    
    // New bits should represent higher difficulty (larger exponent or smaller mantissa)
    EXPECT_NE(newBits, currentBits);
    
    // Test case: blocks took double the expected time (decrease difficulty)
    actualTimespan = targetTimespan * 2;
    newBits = Difficulty::adjustDifficulty(currentBits, actualTimespan, targetTimespan);
    EXPECT_NE(newBits, currentBits);
    
    // Test case: blocks took exactly the expected time (no change)
    actualTimespan = targetTimespan;
    newBits = Difficulty::adjustDifficulty(currentBits, actualTimespan, targetTimespan);
    EXPECT_EQ(newBits, currentBits);
}

TEST_F(DifficultyTest, DifficultyClampingTest) {
    uint32_t currentBits = 0x1d00ffff;
    uint32_t targetTimespan = 1209600; // 2 weeks
    
    // Test extreme case: very fast blocks (should clamp to 4x increase max)
    uint32_t veryFastTimespan = targetTimespan / 10;
    uint32_t newBits = Difficulty::adjustDifficulty(currentBits, veryFastTimespan, targetTimespan);
    
    // Should be clamped to maximum 4x difficulty increase
    std::string oldTarget = Difficulty::bitsToTarget(currentBits);
    std::string newTarget = Difficulty::bitsToTarget(newBits);
    
    // New target should be smaller (higher difficulty) but not too extreme
    EXPECT_TRUE(newTarget < oldTarget);
    
    // Test extreme case: very slow blocks (should clamp to 4x decrease max)
    uint32_t verySlowTimespan = targetTimespan * 10;
    newBits = Difficulty::adjustDifficulty(currentBits, verySlowTimespan, targetTimespan);
    
    newTarget = Difficulty::bitsToTarget(newBits);
    
    // New target should be larger (lower difficulty) but not too extreme
    EXPECT_TRUE(newTarget > oldTarget);
}

TEST_F(DifficultyTest, MinimumDifficultyTest) {
    // Test that we never go below minimum difficulty
    uint32_t minBits = 0x1d00ffff; // Maximum target (minimum difficulty)
    uint32_t targetTimespan = 1209600;
    
    // Try to decrease difficulty from minimum
    uint32_t verySlowTimespan = targetTimespan * 10;
    uint32_t newBits = Difficulty::adjustDifficulty(minBits, verySlowTimespan, targetTimespan);
    
    // Should not go below minimum difficulty
    EXPECT_LE(newBits, minBits);
}

TEST_F(DifficultyTest, EdgeCaseBitsValuesTest) {
    // Test various edge case bits values
    std::vector<uint32_t> testBits = {
        0x1d00ffff, // Standard max target
        0x1c00ffff, // Slightly higher difficulty
        0x1b00ffff, // Even higher difficulty
        0x207fffff, // Very easy difficulty for testing
        0x1e00ffff  // Lower difficulty than max
    };
    
    for (uint32_t bits : testBits) {
        std::string target = Difficulty::bitsToTarget(bits);
        EXPECT_FALSE(target.empty());
        EXPECT_EQ(target.length(), 64);
        
        // Round-trip test
        uint32_t convertedBits = Difficulty::targetToBits(target);
        EXPECT_EQ(bits, convertedBits);
    }
}

TEST_F(DifficultyTest, CompareTargetsTest) {
    std::string target1 = "0000ffff00000000000000000000000000000000000000000000000000000000";
    std::string target2 = "0001000000000000000000000000000000000000000000000000000000000000";
    std::string target3 = "0000ffff00000000000000000000000000000000000000000000000000000000";
    
    // target1 < target2
    EXPECT_TRUE(target1 < target2);
    EXPECT_FALSE(target2 < target1);
    
    // target1 == target3
    EXPECT_FALSE(target1 < target3);
    EXPECT_FALSE(target3 < target1);
    EXPECT_TRUE(target1 == target3);
}

TEST_F(DifficultyTest, HashTargetComparisonsTest) {
    std::string target = "0000ffff00000000000000000000000000000000000000000000000000000000";
    
    // Test various hash values
    std::vector<std::pair<std::string, bool>> testCases = {
        {"0000000100000000000000000000000000000000000000000000000000000000", true},  // Much smaller
        {"0000fffe00000000000000000000000000000000000000000000000000000000", true},  // Just smaller
        {"0000ffff00000000000000000000000000000000000000000000000000000000", true},  // Equal
        {"0001000000000000000000000000000000000000000000000000000000000000", false}, // Just larger
        {"1000000000000000000000000000000000000000000000000000000000000000", false}  // Much larger
    };
    
    for (const auto& testCase : testCases) {
        EXPECT_EQ(Difficulty::meetsTarget(testCase.first, target), testCase.second)
            << "Hash: " << testCase.first << " Target: " << target;
    }
}
