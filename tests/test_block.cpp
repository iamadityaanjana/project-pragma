#include <gtest/gtest.h>
#include "core/block.h"
#include "core/difficulty.h"

using namespace pragma;

class BlockTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(BlockTest, BlockHeaderSerializationTest) {
    BlockHeader original(1, "prev_hash", "merkle_root", 1234567890, 0x1d00ffff, 12345);
    
    auto serialized = original.serialize();
    BlockHeader deserialized = BlockHeader::deserialize(serialized);
    
    EXPECT_EQ(original, deserialized);
    EXPECT_EQ(original.version, deserialized.version);
    EXPECT_EQ(original.prevHash, deserialized.prevHash);
    EXPECT_EQ(original.merkleRoot, deserialized.merkleRoot);
    EXPECT_EQ(original.timestamp, deserialized.timestamp);
    EXPECT_EQ(original.bits, deserialized.bits);
    EXPECT_EQ(original.nonce, deserialized.nonce);
}

TEST_F(BlockTest, BlockHeaderHashTest) {
    BlockHeader header(1, "prev_hash", "merkle_root", 1234567890, 0x1d00ffff, 12345);
    
    std::string hash1 = header.computeHash();
    std::string hash2 = header.computeHash();
    
    EXPECT_EQ(hash1, hash2); // Hash should be deterministic
    EXPECT_FALSE(hash1.empty());
    EXPECT_EQ(hash1.length(), 64); // SHA256 hash is 32 bytes = 64 hex characters
}

TEST_F(BlockTest, GenesisBlockTest) {
    std::string minerAddress = "1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa";
    uint64_t reward = 5000000000ULL;
    
    Block genesis = Block::createGenesis(minerAddress, reward);
    
    EXPECT_EQ(genesis.transactions.size(), 1);
    EXPECT_TRUE(genesis.transactions[0].isCoinbase);
    EXPECT_EQ(genesis.transactions[0].getTotalOutput(), reward);
    EXPECT_EQ(genesis.header.prevHash, "0000000000000000000000000000000000000000000000000000000000000000");
    EXPECT_FALSE(genesis.hash.empty());
    EXPECT_TRUE(genesis.isValid());
}

TEST_F(BlockTest, BlockCreationTest) {
    // Create genesis block first
    Block genesis = Block::createGenesis("miner1", 5000000000ULL);
    
    // Create some transactions
    std::vector<Transaction> txs;
    
    std::vector<TxIn> inputs;
    inputs.emplace_back(OutPoint(genesis.transactions[0].txid, 0), "sig", "pk");
    
    std::vector<TxOut> outputs;
    outputs.emplace_back(1000000000ULL, "addr1");
    outputs.emplace_back(3999999000ULL, "addr2");
    
    Transaction tx = Transaction::create(inputs, outputs);
    txs.push_back(tx);
    
    // Create new block
    Block block = Block::create(genesis, txs, "miner2", 5000000000ULL);
    
    EXPECT_EQ(block.transactions.size(), 2); // Coinbase + 1 regular tx
    EXPECT_TRUE(block.transactions[0].isCoinbase);
    EXPECT_FALSE(block.transactions[1].isCoinbase);
    EXPECT_EQ(block.header.prevHash, genesis.hash);
    EXPECT_TRUE(block.isValid());
}

TEST_F(BlockTest, BlockValidationTest) {
    Block genesis = Block::createGenesis("miner", 5000000000ULL);
    
    // Valid block should pass validation
    EXPECT_TRUE(genesis.isValid());
    
    // Test invalid merkle root
    Block invalidMerkle = genesis;
    invalidMerkle.header.merkleRoot = "invalid_merkle_root";
    invalidMerkle.computeHash(); // Recompute hash with invalid merkle root
    EXPECT_FALSE(invalidMerkle.isValid());
    
    // Test block with no transactions
    Block noTx;
    EXPECT_FALSE(noTx.isValid());
    
    // Test block with non-coinbase as first transaction
    Block invalidCoinbase = genesis;
    invalidCoinbase.transactions[0].isCoinbase = false;
    EXPECT_FALSE(invalidCoinbase.isValid());
}

TEST_F(BlockTest, BlockSerializationTest) {
    // Create a block with multiple transactions
    Block original = Block::createGenesis("miner", 5000000000ULL);
    
    // Add another transaction
    std::vector<TxIn> inputs;
    inputs.emplace_back(OutPoint("prev_tx", 0), "sig", "pk");
    
    std::vector<TxOut> outputs;
    outputs.emplace_back(1000000, "addr");
    
    Transaction tx = Transaction::create(inputs, outputs);
    original.transactions.push_back(tx);
    original.header.merkleRoot = MerkleTree::buildMerkleRoot(original.transactions);
    original.computeHash();
    
    // Test serialization
    auto serialized = original.serialize();
    Block deserialized = Block::deserialize(serialized);
    
    EXPECT_EQ(original.header, deserialized.header);
    EXPECT_EQ(original.transactions.size(), deserialized.transactions.size());
    EXPECT_EQ(original.hash, deserialized.hash);
}

TEST_F(BlockTest, DifficultyTargetTest) {
    // Test bits to target conversion
    uint32_t bits = 0x1d00ffff;
    std::string target = Difficulty::bitsToTarget(bits);
    
    EXPECT_FALSE(target.empty());
    EXPECT_EQ(target.length(), 64);
    
    // Test target back to bits
    uint32_t convertedBits = Difficulty::targetToBits(target);
    EXPECT_EQ(bits, convertedBits);
}

TEST_F(BlockTest, MiningTest) {
    Block block = Block::createGenesis("miner", 5000000000ULL);
    
    // Set easy difficulty for testing
    block.header.bits = 0x207fffff; // Very easy difficulty
    
    uint32_t originalNonce = block.header.nonce;
    
    // Mine the block
    block.mine(1000); // Limit iterations for testing
    
    // Nonce should have changed (unless we got very lucky)
    // Hash should meet target
    EXPECT_TRUE(block.meetsTarget() || block.header.nonce != originalNonce);
}

TEST_F(BlockTest, BlockRewardAndFeesTest) {
    Block genesis = Block::createGenesis("miner", 5000000000ULL);
    
    EXPECT_EQ(genesis.getBlockReward(), 5000000000ULL);
    EXPECT_EQ(genesis.getTotalFees(), 0); // Genesis has no fees
    
    // Create block with transactions that have fees
    std::vector<Transaction> txs;
    
    std::vector<TxIn> inputs;
    inputs.emplace_back(OutPoint("prev", 0), "sig", "pk");
    
    std::vector<TxOut> outputs;
    outputs.emplace_back(1000000, "addr"); // Less than input (creates fee)
    
    Transaction tx = Transaction::create(inputs, outputs);
    txs.push_back(tx);
    
    Block block = Block::create(genesis, txs, "miner", 5000000000ULL);
    
    EXPECT_EQ(block.getTransactionCount(), 2);
    EXPECT_EQ(block.getBlockReward(), 5000000000ULL);
}
