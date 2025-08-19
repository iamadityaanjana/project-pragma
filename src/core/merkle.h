#pragma once

#include <string>
#include <vector>
#include "transaction.h"

namespace pragma {

/**
 * Merkle tree implementation for transaction hashing
 */
class MerkleTree {
public:
    /**
     * Build merkle root from a list of transaction IDs
     * @param txids Vector of transaction IDs (hex strings)
     * @return Merkle root hash (hex string)
     */
    static std::string buildMerkleRoot(const std::vector<std::string>& txids);
    
    /**
     * Build merkle root from a list of transactions
     * @param transactions Vector of transactions
     * @return Merkle root hash (hex string)
     */
    static std::string buildMerkleRoot(const std::vector<Transaction>& transactions);
    
    /**
     * Verify a merkle proof for a transaction
     * @param txid Transaction ID to verify
     * @param merkleRoot Expected merkle root
     * @param proof Vector of sibling hashes for the merkle path
     * @param index Index of the transaction in the block
     * @return True if proof is valid
     */
    static bool verifyMerkleProof(const std::string& txid, 
                                  const std::string& merkleRoot,
                                  const std::vector<std::string>& proof,
                                  size_t index);
    
    /**
     * Generate merkle proof for a transaction
     * @param txids Vector of all transaction IDs in the block
     * @param targetIndex Index of the transaction to prove
     * @return Vector of sibling hashes forming the merkle path
     */
    static std::vector<std::string> generateMerkleProof(const std::vector<std::string>& txids,
                                                         size_t targetIndex);

private:
    /**
     * Hash two values together (used for merkle tree construction)
     * @param left Left hash
     * @param right Right hash
     * @return Combined hash
     */
    static std::string hashPair(const std::string& left, const std::string& right);
    
    /**
     * Build merkle tree level by level and return root
     * @param hashes Initial level of hashes
     * @return Merkle root hash
     */
    static std::string buildMerkleRootInternal(std::vector<std::string> hashes);
};

} // namespace pragma
