#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace pragma {

// Transaction structures
struct TransactionInput {
    std::string previousTxHash;
    uint32_t outputIndex = 0;
    std::string scriptSig;
    uint32_t sequence = 0xFFFFFFFF;
};

struct TransactionOutput {
    double amount = 0.0;
    std::string scriptPubKey;
    std::string address;
};

struct Transaction {
    std::string hash;
    uint32_t version = 1;
    std::vector<TransactionInput> inputs;
    std::vector<TransactionOutput> outputs;
    uint32_t lockTime = 0;
    uint64_t timestamp = 0;
    double fee = 0.0;
    
    Transaction() {
        timestamp = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
};

// Block structures
struct Block {
    std::string hash;
    uint32_t version = 1;
    std::string previousHash;
    std::string merkleRoot;
    uint64_t timestamp = 0;
    uint32_t bits = 0x1d00ffff;
    uint32_t nonce = 0;
    uint32_t height = 0;
    std::vector<Transaction> transactions;
    
    Block() {
        timestamp = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
};

// Chain entry for blockchain management
struct ChainEntry {
    std::string hash;
    uint32_t height = 0;
    Block block;
    std::string previousHash;
    uint64_t chainWork = 0;
    bool isMainChain = false;
};

// Utility functions
std::string calculateTransactionHash(const Transaction& tx);
std::string calculateBlockHash(const Block& block);
std::string calculateMerkleRoot(const std::vector<Transaction>& transactions);
bool validateTransactionSignature(const Transaction& tx);
bool validateBlockHash(const Block& block, uint32_t difficulty = 1);

// Hash utility implementation
inline std::string sha256(const std::string& input) {
    // Simplified hash for demonstration
    std::hash<std::string> hasher;
    size_t hashValue = hasher(input);
    
    std::stringstream ss;
    ss << std::hex << hashValue;
    std::string result = ss.str();
    
    // Pad to 64 characters
    while (result.length() < 64) {
        result = "0" + result;
    }
    
    return result;
}

inline std::string calculateTransactionHash(const Transaction& tx) {
    std::stringstream ss;
    ss << tx.version;
    
    for (const auto& input : tx.inputs) {
        ss << input.previousTxHash << input.outputIndex << input.scriptSig;
    }
    
    for (const auto& output : tx.outputs) {
        ss << output.amount << output.scriptPubKey << output.address;
    }
    
    ss << tx.lockTime << tx.timestamp;
    
    return sha256(ss.str());
}

inline std::string calculateBlockHash(const Block& block) {
    std::stringstream ss;
    ss << block.version << block.previousHash << block.merkleRoot;
    ss << block.timestamp << block.bits << block.nonce;
    
    return sha256(ss.str());
}

inline std::string calculateMerkleRoot(const std::vector<Transaction>& transactions) {
    if (transactions.empty()) {
        return std::string(64, '0');
    }
    
    std::vector<std::string> hashes;
    for (const auto& tx : transactions) {
        hashes.push_back(tx.hash);
    }
    
    // Simple merkle root calculation
    while (hashes.size() > 1) {
        std::vector<std::string> newLevel;
        
        for (size_t i = 0; i < hashes.size(); i += 2) {
            std::string combined = hashes[i];
            if (i + 1 < hashes.size()) {
                combined += hashes[i + 1];
            } else {
                combined += hashes[i]; // Duplicate if odd number
            }
            newLevel.push_back(sha256(combined));
        }
        
        hashes = newLevel;
    }
    
    return hashes[0];
}

inline bool validateTransactionSignature(const Transaction& tx) {
    // Simplified validation - in real implementation would verify digital signatures
    return !tx.hash.empty() && !tx.outputs.empty();
}

inline bool validateBlockHash(const Block& block, uint32_t difficulty) {
    std::string hash = calculateBlockHash(block);
    std::string target(difficulty, '0');
    return hash.substr(0, difficulty) == target;
}

} // namespace pragma
