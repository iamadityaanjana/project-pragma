#include <gtest/gtest.h>
#include "primitives/utils.h"

using namespace pragma;

class UtilsTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(UtilsTest, TimestampTest) {
    uint64_t timestamp = Utils::getCurrentTimestamp();
    EXPECT_GT(timestamp, 0);
    
    // Test that it's roughly current (within last year)
    uint64_t year_ago = timestamp - (365 * 24 * 60 * 60);
    EXPECT_GT(timestamp, year_ago);
}

TEST_F(UtilsTest, TrimTest) {
    EXPECT_EQ(Utils::trim("  hello  "), "hello");
    EXPECT_EQ(Utils::trim("hello"), "hello");
    EXPECT_EQ(Utils::trim("  "), "");
    EXPECT_EQ(Utils::trim(""), "");
}

TEST_F(UtilsTest, SplitTest) {
    auto result = Utils::split("a,b,c", ',');
    std::vector<std::string> expected = {"a", "b", "c"};
    EXPECT_EQ(result, expected);
    
    result = Utils::split("single", ',');
    expected = {"single"};
    EXPECT_EQ(result, expected);
    
    result = Utils::split("", ',');
    expected = {""};
    EXPECT_EQ(result, expected);
}

TEST_F(UtilsTest, StartsWithTest) {
    EXPECT_TRUE(Utils::startsWith("hello world", "hello"));
    EXPECT_TRUE(Utils::startsWith("hello", "hello"));
    EXPECT_FALSE(Utils::startsWith("hello", "world"));
    EXPECT_FALSE(Utils::startsWith("hi", "hello"));
}

TEST_F(UtilsTest, EndsWithTest) {
    EXPECT_TRUE(Utils::endsWith("hello world", "world"));
    EXPECT_TRUE(Utils::endsWith("world", "world"));
    EXPECT_FALSE(Utils::endsWith("hello world", "hello"));
    EXPECT_FALSE(Utils::endsWith("hi", "world"));
}

TEST_F(UtilsTest, ValidHexTest) {
    EXPECT_TRUE(Utils::isValidHex("0123456789abcdef"));
    EXPECT_TRUE(Utils::isValidHex("0123456789ABCDEF"));
    EXPECT_TRUE(Utils::isValidHex("a1b2c3"));
    EXPECT_FALSE(Utils::isValidHex(""));
    EXPECT_FALSE(Utils::isValidHex("xyz"));
    EXPECT_FALSE(Utils::isValidHex("12g4"));
}

TEST_F(UtilsTest, ValidHashTest) {
    // 64-character hex string (32 bytes)
    std::string validHash = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
    EXPECT_TRUE(Utils::isValidHash(validHash));
    
    EXPECT_FALSE(Utils::isValidHash("short"));
    EXPECT_FALSE(Utils::isValidHash("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b85g"));
}

TEST_F(UtilsTest, RandomBytesTest) {
    auto bytes1 = Utils::randomBytes(16);
    auto bytes2 = Utils::randomBytes(16);
    
    EXPECT_EQ(bytes1.size(), 16);
    EXPECT_EQ(bytes2.size(), 16);
    EXPECT_NE(bytes1, bytes2); // Very unlikely to be equal
}

TEST_F(UtilsTest, RandomNumbersTest) {
    uint32_t num1 = Utils::randomUint32();
    uint32_t num2 = Utils::randomUint32();
    EXPECT_NE(num1, num2); // Very unlikely to be equal
    
    uint64_t num3 = Utils::randomUint64();
    uint64_t num4 = Utils::randomUint64();
    EXPECT_NE(num3, num4); // Very unlikely to be equal
}
