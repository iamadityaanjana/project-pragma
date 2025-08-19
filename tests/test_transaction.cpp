#include <gtest/gtest.h>
#include "core/transaction.h"

using namespace pragma;

class TransactionTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TransactionTest, TxOutSerializationTest) {
    TxOut original(100000000, "1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa");
    
    auto serialized = original.serialize();
    size_t offset = 0;
    TxOut deserialized = TxOut::deserialize(serialized, offset);
    
    EXPECT_EQ(original, deserialized);
    EXPECT_EQ(original.value, deserialized.value);
    EXPECT_EQ(original.pubKeyHash, deserialized.pubKeyHash);
}

TEST_F(TransactionTest, OutPointSerializationTest) {
    OutPoint original("abc123def456", 1);
    
    auto serialized = original.serialize();
    size_t offset = 0;
    OutPoint deserialized = OutPoint::deserialize(serialized, offset);
    
    EXPECT_EQ(original, deserialized);
    EXPECT_EQ(original.txid, deserialized.txid);
    EXPECT_EQ(original.index, deserialized.index);
}

TEST_F(TransactionTest, TxInSerializationTest) {
    OutPoint prevout("abc123def456", 0);
    TxIn original(prevout, "signature_data", "public_key_data");
    
    auto serialized = original.serialize();
    size_t offset = 0;
    TxIn deserialized = TxIn::deserialize(serialized, offset);
    
    EXPECT_EQ(original, deserialized);
    EXPECT_EQ(original.prevout, deserialized.prevout);
    EXPECT_EQ(original.sig, deserialized.sig);
    EXPECT_EQ(original.pubKey, deserialized.pubKey);
}

TEST_F(TransactionTest, CoinbaseTransactionTest) {
    std::string minerAddress = "1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa";
    uint64_t reward = 5000000000ULL; // 50 BTC in satoshis
    
    Transaction coinbase = Transaction::createCoinbase(minerAddress, reward);
    
    EXPECT_TRUE(coinbase.isCoinbase);
    EXPECT_TRUE(coinbase.vin.empty()); // Coinbase has no inputs
    EXPECT_EQ(coinbase.vout.size(), 1);
    EXPECT_EQ(coinbase.vout[0].value, reward);
    EXPECT_EQ(coinbase.vout[0].pubKeyHash, minerAddress);
    EXPECT_FALSE(coinbase.txid.empty());
    EXPECT_TRUE(coinbase.isValid());
}

TEST_F(TransactionTest, RegularTransactionTest) {
    // Create inputs
    std::vector<TxIn> inputs;
    OutPoint prevout("previous_tx_hash", 0);
    inputs.emplace_back(prevout, "signature", "pubkey");
    
    // Create outputs
    std::vector<TxOut> outputs;
    outputs.emplace_back(1000000, "address1"); // 0.01 BTC
    outputs.emplace_back(2000000, "address2"); // 0.02 BTC
    
    Transaction tx = Transaction::create(inputs, outputs);
    
    EXPECT_FALSE(tx.isCoinbase);
    EXPECT_EQ(tx.vin.size(), 1);
    EXPECT_EQ(tx.vout.size(), 2);
    EXPECT_EQ(tx.getTotalOutput(), 3000000);
    EXPECT_FALSE(tx.txid.empty());
    EXPECT_TRUE(tx.isValid());
}

TEST_F(TransactionTest, TransactionSerializationTest) {
    // Create a transaction
    std::vector<TxIn> inputs;
    OutPoint prevout("abcdef123456", 1);
    inputs.emplace_back(prevout, "sig_data", "pk_data");
    
    std::vector<TxOut> outputs;
    outputs.emplace_back(1000000, "addr1");
    outputs.emplace_back(2000000, "addr2");
    
    Transaction original = Transaction::create(inputs, outputs);
    
    // Serialize and deserialize
    auto serialized = original.serialize();
    Transaction deserialized = Transaction::deserialize(serialized);
    
    EXPECT_EQ(original, deserialized);
    EXPECT_EQ(original.isCoinbase, deserialized.isCoinbase);
    EXPECT_EQ(original.vin.size(), deserialized.vin.size());
    EXPECT_EQ(original.vout.size(), deserialized.vout.size());
    EXPECT_EQ(original.txid, deserialized.txid);
}

TEST_F(TransactionTest, TransactionValidationTest) {
    // Test invalid transactions
    Transaction invalid1;
    EXPECT_FALSE(invalid1.isValid()); // No outputs
    
    // Coinbase with inputs (invalid)
    Transaction invalid2;
    invalid2.isCoinbase = true;
    invalid2.vin.emplace_back(OutPoint("test", 0), "sig", "pk");
    invalid2.vout.emplace_back(1000, "addr");
    EXPECT_FALSE(invalid2.isValid());
    
    // Non-coinbase without inputs (invalid)
    Transaction invalid3;
    invalid3.isCoinbase = false;
    invalid3.vout.emplace_back(1000, "addr");
    EXPECT_FALSE(invalid3.isValid());
    
    // Zero-value output (invalid)
    Transaction invalid4;
    invalid4.isCoinbase = false;
    invalid4.vin.emplace_back(OutPoint("test", 0), "sig", "pk");
    invalid4.vout.emplace_back(0, "addr"); // Zero value
    EXPECT_FALSE(invalid4.isValid());
}

TEST_F(TransactionTest, DuplicateInputsTest) {
    Transaction tx;
    tx.isCoinbase = false;
    
    // Add duplicate inputs (same outpoint)
    OutPoint same_outpoint("same_tx", 0);
    tx.vin.emplace_back(same_outpoint, "sig1", "pk1");
    tx.vin.emplace_back(same_outpoint, "sig2", "pk2"); // Duplicate!
    
    tx.vout.emplace_back(1000, "addr");
    
    EXPECT_FALSE(tx.isValid()); // Should be invalid due to duplicate inputs
}
