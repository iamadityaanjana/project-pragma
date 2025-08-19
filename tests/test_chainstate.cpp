#include <gtest/gtest.h>
#include "core/chainstate.h"
#include "core/block.h"

using namespace pragma;

class ChainStateTest : public ::testing::Test {
protected:
    void SetUp() override {
        chainState = std::make_unique<ChainState>();
    }
    
    void TearDown() override {
        chainState.reset();
    }
    
    std::unique_ptr<ChainState> chainState;
    
    Block createTestGenesis() {
        return Block::createGenesis("1GenesisAddress", 5000000000ULL);
    }
    
    Block createTestBlock(const Block& prev, const std::string& minerAddress = "1MinerAddress") {
        std::vector<Transaction> txs;
        return Block::create(prev, txs, minerAddress, 5000000000ULL);
    }
};

TEST_F(ChainStateTest, InitialStateTest) {
    EXPECT_FALSE(chainState->hasGenesis());
    EXPECT_EQ(chainState->getBestHeight(), 0);
    EXPECT_EQ(chainState->getTotalBlocks(), 0);
    EXPECT_TRUE(chainState->getBestHash().empty());
    EXPECT_EQ(chainState->getBestTip(), nullptr);
}

TEST_F(ChainStateTest, GenesisBlockTest) {
    Block genesis = createTestGenesis();
    
    EXPECT_TRUE(chainState->setGenesis(genesis));
    EXPECT_TRUE(chainState->hasGenesis());
    EXPECT_EQ(chainState->getBestHeight(), 0);
    EXPECT_EQ(chainState->getTotalBlocks(), 1);
    EXPECT_EQ(chainState->getBestHash(), genesis.hash);
    
    auto tip = chainState->getBestTip();
    ASSERT_NE(tip, nullptr);
    EXPECT_EQ(tip->block.hash, genesis.hash);
    EXPECT_EQ(tip->height, 0);
}

TEST_F(ChainStateTest, DuplicateGenesisTest) {
    Block genesis = createTestGenesis();
    
    EXPECT_TRUE(chainState->setGenesis(genesis));
    EXPECT_FALSE(chainState->setGenesis(genesis)); // Should fail on duplicate
}

TEST_F(ChainStateTest, GetBlockTest) {
    Block genesis = createTestGenesis();
    chainState->setGenesis(genesis);
    
    auto entry = chainState->getBlock(genesis.hash);
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->block.hash, genesis.hash);
    EXPECT_EQ(entry->height, 0);
    
    // Test non-existent block
    auto nullEntry = chainState->getBlock("nonexistent");
    EXPECT_EQ(nullEntry, nullptr);
}

TEST_F(ChainStateTest, AddSingleBlockTest) {
    Block genesis = createTestGenesis();
    chainState->setGenesis(genesis);
    
    Block block1 = createTestBlock(genesis);
    EXPECT_TRUE(chainState->addBlock(block1));
    
    EXPECT_EQ(chainState->getBestHeight(), 1);
    EXPECT_EQ(chainState->getTotalBlocks(), 2);
    EXPECT_EQ(chainState->getBestHash(), block1.hash);
    
    auto tip = chainState->getBestTip();
    ASSERT_NE(tip, nullptr);
    EXPECT_EQ(tip->block.hash, block1.hash);
    EXPECT_EQ(tip->height, 1);
    EXPECT_GT(tip->totalWork, 0);
}

TEST_F(ChainStateTest, AddMultipleBlocksTest) {
    Block genesis = createTestGenesis();
    chainState->setGenesis(genesis);
    
    Block block1 = createTestBlock(genesis);
    Block block2 = createTestBlock(block1);
    Block block3 = createTestBlock(block2);
    
    EXPECT_TRUE(chainState->addBlock(block1));
    EXPECT_TRUE(chainState->addBlock(block2));
    EXPECT_TRUE(chainState->addBlock(block3));
    
    EXPECT_EQ(chainState->getBestHeight(), 3);
    EXPECT_EQ(chainState->getTotalBlocks(), 4);
    EXPECT_EQ(chainState->getBestHash(), block3.hash);
}

TEST_F(ChainStateTest, InvalidBlockTest) {
    Block genesis = createTestGenesis();
    chainState->setGenesis(genesis);
    
    // Try to add block with invalid previous hash
    Block invalidBlock = createTestBlock(genesis);
    invalidBlock.header.prevHash = "invalid_hash";
    invalidBlock.computeHash();
    
    EXPECT_FALSE(chainState->addBlock(invalidBlock));
    EXPECT_EQ(chainState->getBestHeight(), 0); // Should remain unchanged
}

TEST_F(ChainStateTest, DuplicateBlockTest) {
    Block genesis = createTestGenesis();
    chainState->setGenesis(genesis);
    
    Block block1 = createTestBlock(genesis);
    
    EXPECT_TRUE(chainState->addBlock(block1));
    EXPECT_FALSE(chainState->addBlock(block1)); // Duplicate should fail
    
    EXPECT_EQ(chainState->getTotalBlocks(), 2); // Should not increase
}

TEST_F(ChainStateTest, ChainPathTest) {
    Block genesis = createTestGenesis();
    chainState->setGenesis(genesis);
    
    Block block1 = createTestBlock(genesis);
    Block block2 = createTestBlock(block1);
    
    chainState->addBlock(block1);
    chainState->addBlock(block2);
    
    auto path = chainState->getChainPath(genesis.hash, block2.hash);
    EXPECT_EQ(path.size(), 3);
    EXPECT_EQ(path[0], genesis.hash);
    EXPECT_EQ(path[1], block1.hash);
    EXPECT_EQ(path[2], block2.hash);
}

TEST_F(ChainStateTest, GetChainTest) {
    Block genesis = createTestGenesis();
    chainState->setGenesis(genesis);
    
    Block block1 = createTestBlock(genesis);
    Block block2 = createTestBlock(block1);
    
    chainState->addBlock(block1);
    chainState->addBlock(block2);
    
    auto chain = chainState->getChain();
    EXPECT_EQ(chain.size(), 3);
    EXPECT_EQ(chain[0]->height, 0);
    EXPECT_EQ(chain[1]->height, 1);
    EXPECT_EQ(chain[2]->height, 2);
    
    // Test limited chain
    auto limitedChain = chainState->getChain(2);
    EXPECT_EQ(limitedChain.size(), 2);
}

TEST_F(ChainStateTest, ChainStatsTest) {
    Block genesis = createTestGenesis();
    chainState->setGenesis(genesis);
    
    Block block1 = createTestBlock(genesis);
    chainState->addBlock(block1);
    
    auto stats = chainState->getChainStats();
    EXPECT_EQ(stats.height, 1);
    EXPECT_EQ(stats.totalBlocks, 2);
    EXPECT_EQ(stats.bestHash, block1.hash);
    EXPECT_GT(stats.averageBlockTime, 0);
}

TEST_F(ChainStateTest, BlockValidationTest) {
    Block genesis = createTestGenesis();
    chainState->setGenesis(genesis);
    
    // Test valid block
    Block validBlock = createTestBlock(genesis);
    EXPECT_TRUE(chainState->isValidBlock(validBlock));
    
    // Test block with future timestamp
    Block futureBlock = createTestBlock(genesis);
    futureBlock.header.timestamp = Utils::getCurrentTimestamp() + 10000; // Far future
    futureBlock.computeHash();
    EXPECT_FALSE(chainState->isValidBlock(futureBlock));
}

TEST_F(ChainStateTest, WorkCalculationTest) {
    Block genesis = createTestGenesis();
    chainState->setGenesis(genesis);
    
    Block block1 = createTestBlock(genesis);
    chainState->addBlock(block1);
    
    auto genesisEntry = chainState->getBlock(genesis.hash);
    auto block1Entry = chainState->getBlock(block1.hash);
    
    ASSERT_NE(genesisEntry, nullptr);
    ASSERT_NE(block1Entry, nullptr);
    
    // Block 1 should have more total work than genesis
    EXPECT_GT(block1Entry->totalWork, genesisEntry->totalWork);
}

TEST_F(ChainStateTest, ForkDetectionTest) {
    Block genesis = createTestGenesis();
    chainState->setGenesis(genesis);
    
    Block block1a = createTestBlock(genesis, "miner1");
    Block block1b = createTestBlock(genesis, "miner2");
    
    chainState->addBlock(block1a);
    chainState->addBlock(block1b); // This creates a fork
    
    EXPECT_EQ(chainState->getTotalBlocks(), 3); // Genesis + 2 competing blocks
    
    // The chain should follow the block with higher work
    // (In our simplified implementation, this might be the last one added)
}

TEST_F(ChainStateTest, DisconnectBlockTest) {
    Block genesis = createTestGenesis();
    chainState->setGenesis(genesis);
    
    Block block1 = createTestBlock(genesis);
    chainState->addBlock(block1);
    
    EXPECT_EQ(chainState->getBestHeight(), 1);
    
    // Disconnect the block
    EXPECT_TRUE(chainState->disconnectBlock(block1.hash));
    
    // Should not be able to disconnect genesis
    EXPECT_FALSE(chainState->disconnectBlock(genesis.hash));
}

TEST_F(ChainStateTest, SaveAndLoadTest) {
    Block genesis = createTestGenesis();
    chainState->setGenesis(genesis);
    
    Block block1 = createTestBlock(genesis);
    chainState->addBlock(block1);
    
    std::string filename = "/tmp/test_chainstate.dat";
    
    // Save chain state
    EXPECT_TRUE(chainState->saveToFile(filename));
    
    // Create new chain state and load
    auto newChainState = std::make_unique<ChainState>();
    EXPECT_TRUE(newChainState->loadFromFile(filename));
    
    // Verify loaded state
    EXPECT_EQ(newChainState->getBestHeight(), chainState->getBestHeight());
    EXPECT_EQ(newChainState->getTotalBlocks(), chainState->getTotalBlocks());
    EXPECT_EQ(newChainState->getBestHash(), chainState->getBestHash());
    
    // Clean up
    std::remove(filename.c_str());
}
