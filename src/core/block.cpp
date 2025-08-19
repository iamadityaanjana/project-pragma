#include "block.h"
#include "merkle.h"
#include "difficulty.h"
#include "../primitives/serialize.h"
#include "../primitives/hash.h"
#include "../primitives/utils.h"
#include <algorithm>

namespace pragma {

// BlockHeader implementation
std::vector<uint8_t> BlockHeader::serialize() const {
    std::vector<std::vector<uint8_t>> parts;
    
    // Serialize all fields in order
    parts.push_back(Serialize::encodeUint32LE(version));
    parts.push_back(Serialize::encodeString(prevHash));
    parts.push_back(Serialize::encodeString(merkleRoot));
    parts.push_back(Serialize::encodeUint64LE(timestamp));
    parts.push_back(Serialize::encodeUint32LE(bits));
    parts.push_back(Serialize::encodeUint32LE(nonce));
    
    return Serialize::combine(parts);
}

BlockHeader BlockHeader::deserialize(const std::vector<uint8_t>& data) {
    BlockHeader header;
    size_t offset = 0;
    
    // Deserialize all fields in order
    header.version = Serialize::decodeUint32LE(data, offset);
    offset += 4;
    
    auto [prevHashStr, prevHashSize] = Serialize::decodeString(data, offset);
    header.prevHash = prevHashStr;
    offset += prevHashSize;
    
    auto [merkleStr, merkleSize] = Serialize::decodeString(data, offset);
    header.merkleRoot = merkleStr;
    offset += merkleSize;
    
    header.timestamp = Serialize::decodeUint64LE(data, offset);
    offset += 8;
    
    header.bits = Serialize::decodeUint32LE(data, offset);
    offset += 4;
    
    header.nonce = Serialize::decodeUint32LE(data, offset);
    
    return header;
}

std::string BlockHeader::computeHash() const {
    auto serialized = serialize();
    return Hash::dbl_sha256(serialized);
}

bool BlockHeader::operator==(const BlockHeader& other) const {
    return version == other.version &&
           prevHash == other.prevHash &&
           merkleRoot == other.merkleRoot &&
           timestamp == other.timestamp &&
           bits == other.bits &&
           nonce == other.nonce;
}

// Block implementation
Block Block::createGenesis(const std::string& minerAddress, uint64_t reward) {
    Block genesis;
    
    // Create coinbase transaction
    Transaction coinbase = Transaction::createCoinbase(minerAddress, reward);
    genesis.transactions.push_back(coinbase);
    
    // Set up genesis header
    genesis.header.version = 1;
    genesis.header.prevHash = "0000000000000000000000000000000000000000000000000000000000000000";
    genesis.header.merkleRoot = MerkleTree::buildMerkleRoot(genesis.transactions);
    genesis.header.timestamp = Utils::getCurrentTimestamp();
    genesis.header.bits = 0x1d00ffff; // Initial difficulty (easy for testing)
    genesis.header.nonce = 0;
    
    // Compute initial hash
    genesis.computeHash();
    
    return genesis;
}

Block Block::create(const Block& prevBlock, const std::vector<Transaction>& txs, 
                   const std::string& minerAddress, uint64_t reward) {
    Block newBlock;
    
    // Create coinbase transaction for miner
    Transaction coinbase = Transaction::createCoinbase(minerAddress, reward);
    newBlock.transactions.push_back(coinbase);
    
    // Add provided transactions
    for (const auto& tx : txs) {
        newBlock.transactions.push_back(tx);
    }
    
    // Set up header
    newBlock.header.version = prevBlock.header.version;
    newBlock.header.prevHash = prevBlock.hash;
    newBlock.header.merkleRoot = MerkleTree::buildMerkleRoot(newBlock.transactions);
    newBlock.header.timestamp = Utils::getCurrentTimestamp();
    newBlock.header.bits = prevBlock.header.bits; // TODO: Implement difficulty adjustment
    newBlock.header.nonce = 0;
    
    // Compute initial hash
    newBlock.computeHash();
    
    return newBlock;
}

std::vector<uint8_t> Block::serialize() const {
    std::vector<std::vector<uint8_t>> parts;
    
    // Serialize header
    parts.push_back(header.serialize());
    
    // Serialize transaction count
    parts.push_back(Serialize::encodeVarInt(transactions.size()));
    
    // Serialize each transaction
    for (const auto& tx : transactions) {
        parts.push_back(tx.serialize());
    }
    
    return Serialize::combine(parts);
}

Block Block::deserialize(const std::vector<uint8_t>& data) {
    Block block;
    size_t offset = 0;
    
    // Deserialize header first to get its size
    auto headerData = std::vector<uint8_t>(data.begin(), data.end());
    block.header = BlockHeader::deserialize(headerData);
    
    // Skip past header (approximate - we need to recalculate this properly)
    // For now, let's serialize the header again to get its exact size
    auto headerSerialized = block.header.serialize();
    offset = headerSerialized.size();
    
    // Deserialize transaction count
    auto [txCount, txCountSize] = Serialize::decodeVarInt(data, offset);
    offset += txCountSize;
    
    // Deserialize each transaction
    for (uint64_t i = 0; i < txCount; ++i) {
        // Extract transaction data from current offset
        std::vector<uint8_t> txData(data.begin() + offset, data.end());
        Transaction tx = Transaction::deserialize(txData);
        block.transactions.push_back(tx);
        
        // Move offset past this transaction
        auto txSerialized = tx.serialize();
        offset += txSerialized.size();
    }
    
    // Compute block hash
    block.computeHash();
    
    return block;
}

void Block::computeHash() {
    hash = header.computeHash();
}

std::string Block::computeHash() const {
    return header.computeHash();
}

bool Block::isValidHash() const {
    return hash == computeHash();
}

bool Block::isValid() const {
    // Check if hash is valid
    if (!isValidHash()) {
        return false;
    }
    
    // Check if merkle root matches
    if (!isValidMerkleRoot()) {
        return false;
    }
    
    // Check if block has at least one transaction (coinbase)
    if (transactions.empty()) {
        return false;
    }
    
    // Check if first transaction is coinbase
    if (!transactions[0].isCoinbase) {
        return false;
    }
    
    // Check that only first transaction is coinbase
    for (size_t i = 1; i < transactions.size(); ++i) {
        if (transactions[i].isCoinbase) {
            return false;
        }
    }
    
    // Validate all transactions
    for (const auto& tx : transactions) {
        if (!tx.isValid()) {
            return false;
        }
    }
    
    // Check timestamp (should not be too far in future)
    uint64_t currentTime = Utils::getCurrentTimestamp();
    if (header.timestamp > currentTime + 7200) { // 2 hours tolerance
        return false;
    }
    
    return true;
}

bool Block::isValidMerkleRoot() const {
    std::string computedRoot = MerkleTree::buildMerkleRoot(transactions);
    return header.merkleRoot == computedRoot;
}

bool Block::meetsTarget() const {
    return Difficulty::meetsTarget(hash, header.bits);
}

void Block::mine(uint32_t maxIterations) {
    Utils::logInfo("Starting mining with target bits: " + std::to_string(header.bits));
    
    uint32_t iterations = 0;
    header.nonce = 0;
    
    while (iterations < maxIterations) {
        // Update hash with current nonce
        computeHash();
        
        // Check if we meet the target
        if (meetsTarget()) {
            Utils::logInfo("Block mined! Nonce: " + std::to_string(header.nonce) + 
                          ", Hash: " + hash);
            return;
        }
        
        // Increment nonce and try again
        header.nonce++;
        iterations++;
        
        // Log progress periodically
        if (iterations % 100000 == 0) {
            Utils::logInfo("Mining progress: " + std::to_string(iterations) + 
                          " iterations, current hash: " + hash);
        }
    }
    
    Utils::logWarning("Mining stopped after " + std::to_string(maxIterations) + 
                     " iterations without finding valid nonce");
}

uint64_t Block::getTotalFees() const {
    uint64_t totalFees = 0;
    
    // Skip coinbase transaction (index 0)
    for (size_t i = 1; i < transactions.size(); ++i) {
        totalFees += transactions[i].getFee();
    }
    
    return totalFees;
}

uint64_t Block::getBlockReward() const {
    if (transactions.empty() || !transactions[0].isCoinbase) {
        return 0;
    }
    
    return transactions[0].getTotalOutput();
}

bool Block::operator==(const Block& other) const {
    return header == other.header &&
           transactions == other.transactions &&
           hash == other.hash;
}

} // namespace pragma
