#include <gtest/gtest.h>
#include "primitives/serialize.h"

using namespace pragma;

class SerializeTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(SerializeTest, VarIntSmallTest) {
    uint64_t value = 100;
    auto encoded = Serialize::encodeVarInt(value);
    
    EXPECT_EQ(encoded.size(), 1);
    EXPECT_EQ(encoded[0], 100);
    
    auto [decoded, size] = Serialize::decodeVarInt(encoded);
    EXPECT_EQ(decoded, value);
    EXPECT_EQ(size, 1);
}

TEST_F(SerializeTest, VarIntMediumTest) {
    uint64_t value = 1000;
    auto encoded = Serialize::encodeVarInt(value);
    
    EXPECT_EQ(encoded.size(), 3);
    EXPECT_EQ(encoded[0], 0xFD);
    
    auto [decoded, size] = Serialize::decodeVarInt(encoded);
    EXPECT_EQ(decoded, value);
    EXPECT_EQ(size, 3);
}

TEST_F(SerializeTest, VarIntLargeTest) {
    uint64_t value = 100000;
    auto encoded = Serialize::encodeVarInt(value);
    
    EXPECT_EQ(encoded.size(), 5);
    EXPECT_EQ(encoded[0], 0xFE);
    
    auto [decoded, size] = Serialize::decodeVarInt(encoded);
    EXPECT_EQ(decoded, value);
    EXPECT_EQ(size, 5);
}

TEST_F(SerializeTest, VarIntVeryLargeTest) {
    uint64_t value = 0x100000000ULL;
    auto encoded = Serialize::encodeVarInt(value);
    
    EXPECT_EQ(encoded.size(), 9);
    EXPECT_EQ(encoded[0], 0xFF);
    
    auto [decoded, size] = Serialize::decodeVarInt(encoded);
    EXPECT_EQ(decoded, value);
    EXPECT_EQ(size, 9);
}

TEST_F(SerializeTest, Uint32LETest) {
    uint32_t value = 0x12345678;
    auto encoded = Serialize::encodeUint32LE(value);
    
    EXPECT_EQ(encoded.size(), 4);
    // Little endian: least significant byte first
    EXPECT_EQ(encoded[0], 0x78);
    EXPECT_EQ(encoded[1], 0x56);
    EXPECT_EQ(encoded[2], 0x34);
    EXPECT_EQ(encoded[3], 0x12);
    
    uint32_t decoded = Serialize::decodeUint32LE(encoded);
    EXPECT_EQ(decoded, value);
}

TEST_F(SerializeTest, Uint64LETest) {
    uint64_t value = 0x123456789ABCDEF0ULL;
    auto encoded = Serialize::encodeUint64LE(value);
    
    EXPECT_EQ(encoded.size(), 8);
    // Little endian: least significant byte first
    EXPECT_EQ(encoded[0], 0xF0);
    EXPECT_EQ(encoded[7], 0x12);
    
    uint64_t decoded = Serialize::decodeUint64LE(encoded);
    EXPECT_EQ(decoded, value);
}

TEST_F(SerializeTest, StringEncodeDecodeTest) {
    std::string original = "Hello, Blockchain!";
    auto encoded = Serialize::encodeString(original);
    
    auto [decoded, size] = Serialize::decodeString(encoded);
    EXPECT_EQ(decoded, original);
    EXPECT_EQ(size, encoded.size());
}

TEST_F(SerializeTest, EmptyStringTest) {
    std::string original = "";
    auto encoded = Serialize::encodeString(original);
    
    auto [decoded, size] = Serialize::decodeString(encoded);
    EXPECT_EQ(decoded, original);
    EXPECT_EQ(size, encoded.size());
}

TEST_F(SerializeTest, CombineTest) {
    std::vector<uint8_t> part1 = {0x01, 0x02};
    std::vector<uint8_t> part2 = {0x03, 0x04};
    std::vector<uint8_t> part3 = {0x05, 0x06};
    
    auto combined = Serialize::combine({part1, part2, part3});
    
    std::vector<uint8_t> expected = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    EXPECT_EQ(combined, expected);
}
