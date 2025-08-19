#include "merkle.h"
#include "../primitives/hash.h"
#include "../primitives/utils.h"
#include <algorithm>

namespace pragma {

std::string MerkleTree::buildMerkleRoot(const std::vector<std::string>& txids) {
    if (txids.empty()) {
        return ""; // Empty block has no merkle root
    }
    
    if (txids.size() == 1) {
        return txids[0]; // Single transaction = merkle root
    }
    
    return buildMerkleRootInternal(txids);
}

std::string MerkleTree::buildMerkleRoot(const std::vector<Transaction>& transactions) {
    std::vector<std::string> txids;
    txids.reserve(transactions.size());
    
    for (const auto& tx : transactions) {
        txids.push_back(tx.txid);
    }
    
    return buildMerkleRoot(txids);
}

std::string MerkleTree::buildMerkleRootInternal(std::vector<std::string> hashes) {
    while (hashes.size() > 1) {
        std::vector<std::string> nextLevel;
        
        // Process pairs of hashes
        for (size_t i = 0; i < hashes.size(); i += 2) {
            if (i + 1 < hashes.size()) {
                // We have a pair
                nextLevel.push_back(hashPair(hashes[i], hashes[i + 1]));
            } else {
                // Odd number of hashes - duplicate the last one (Bitcoin style)
                nextLevel.push_back(hashPair(hashes[i], hashes[i]));
            }
        }
        
        hashes = std::move(nextLevel);
    }
    
    return hashes[0];
}

std::string MerkleTree::hashPair(const std::string& left, const std::string& right) {
    // Convert hex strings to bytes
    auto leftBytes = Hash::fromHex(left);
    auto rightBytes = Hash::fromHex(right);
    
    // Concatenate the two hashes
    std::vector<uint8_t> combined;
    combined.reserve(leftBytes.size() + rightBytes.size());
    combined.insert(combined.end(), leftBytes.begin(), leftBytes.end());
    combined.insert(combined.end(), rightBytes.begin(), rightBytes.end());
    
    // Return double SHA-256 of the concatenated hashes
    return Hash::dbl_sha256(combined);
}

bool MerkleTree::verifyMerkleProof(const std::string& txid,
                                   const std::string& merkleRoot,
                                   const std::vector<std::string>& proof,
                                   size_t index) {
    if (proof.empty()) {
        // No proof needed if only one transaction
        return txid == merkleRoot;
    }
    
    std::string currentHash = txid;
    size_t currentIndex = index;
    
    // Walk up the merkle tree using the proof
    for (const auto& siblingHash : proof) {
        if (currentIndex % 2 == 0) {
            // We are the left child, sibling is on the right
            currentHash = hashPair(currentHash, siblingHash);
        } else {
            // We are the right child, sibling is on the left
            currentHash = hashPair(siblingHash, currentHash);
        }
        
        // Move up one level
        currentIndex /= 2;
    }
    
    return currentHash == merkleRoot;
}

std::vector<std::string> MerkleTree::generateMerkleProof(const std::vector<std::string>& txids,
                                                          size_t targetIndex) {
    if (targetIndex >= txids.size()) {
        throw std::invalid_argument("Target index out of bounds");
    }
    
    if (txids.size() == 1) {
        return {}; // No proof needed for single transaction
    }
    
    std::vector<std::string> proof;
    std::vector<std::string> currentLevel = txids;
    size_t currentIndex = targetIndex;
    
    while (currentLevel.size() > 1) {
        std::vector<std::string> nextLevel;
        
        // Find sibling at current level
        size_t siblingIndex;
        if (currentIndex % 2 == 0) {
            // We are left child, sibling is right
            siblingIndex = currentIndex + 1;
            if (siblingIndex >= currentLevel.size()) {
                // Odd number of nodes, sibling is ourselves (Bitcoin style)
                siblingIndex = currentIndex;
            }
        } else {
            // We are right child, sibling is left
            siblingIndex = currentIndex - 1;
        }
        
        proof.push_back(currentLevel[siblingIndex]);
        
        // Build next level
        for (size_t i = 0; i < currentLevel.size(); i += 2) {
            if (i + 1 < currentLevel.size()) {
                nextLevel.push_back(hashPair(currentLevel[i], currentLevel[i + 1]));
            } else {
                nextLevel.push_back(hashPair(currentLevel[i], currentLevel[i]));
            }
        }
        
        // Move to next level
        currentLevel = std::move(nextLevel);
        currentIndex /= 2;
    }
    
    return proof;
}

} // namespace pragma
