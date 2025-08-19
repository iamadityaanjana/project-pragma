#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include "transaction.h"

namespace pragma {

/**
 * Block header structure containing metadata for a block
 */
struct BlockHeader {
    uint32_t version;           // Protocol version
    std::string prevHash;       // Hash of previous block (32-byte hex)
    std::string merkleRoot;     // Merkle root of transactions (32-byte hex)
    uint64_t timestamp;         // Block creation time (unix timestamp)
    uint32_t bits;              // Difficulty target in compact format
    uint32_t nonce;             // Proof-of-work nonce
    
    BlockHeader() : version(1), timestamp(0), bits(0x1d00ffff), nonce(0) {}
    
    BlockHeader(uint32_t ver, const std::string& prev, const std::string& merkle, 
                uint64_t time, uint32_t targetBits, uint32_t n = 0)
        : version(ver), prevHash(prev), merkleRoot(merkle), 
          timestamp(time), bits(targetBits), nonce(n) {}
    
    // Serialization
    std::vector<uint8_t> serialize() const;
    static BlockHeader deserialize(const std::vector<uint8_t>& data);
    
    // Hash computation
    std::string computeHash() const;
    
    bool operator==(const BlockHeader& other) const;
};

/**
 * Complete block containing header and transactions
 */
struct Block {
    BlockHeader header;                    // Block header
    std::vector<Transaction> transactions; // Block transactions
    std::string hash;                      // Block hash (computed from header)
    
    Block() = default;
    Block(const BlockHeader& h, const std::vector<Transaction>& txs)
        : header(h), transactions(txs) {
        computeHash();
    }
    
    // Create genesis block
    static Block createGenesis(const std::string& minerAddress, uint64_t reward);
    
    // Create new block from previous block
    static Block create(const Block& prevBlock, const std::vector<Transaction>& txs, 
                       const std::string& minerAddress, uint64_t reward);
    
    // Serialization
    std::vector<uint8_t> serialize() const;
    static Block deserialize(const std::vector<uint8_t>& data);
    
    // Hash computation and validation
    void computeHash();
    std::string computeHash() const;
    bool isValidHash() const;
    
    // Block validation
    bool isValid() const;
    bool isValidMerkleRoot() const;
    
    // Mining and proof-of-work
    bool meetsTarget() const;
    void mine(uint32_t maxIterations = 1000000);
    
    // Getters
    uint32_t getTransactionCount() const { return transactions.size(); }
    uint64_t getTotalFees() const;
    uint64_t getBlockReward() const;
    
    bool operator==(const Block& other) const;
};

} // namespace pragma
