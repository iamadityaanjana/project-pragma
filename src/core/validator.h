#pragma once

#include "block.h"
#include "transaction.h"
#include "utxo.h"
#include "chainstate.h"
#include <vector>
#include <string>
#include <unordered_set>

namespace pragma {

/**
 * Validation error codes for detailed error reporting
 */
enum class ValidationError {
    NONE = 0,
    
    // Block validation errors
    INVALID_BLOCK_HASH,
    INVALID_PROOF_OF_WORK,
    INVALID_PREVIOUS_HASH,
    INVALID_TIMESTAMP,
    INVALID_MERKLE_ROOT,
    INVALID_BLOCK_SIZE,
    DUPLICATE_TRANSACTION,
    
    // Transaction validation errors
    INVALID_TRANSACTION_STRUCTURE,
    MISSING_INPUTS,
    INVALID_SIGNATURES,
    DOUBLE_SPEND,
    INSUFFICIENT_FUNDS,
    INVALID_COINBASE,
    IMMATURE_COINBASE_SPEND,
    NEGATIVE_OUTPUT_VALUE,
    
    // Chain validation errors
    ORPHAN_BLOCK,
    INVALID_DIFFICULTY,
    INVALID_BLOCK_REWARD,
    INVALID_FEES,
    
    // Other errors
    UNKNOWN_ERROR
};

/**
 * Validation result containing success status and detailed error information
 */
struct ValidationResult {
    bool isValid;
    ValidationError error;
    std::string errorMessage;
    uint32_t errorHeight;  // Block height where error occurred
    std::string errorTxid; // Transaction ID that caused error (if applicable)
    
    ValidationResult() : isValid(true), error(ValidationError::NONE), errorHeight(0) {}
    
    ValidationResult(ValidationError err, const std::string& msg, uint32_t height = 0, const std::string& txid = "") 
        : isValid(false), error(err), errorMessage(msg), errorHeight(height), errorTxid(txid) {}
    
    static ValidationResult success() {
        return ValidationResult();
    }
    
    static ValidationResult failure(ValidationError err, const std::string& msg, uint32_t height = 0, const std::string& txid = "") {
        return ValidationResult(err, msg, height, txid);
    }
};

/**
 * Comprehensive blockchain validator that enforces all consensus rules
 */
class BlockValidator {
private:
    UTXOSet* utxoSet;
    ChainState* chainState;
    
    // Validation constants
    static const uint32_t COINBASE_MATURITY = 100;
    static const uint32_t MAX_BLOCK_SIZE = 1000000; // 1MB
    static const uint32_t MAX_TIMESTAMP_DRIFT = 7200; // 2 hours
    static const uint32_t MEDIAN_TIME_SPAN = 11; // Blocks for median time calculation
    
    // Internal validation methods
    ValidationResult validateBlockStructure(const Block& block) const;
    ValidationResult validateBlockHeader(const BlockHeader& header, uint32_t height) const;
    ValidationResult validateProofOfWork(const BlockHeader& header) const;
    ValidationResult validateTimestamp(const BlockHeader& header, uint32_t height) const;
    ValidationResult validateMerkleRoot(const Block& block) const;
    ValidationResult validateTransactions(const Block& block, uint32_t height) const;
    ValidationResult validateTransaction(const Transaction& tx, uint32_t height, bool isCoinbase = false) const;
    ValidationResult validateCoinbaseTransaction(const Transaction& tx, uint32_t height, uint64_t expectedReward) const;
    ValidationResult validateNonCoinbaseTransaction(const Transaction& tx, uint32_t height) const;
    ValidationResult validateTransactionInputs(const Transaction& tx, uint32_t height) const;
    ValidationResult validateTransactionOutputs(const Transaction& tx) const;
    ValidationResult validateBlockReward(const Block& block, uint32_t height) const;
    ValidationResult checkDoubleSpends(const Block& block) const;
    
    // Helper methods
    uint64_t calculateBlockSubsidy(uint32_t height) const;
    uint64_t calculateTotalFees(const Block& block, uint32_t height) const;
    uint64_t getMedianTimestamp(uint32_t height) const;
    bool isTimestampValid(uint64_t timestamp, uint32_t height) const;
    
public:
    BlockValidator(UTXOSet* utxos, ChainState* chain);
    ~BlockValidator() = default;
    
    // Main validation interface
    ValidationResult validateBlock(const Block& block, uint32_t height) const;
    ValidationResult validateBlockSequence(const std::vector<Block>& blocks, uint32_t startHeight) const;
    
    // Individual component validation (for testing)
    ValidationResult validateBlockOnly(const Block& block, uint32_t height) const;
    ValidationResult validateTransactionOnly(const Transaction& tx, uint32_t height) const;
    
    // Contextual validation requiring chain state
    ValidationResult validateBlockContext(const Block& block, uint32_t height) const;
    
    // Full validation that updates UTXO set
    ValidationResult validateAndApplyBlock(const Block& block, uint32_t height);
    
    // Utility methods
    std::string getErrorString(ValidationError error) const;
    void printValidationResult(const ValidationResult& result) const;
    
    // Statistics and debugging
    struct ValidationStats {
        uint32_t blocksValidated;
        uint32_t transactionsValidated;
        uint32_t validationErrors;
        uint64_t totalFees;
        uint64_t totalSubsidy;
        
        ValidationStats() : blocksValidated(0), transactionsValidated(0), 
                          validationErrors(0), totalFees(0), totalSubsidy(0) {}
    };
    
    ValidationStats getValidationStats() const;
    void resetValidationStats();
    void printValidationStats() const;
    
private:
    mutable ValidationStats stats;
};

/**
 * Block validation context for complex validation scenarios
 */
struct ValidationContext {
    const Block* block;
    uint32_t height;
    const Block* prevBlock;
    UTXOSet* utxoSnapshot; // UTXO state before this block
    bool checkProofOfWork;
    bool checkTransactions;
    bool checkUTXOConsistency;
    
    ValidationContext(const Block* b, uint32_t h) 
        : block(b), height(h), prevBlock(nullptr), utxoSnapshot(nullptr),
          checkProofOfWork(true), checkTransactions(true), checkUTXOConsistency(true) {}
};

/**
 * Batch validation for multiple blocks (used in chain sync)
 */
class BatchValidator {
private:
    BlockValidator* validator;
    
public:
    BatchValidator(BlockValidator* val) : validator(val) {}
    
    ValidationResult validateBlockBatch(const std::vector<Block>& blocks, uint32_t startHeight);
    ValidationResult validateAndApplyBlockBatch(const std::vector<Block>& blocks, uint32_t startHeight);
    
    // Parallel validation (future enhancement)
    ValidationResult validateBlockBatchParallel(const std::vector<Block>& blocks, uint32_t startHeight);
};

} // namespace pragma
