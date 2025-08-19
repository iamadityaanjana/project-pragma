#include <gtest/gtest.h>
#include "core/utxo.h"
#include "core/transaction.h"

using namespace pragma;

class UTXOTest : public ::testing::Test {
protected:
    void SetUp() override {
        utxoSet = std::make_unique<UTXOSet>();
        
        // Create test transactions
        coinbaseTx = Transaction::createCoinbase("1CoinbaseAddress", 5000000000ULL);
        
        std::vector<TxIn> inputs;
        inputs.emplace_back(OutPoint(coinbaseTx.txid, 0), "sig", "pubkey");
        
        std::vector<TxOut> outputs;
        outputs.emplace_back(1000000000ULL, "1RecipientAddress1");
        outputs.emplace_back(3999999000ULL, "1RecipientAddress2");
        
        regularTx = Transaction::create(inputs, outputs);
    }
    
    void TearDown() override {
        utxoSet.reset();
    }
    
    std::unique_ptr<UTXOSet> utxoSet;
    Transaction coinbaseTx;
    Transaction regularTx;
};

TEST_F(UTXOTest, InitialStateTest) {
    EXPECT_EQ(utxoSet->size(), 0);
    EXPECT_EQ(utxoSet->getTotalValue(), 0);
    EXPECT_TRUE(utxoSet->isEmpty());
    EXPECT_EQ(utxoSet->getCurrentHeight(), 0);
}

TEST_F(UTXOTest, AddUTXOTest) {
    OutPoint outpoint("test_tx", 0);
    TxOut output(1000000, "1TestAddress");
    
    EXPECT_TRUE(utxoSet->addUTXO(outpoint, output, 1, false));
    EXPECT_EQ(utxoSet->size(), 1);
    EXPECT_EQ(utxoSet->getTotalValue(), 1000000);
    EXPECT_FALSE(utxoSet->isEmpty());
    
    // Test duplicate addition
    EXPECT_FALSE(utxoSet->addUTXO(outpoint, output, 1, false));
    EXPECT_EQ(utxoSet->size(), 1); // Should not increase
}

TEST_F(UTXOTest, GetUTXOTest) {
    OutPoint outpoint("test_tx", 0);
    TxOut output(1000000, "1TestAddress");
    
    // Test non-existent UTXO
    const UTXO* utxo = utxoSet->getUTXO(outpoint);
    EXPECT_EQ(utxo, nullptr);
    EXPECT_FALSE(utxoSet->hasUTXO(outpoint));
    
    // Add UTXO and test retrieval
    utxoSet->addUTXO(outpoint, output, 1, true);
    utxo = utxoSet->getUTXO(outpoint);
    
    ASSERT_NE(utxo, nullptr);
    EXPECT_EQ(utxo->output.value, 1000000);
    EXPECT_EQ(utxo->output.scriptPubKey, "1TestAddress");
    EXPECT_EQ(utxo->height, 1);
    EXPECT_TRUE(utxo->isCoinbase);
    EXPECT_TRUE(utxoSet->hasUTXO(outpoint));
}

TEST_F(UTXOTest, RemoveUTXOTest) {
    OutPoint outpoint("test_tx", 0);
    TxOut output(1000000, "1TestAddress");
    
    // Test removing non-existent UTXO
    EXPECT_FALSE(utxoSet->removeUTXO(outpoint));
    
    // Add UTXO and then remove it
    utxoSet->addUTXO(outpoint, output, 1, false);
    EXPECT_EQ(utxoSet->size(), 1);
    EXPECT_EQ(utxoSet->getTotalValue(), 1000000);
    
    EXPECT_TRUE(utxoSet->removeUTXO(outpoint));
    EXPECT_EQ(utxoSet->size(), 0);
    EXPECT_EQ(utxoSet->getTotalValue(), 0);
    EXPECT_FALSE(utxoSet->hasUTXO(outpoint));
}

TEST_F(UTXOTest, CoinbaseMaturityTest) {
    OutPoint outpoint("coinbase_tx", 0);
    TxOut output(5000000000ULL, "1MinerAddress");
    
    utxoSet->addUTXO(outpoint, output, 1, true);
    utxoSet->setCurrentHeight(50); // Height 50
    
    const UTXO* utxo = utxoSet->getUTXO(outpoint);
    ASSERT_NE(utxo, nullptr);
    
    // Should not be spendable yet (need 100 confirmations for coinbase)
    EXPECT_FALSE(utxo->isSpendable(50));
    
    // Should be spendable after maturity
    EXPECT_TRUE(utxo->isSpendable(101));
}

TEST_F(UTXOTest, ApplyTransactionTest) {
    // First apply coinbase transaction
    EXPECT_TRUE(utxoSet->applyTransaction(coinbaseTx, 1));
    EXPECT_EQ(utxoSet->size(), 1);
    EXPECT_EQ(utxoSet->getTotalValue(), 5000000000ULL);
    
    // Check that coinbase UTXO was created
    OutPoint coinbaseOutpoint(coinbaseTx.txid, 0);
    const UTXO* coinbaseUTXO = utxoSet->getUTXO(coinbaseOutpoint);
    ASSERT_NE(coinbaseUTXO, nullptr);
    EXPECT_TRUE(coinbaseUTXO->isCoinbase);
    EXPECT_EQ(coinbaseUTXO->output.value, 5000000000ULL);
}

TEST_F(UTXOTest, ApplyRegularTransactionTest) {
    // First apply coinbase to create UTXO to spend
    utxoSet->applyTransaction(coinbaseTx, 1);
    utxoSet->setCurrentHeight(101); // Make coinbase spendable
    
    // Now apply regular transaction
    EXPECT_TRUE(utxoSet->applyTransaction(regularTx, 101));
    
    // Should have 2 new UTXOs, coinbase UTXO should be removed
    EXPECT_EQ(utxoSet->size(), 2);
    
    // Check coinbase UTXO was spent
    OutPoint coinbaseOutpoint(coinbaseTx.txid, 0);
    EXPECT_FALSE(utxoSet->hasUTXO(coinbaseOutpoint));
    
    // Check new UTXOs were created
    OutPoint outpoint1(regularTx.txid, 0);
    OutPoint outpoint2(regularTx.txid, 1);
    
    const UTXO* utxo1 = utxoSet->getUTXO(outpoint1);
    const UTXO* utxo2 = utxoSet->getUTXO(outpoint2);
    
    ASSERT_NE(utxo1, nullptr);
    ASSERT_NE(utxo2, nullptr);
    
    EXPECT_EQ(utxo1->output.value, 1000000000ULL);
    EXPECT_EQ(utxo2->output.value, 3999999000ULL);
    EXPECT_FALSE(utxo1->isCoinbase);
    EXPECT_FALSE(utxo2->isCoinbase);
}

TEST_F(UTXOTest, TransactionValidationTest) {
    // Apply coinbase first
    utxoSet->applyTransaction(coinbaseTx, 1);
    utxoSet->setCurrentHeight(101);
    
    // Regular transaction should be valid now
    EXPECT_TRUE(utxoSet->validateTransaction(regularTx));
    
    // Test invalid transaction (spending non-existent UTXO)
    std::vector<TxIn> invalidInputs;
    invalidInputs.emplace_back(OutPoint("nonexistent", 0), "sig", "pubkey");
    
    std::vector<TxOut> outputs;
    outputs.emplace_back(1000000, "1Address");
    
    Transaction invalidTx = Transaction::create(invalidInputs, outputs);
    EXPECT_FALSE(utxoSet->validateTransaction(invalidTx));
}

TEST_F(UTXOTest, TransactionFeeTest) {
    // Apply coinbase and regular transaction
    utxoSet->applyTransaction(coinbaseTx, 1);
    utxoSet->setCurrentHeight(101);
    
    uint64_t fee = utxoSet->calculateTransactionFee(regularTx);
    uint64_t expectedFee = 5000000000ULL - (1000000000ULL + 3999999000ULL);
    EXPECT_EQ(fee, expectedFee);
    
    // Coinbase transaction should have no fee
    EXPECT_EQ(utxoSet->calculateTransactionFee(coinbaseTx), 0);
}

TEST_F(UTXOTest, BalanceQueryTest) {
    // Apply coinbase
    utxoSet->applyTransaction(coinbaseTx, 1);
    utxoSet->setCurrentHeight(101);
    
    // Check balance for coinbase address
    std::string coinbaseAddress = "1CoinbaseAddress";
    uint64_t balance = utxoSet->getBalanceForAddress(coinbaseAddress);
    EXPECT_EQ(balance, 5000000000ULL);
    
    // Apply regular transaction
    utxoSet->applyTransaction(regularTx, 101);
    
    // Check balances after spending
    EXPECT_EQ(utxoSet->getBalanceForAddress(coinbaseAddress), 0);
    EXPECT_EQ(utxoSet->getBalanceForAddress("1RecipientAddress1"), 1000000000ULL);
    EXPECT_EQ(utxoSet->getBalanceForAddress("1RecipientAddress2"), 3999999000ULL);
}

TEST_F(UTXOTest, UndoTransactionTest) {
    // Apply coinbase and regular transaction
    utxoSet->applyTransaction(coinbaseTx, 1);
    utxoSet->setCurrentHeight(101);
    
    std::vector<UTXO> spentUTXOs = utxoSet->getSpentUTXOs(regularTx);
    utxoSet->applyTransaction(regularTx, 101);
    
    EXPECT_EQ(utxoSet->size(), 2); // 2 outputs from regular tx
    
    // Undo the regular transaction
    EXPECT_TRUE(utxoSet->undoTransaction(regularTx, spentUTXOs));
    
    // Should be back to original state
    EXPECT_EQ(utxoSet->size(), 1); // Just coinbase UTXO
    
    OutPoint coinbaseOutpoint(coinbaseTx.txid, 0);
    EXPECT_TRUE(utxoSet->hasUTXO(coinbaseOutpoint));
}

TEST_F(UTXOTest, BlockOperationsTest) {
    std::vector<Transaction> blockTxs = {coinbaseTx, regularTx};
    
    // Cannot apply block with regular tx before coinbase is mature
    utxoSet->setCurrentHeight(1);
    EXPECT_FALSE(utxoSet->applyBlock(blockTxs, 1));
    
    // Should work if we make coinbase first mature
    utxoSet->clear();
    utxoSet->applyTransaction(coinbaseTx, 1);
    utxoSet->setCurrentHeight(101);
    
    std::vector<Transaction> laterBlockTxs = {regularTx};
    EXPECT_TRUE(utxoSet->applyBlock(laterBlockTxs, 101));
}

TEST_F(UTXOTest, UTXOStatsTest) {
    // Apply some transactions
    utxoSet->applyTransaction(coinbaseTx, 1);
    utxoSet->setCurrentHeight(101);
    utxoSet->applyTransaction(regularTx, 101);
    
    auto stats = utxoSet->getStats();
    
    EXPECT_EQ(stats.totalUTXOs, 2);
    EXPECT_EQ(stats.totalValue, 1000000000ULL + 3999999000ULL);
    EXPECT_EQ(stats.coinbaseUTXOs, 0); // Coinbase was spent
    EXPECT_EQ(stats.matureUTXOs, 2); // Both regular UTXOs are mature
    EXPECT_GT(stats.averageUTXOValue, 0);
}

TEST_F(UTXOTest, PersistenceTest) {
    // Add some UTXOs
    utxoSet->applyTransaction(coinbaseTx, 1);
    utxoSet->setCurrentHeight(101);
    utxoSet->applyTransaction(regularTx, 101);
    
    std::string filename = "/tmp/test_utxo.dat";
    
    // Save UTXO set
    EXPECT_TRUE(utxoSet->saveToFile(filename));
    
    // Create new UTXO set and load
    auto newUTXOSet = std::make_unique<UTXOSet>();
    EXPECT_TRUE(newUTXOSet->loadFromFile(filename));
    
    // Verify loaded state
    EXPECT_EQ(newUTXOSet->size(), utxoSet->size());
    EXPECT_EQ(newUTXOSet->getTotalValue(), utxoSet->getTotalValue());
    EXPECT_EQ(newUTXOSet->getCurrentHeight(), utxoSet->getCurrentHeight());
    
    // Clean up
    std::remove(filename.c_str());
}

TEST_F(UTXOTest, UTXOCacheTest) {
    // Add base UTXO
    utxoSet->applyTransaction(coinbaseTx, 1);
    
    UTXOCache cache(utxoSet.get());
    
    // Add UTXO to cache
    OutPoint testOutpoint("cache_test", 0);
    TxOut testOutput(1000000, "1CacheAddress");
    
    EXPECT_TRUE(cache.addUTXO(testOutpoint, testOutput, 1, false));
    EXPECT_TRUE(cache.hasUTXO(testOutpoint));
    EXPECT_FALSE(utxoSet->hasUTXO(testOutpoint)); // Not in base set yet
    
    // Flush cache
    cache.flush();
    EXPECT_TRUE(utxoSet->hasUTXO(testOutpoint)); // Now in base set
}

TEST_F(UTXOTest, SubsidyCalculationTest) {
    // Test block subsidy calculation
    EXPECT_EQ(UTXOSet::getBlockSubsidy(0), 5000000000ULL); // 50 BTC
    EXPECT_EQ(UTXOSet::getBlockSubsidy(210000), 2500000000ULL); // 25 BTC after first halving
    EXPECT_EQ(UTXOSet::getBlockSubsidy(420000), 1250000000ULL); // 12.5 BTC after second halving
    
    // Test coinbase maturity
    EXPECT_FALSE(UTXOSet::isCoinbaseMatured(1, 50, 100));
    EXPECT_TRUE(UTXOSet::isCoinbaseMatured(1, 101, 100));
}

TEST_F(UTXOTest, UTXOSelectionTest) {
    // Create multiple UTXOs for the same address
    std::string address = "1TestAddress";
    
    OutPoint outpoint1("tx1", 0);
    OutPoint outpoint2("tx2", 0);
    OutPoint outpoint3("tx3", 0);
    
    TxOut output1(1000000, address);
    TxOut output2(2000000, address);
    TxOut output3(5000000, address);
    
    utxoSet->addUTXO(outpoint1, output1, 1, false);
    utxoSet->addUTXO(outpoint2, output2, 1, false);
    utxoSet->addUTXO(outpoint3, output3, 1, false);
    
    utxoSet->setCurrentHeight(1);
    
    // Test UTXO selection for amount
    auto selected = utxoSet->getSpendableUTXOs(address, 3000000);
    EXPECT_GE(selected.size(), 2); // Should select at least 2 UTXOs
    
    // Test insufficient funds
    auto insufficient = utxoSet->getSpendableUTXOs(address, 10000000);
    EXPECT_TRUE(insufficient.empty());
}
