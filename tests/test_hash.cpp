#include <gtest/gtest.h>
#include "primitives/hash.h"

using namespace pragma;

class HashTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(HashTest, SHA256BasicTest) {
    std::string input = "hello world";
    std::string expected = "b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9";
    
    std::string result = Hash::sha256(input);
    EXPECT_EQ(result, expected);
}

TEST_F(HashTest, DoubleSHA256Test) {
    std::string input = "hello world";
    
    // First SHA256
    std::string first_hash = Hash::sha256(input);
    // Second SHA256 of the first hash bytes
    std::vector<uint8_t> first_bytes = Hash::fromHex(first_hash);
    std::string expected = Hash::sha256(first_bytes);
    
    std::string result = Hash::dbl_sha256(input);
    EXPECT_EQ(result, expected);
}

TEST_F(HashTest, HexConversionTest) {
    std::vector<uint8_t> bytes = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
    std::string expected = "0123456789abcdef";
    
    std::string hex = Hash::toHex(bytes);
    EXPECT_EQ(hex, expected);
    
    std::vector<uint8_t> converted_back = Hash::fromHex(hex);
    EXPECT_EQ(bytes, converted_back);
}

TEST_F(HashTest, ReverseHexTest) {
    std::string input = "0123456789abcdef";
    std::string expected = "efcdab8967452301";
    
    std::string result = Hash::reverseHex(input);
    EXPECT_EQ(result, expected);
}

TEST_F(HashTest, EmptyInputTest) {
    std::string empty = "";
    std::string result = Hash::sha256(empty);
    
    // SHA256 of empty string
    std::string expected = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
    EXPECT_EQ(result, expected);
}

TEST_F(HashTest, InvalidHexTest) {
    EXPECT_THROW(Hash::fromHex("invalid_hex"), std::invalid_argument);
    EXPECT_THROW(Hash::fromHex("12g"), std::invalid_argument);
}

TEST_F(HashTest, OddLengthHexTest) {
    EXPECT_THROW(Hash::fromHex("123"), std::invalid_argument);
}
