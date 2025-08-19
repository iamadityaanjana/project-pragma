#include <gtest/gtest.h>
#include "core/merkle.h"
#include "core/transaction.h"

using namespace pragma;

class MerkleTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(MerkleTest, EmptyMerkleRootTest) {
    std::vector<std::string> empty;
    std::string root = MerkleTree::buildMerkleRoot(empty);
    EXPECT_TRUE(root.empty());
}

TEST_F(MerkleTest, SingleTransactionMerkleRootTest) {
    std::vector<std::string> single = {"abc123"};
    std::string root = MerkleTree::buildMerkleRoot(single);
    EXPECT_EQ(root, "abc123");
}

TEST_F(MerkleTest, TwoTransactionMerkleRootTest) {
    std::vector<std::string> txids = {
        "abc123def456",
        "789ghi012jkl"
    };
    
    std::string root = MerkleTree::buildMerkleRoot(txids);
    EXPECT_FALSE(root.empty());
    EXPECT_NE(root, txids[0]);
    EXPECT_NE(root, txids[1]);
    
    // Result should be deterministic
    std::string root2 = MerkleTree::buildMerkleRoot(txids);
    EXPECT_EQ(root, root2);
}

TEST_F(MerkleTest, ThreeTransactionMerkleRootTest) {
    std::vector<std::string> txids = {
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
        "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
    };
    
    std::string root = MerkleTree::buildMerkleRoot(txids);
    EXPECT_FALSE(root.empty());
    
    // With 3 transactions, the last one should be duplicated
    // Let's verify the structure manually
    std::vector<std::string> evenTxids = {
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
        "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc",
        "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc" // Duplicated
    };
    
    std::string rootEven = MerkleTree::buildMerkleRoot(evenTxids);
    EXPECT_EQ(root, rootEven);
}

TEST_F(MerkleTest, FourTransactionMerkleRootTest) {
    std::vector<std::string> txids = {
        "1111111111111111111111111111111111111111111111111111111111111111",
        "2222222222222222222222222222222222222222222222222222222222222222",
        "3333333333333333333333333333333333333333333333333333333333333333",
        "4444444444444444444444444444444444444444444444444444444444444444"
    };
    
    std::string root = MerkleTree::buildMerkleRoot(txids);
    EXPECT_FALSE(root.empty());
    
    // Test that order matters
    std::vector<std::string> reversedTxids = txids;
    std::reverse(reversedTxids.begin(), reversedTxids.end());
    std::string rootReversed = MerkleTree::buildMerkleRoot(reversedTxids);
    EXPECT_NE(root, rootReversed);
}

TEST_F(MerkleTest, MerkleProofSingleTransactionTest) {
    std::vector<std::string> txids = {"abc123"};
    
    auto proof = MerkleTree::generateMerkleProof(txids, 0);
    EXPECT_TRUE(proof.empty()); // No proof needed for single transaction
    
    std::string root = MerkleTree::buildMerkleRoot(txids);
    bool valid = MerkleTree::verifyMerkleProof(txids[0], root, proof, 0);
    EXPECT_TRUE(valid);
}

TEST_F(MerkleTest, MerkleProofTwoTransactionsTest) {
    std::vector<std::string> txids = {
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
    };
    
    std::string root = MerkleTree::buildMerkleRoot(txids);
    
    // Test proof for first transaction
    auto proof0 = MerkleTree::generateMerkleProof(txids, 0);
    EXPECT_EQ(proof0.size(), 1);
    EXPECT_EQ(proof0[0], txids[1]); // Sibling is the second transaction
    
    bool valid0 = MerkleTree::verifyMerkleProof(txids[0], root, proof0, 0);
    EXPECT_TRUE(valid0);
    
    // Test proof for second transaction
    auto proof1 = MerkleTree::generateMerkleProof(txids, 1);
    EXPECT_EQ(proof1.size(), 1);
    EXPECT_EQ(proof1[0], txids[0]); // Sibling is the first transaction
    
    bool valid1 = MerkleTree::verifyMerkleProof(txids[1], root, proof1, 1);
    EXPECT_TRUE(valid1);
}

TEST_F(MerkleTest, MerkleProofFourTransactionsTest) {
    std::vector<std::string> txids = {
        "1111111111111111111111111111111111111111111111111111111111111111",
        "2222222222222222222222222222222222222222222222222222222222222222",
        "3333333333333333333333333333333333333333333333333333333333333333",
        "4444444444444444444444444444444444444444444444444444444444444444"
    };
    
    std::string root = MerkleTree::buildMerkleRoot(txids);
    
    // Test proof for each transaction
    for (size_t i = 0; i < txids.size(); ++i) {
        auto proof = MerkleTree::generateMerkleProof(txids, i);
        EXPECT_EQ(proof.size(), 2); // Should have 2 levels for 4 transactions
        
        bool valid = MerkleTree::verifyMerkleProof(txids[i], root, proof, i);
        EXPECT_TRUE(valid) << "Proof failed for transaction " << i;
        
        // Test with wrong index should fail
        if (i > 0) {
            bool invalid = MerkleTree::verifyMerkleProof(txids[i], root, proof, i - 1);
            EXPECT_FALSE(invalid) << "Proof should fail for wrong index";
        }
    }
}

TEST_F(MerkleTest, MerkleProofInvalidTest) {
    std::vector<std::string> txids = {
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
    };
    
    std::string root = MerkleTree::buildMerkleRoot(txids);
    auto proof = MerkleTree::generateMerkleProof(txids, 0);
    
    // Test with wrong transaction ID
    bool invalid1 = MerkleTree::verifyMerkleProof("wrong_txid", root, proof, 0);
    EXPECT_FALSE(invalid1);
    
    // Test with wrong root
    bool invalid2 = MerkleTree::verifyMerkleProof(txids[0], "wrong_root", proof, 0);
    EXPECT_FALSE(invalid2);
    
    // Test with modified proof
    auto modifiedProof = proof;
    modifiedProof[0] = "modified_sibling";
    bool invalid3 = MerkleTree::verifyMerkleProof(txids[0], root, modifiedProof, 0);
    EXPECT_FALSE(invalid3);
}

TEST_F(MerkleTest, MerkleRootFromTransactionsTest) {
    // Create some transactions
    Transaction tx1 = Transaction::createCoinbase("addr1", 1000);
    
    std::vector<TxIn> inputs;
    inputs.emplace_back(OutPoint("prev", 0), "sig", "pk");
    std::vector<TxOut> outputs;
    outputs.emplace_back(500, "addr2");
    Transaction tx2 = Transaction::create(inputs, outputs);
    
    std::vector<Transaction> transactions = {tx1, tx2};
    
    // Build merkle root from transactions
    std::string root1 = MerkleTree::buildMerkleRoot(transactions);
    
    // Build merkle root from txids
    std::vector<std::string> txids = {tx1.txid, tx2.txid};
    std::string root2 = MerkleTree::buildMerkleRoot(txids);
    
    EXPECT_EQ(root1, root2);
}
