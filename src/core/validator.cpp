#include "validator.h"
#include "../primitives/utils.h"
#include "../primitives/hash.h"
#include "merkle.h"
#include "difficulty.h"
#include <unordered_set>
#include <algorithm>
#include <ctime>
#include <iostream>

namespace pragma {

BlockValidator::BlockValidator(UTXOSet* utxos, ChainState* chain) 
    : utxoSet(utxos), chainState(chain) {
    if (!utxoSet || !chainState) {
        Utils::logError("BlockValidator: Invalid UTXO set or chain state provided");
    }
}

ValidationResult BlockValidator::validateBlock(const Block& block, uint32_t height) const {
    Utils::logInfo("Validating block at height " + std::to_string(height) + ": " + block.hash);
    
    // Step 1: Basic block structure validation
    auto result = validateBlockStructure(block);
    if (!result.isValid) {
        stats.validationErrors++;
        return result;
    }
    
    // Step 2: Block header validation
    result = validateBlockHeader(block.header, height);
    if (!result.isValid) {
        stats.validationErrors++;
        return result;
    }
    
    // Step 3: Proof of work validation
    result = validateProofOfWork(block.header);
    if (!result.isValid) {
        stats.validationErrors++;
        return result;
    }
    
    // Step 4: Timestamp validation
    result = validateTimestamp(block.header, height);
    if (!result.isValid) {
        stats.validationErrors++;
        return result;
    }
    
    // Step 5: Merkle root validation
    result = validateMerkleRoot(block);
    if (!result.isValid) {
        stats.validationErrors++;
        return result;
    }
    
    // Step 6: Transaction validation
    result = validateTransactions(block, height);
    if (!result.isValid) {
        stats.validationErrors++;
        return result;
    }
    
    // Step 7: Block reward validation
    result = validateBlockReward(block, height);
    if (!result.isValid) {
        stats.validationErrors++;
        return result;
    }
    
    // Step 8: Check for double spends within block
    result = checkDoubleSpends(block);
    if (!result.isValid) {
        stats.validationErrors++;
        return result;
    }
    
    stats.blocksValidated++;
    Utils::logInfo("Block validation successful: " + block.hash);
    return ValidationResult::success();
}

ValidationResult BlockValidator::validateTransactionOnly(const Transaction& tx, uint32_t height) const {
    return validateTransaction(tx, height, tx.isCoinbase);
}

ValidationResult BlockValidator::validateBlockStructure(const Block& block) const {
    // Check block size
    size_t blockSize = block.calculateSize();
    if (blockSize > MAX_BLOCK_SIZE) {
        return ValidationResult::failure(
            ValidationError::INVALID_BLOCK_SIZE,
            "Block size " + std::to_string(blockSize) + " exceeds maximum " + std::to_string(MAX_BLOCK_SIZE)
        );
    }
    
    // Check for transactions
    if (block.transactions.empty()) {
        return ValidationResult::failure(
            ValidationError::INVALID_TRANSACTION_STRUCTURE,
            "Block contains no transactions"
        );
    }
    
    // Check for duplicate transactions
    std::unordered_set<std::string> txids;
    for (const auto& tx : block.transactions) {
        if (txids.count(tx.txid)) {
            return ValidationResult::failure(
                ValidationError::DUPLICATE_TRANSACTION,
                "Duplicate transaction in block: " + tx.txid
            );
        }
        txids.insert(tx.txid);
    }
    
    // First transaction must be coinbase
    if (!block.transactions[0].isCoinbase) {
        return ValidationResult::failure(
            ValidationError::INVALID_COINBASE,
            "First transaction is not coinbase"
        );
    }
    
    // Only first transaction can be coinbase
    for (size_t i = 1; i < block.transactions.size(); ++i) {
        if (block.transactions[i].isCoinbase) {
            return ValidationResult::failure(
                ValidationError::INVALID_COINBASE,
                "Multiple coinbase transactions in block"
            );
        }
    }
    
    return ValidationResult::success();
}

ValidationResult BlockValidator::validateBlockHeader(const BlockHeader& header, uint32_t height) const {
    // Validate previous hash linkage (except for genesis)
    if (height > 0) {
        std::string expectedPrevHash = chainState->getBestHash();
        if (header.prevHash != expectedPrevHash) {
            return ValidationResult::failure(
                ValidationError::INVALID_PREVIOUS_HASH,
                "Previous hash mismatch. Expected: " + expectedPrevHash + ", Got: " + header.prevHash,
                height
            );
        }
    } else {
        // Genesis block should have zero previous hash
        std::string zeroPrevHash(64, '0');
        if (header.prevHash != zeroPrevHash) {
            return ValidationResult::failure(
                ValidationError::INVALID_PREVIOUS_HASH,
                "Genesis block must have zero previous hash",
                height
            );
        }
    }
    
    // Validate difficulty target
    uint32_t expectedBits = 0x1d00ffff; // Use default difficulty for now
    if (header.bits != expectedBits) {
        return ValidationResult::failure(
            ValidationError::INVALID_DIFFICULTY,
            "Invalid difficulty bits. Expected: " + std::to_string(expectedBits) + 
            ", Got: " + std::to_string(header.bits),
            height
        );
    }
    
    return ValidationResult::success();
}

ValidationResult BlockValidator::validateProofOfWork(const BlockHeader& header) const {
    // Check if block hash meets difficulty target
    std::string blockHash = header.computeHash();
    if (!Difficulty::meetsTarget(blockHash, header.bits)) {
        return ValidationResult::failure(
            ValidationError::INVALID_PROOF_OF_WORK,
            "Block hash does not meet difficulty target"
        );
    }
    
    return ValidationResult::success();
}

ValidationResult BlockValidator::validateTimestamp(const BlockHeader& header, uint32_t height) const {
    uint64_t currentTime = static_cast<uint64_t>(std::time(nullptr));
    
    // Block timestamp cannot be too far in the future
    if (header.timestamp > currentTime + MAX_TIMESTAMP_DRIFT) {
        return ValidationResult::failure(
            ValidationError::INVALID_TIMESTAMP,
            "Block timestamp too far in future: " + std::to_string(header.timestamp) +
            ", current: " + std::to_string(currentTime),
            height
        );
    }
    
    // Block timestamp must be greater than median of last 11 blocks
    if (height >= MEDIAN_TIME_SPAN) {
        uint64_t medianTime = getMedianTimestamp(height);
        if (header.timestamp <= medianTime) {
            return ValidationResult::failure(
                ValidationError::INVALID_TIMESTAMP,
                "Block timestamp " + std::to_string(header.timestamp) +
                " not greater than median time " + std::to_string(medianTime),
                height
            );
        }
    }
    
    return ValidationResult::success();
}

ValidationResult BlockValidator::validateMerkleRoot(const Block& block) const {
    // Extract transaction IDs
    std::vector<std::string> txids;
    for (const auto& tx : block.transactions) {
        txids.push_back(tx.txid);
    }
    
    // Calculate merkle root
    std::string calculatedRoot = MerkleTree::buildMerkleRoot(txids);
    
    if (block.header.merkleRoot != calculatedRoot) {
        return ValidationResult::failure(
            ValidationError::INVALID_MERKLE_ROOT,
            "Merkle root mismatch. Expected: " + calculatedRoot + 
            ", Got: " + block.header.merkleRoot
        );
    }
    
    return ValidationResult::success();
}

ValidationResult BlockValidator::validateTransactions(const Block& block, uint32_t height) const {
    // Validate coinbase transaction
    auto result = validateCoinbaseTransaction(block.transactions[0], height, 
                                           calculateBlockSubsidy(height) + calculateTotalFees(block, height));
    if (!result.isValid) {
        return result;
    }
    
    // Validate all non-coinbase transactions
    for (size_t i = 1; i < block.transactions.size(); ++i) {
        result = validateNonCoinbaseTransaction(block.transactions[i], height);
        if (!result.isValid) {
            return result;
        }
        stats.transactionsValidated++;
    }
    
    return ValidationResult::success();
}

ValidationResult BlockValidator::validateCoinbaseTransaction(const Transaction& tx, uint32_t height, uint64_t expectedReward) const {
    if (!tx.isCoinbase) {
        return ValidationResult::failure(
            ValidationError::INVALID_COINBASE,
            "Transaction marked as non-coinbase",
            height, tx.txid
        );
    }
    
    // Coinbase should have no inputs (or one dummy input)
    if (!tx.vin.empty()) {
        // Some implementations have a dummy input for coinbase
        if (tx.vin.size() != 1 || !tx.vin[0].prevout.txid.empty()) {
            return ValidationResult::failure(
                ValidationError::INVALID_COINBASE,
                "Coinbase transaction has invalid inputs",
                height, tx.txid
            );
        }
    }
    
    // Check coinbase reward
    uint64_t totalOutput = tx.getTotalOutput();
    if (totalOutput > expectedReward) {
        return ValidationResult::failure(
            ValidationError::INVALID_BLOCK_REWARD,
            "Coinbase output " + std::to_string(totalOutput) + 
            " exceeds expected reward " + std::to_string(expectedReward),
            height, tx.txid
        );
    }
    
    stats.totalSubsidy += calculateBlockSubsidy(height);
    return ValidationResult::success();
}

ValidationResult BlockValidator::validateNonCoinbaseTransaction(const Transaction& tx, uint32_t height) const {
    if (tx.isCoinbase) {
        return ValidationResult::failure(
            ValidationError::INVALID_COINBASE,
            "Non-first transaction marked as coinbase",
            height, tx.txid
        );
    }
    
    // Validate transaction structure
    auto result = validateTransaction(tx, height, false);
    if (!result.isValid) {
        return result;
    }
    
    // Validate inputs exist and are spendable
    result = validateTransactionInputs(tx, height);
    if (!result.isValid) {
        return result;
    }
    
    // Validate outputs
    result = validateTransactionOutputs(tx);
    if (!result.isValid) {
        return result;
    }
    
    // Calculate and validate fee
    uint64_t fee = utxoSet->calculateTransactionFee(tx);
    stats.totalFees += fee;
    
    return ValidationResult::success();
}

ValidationResult BlockValidator::validateTransaction(const Transaction& tx, uint32_t height, bool isCoinbase) const {
    // Basic structure checks
    if (tx.txid.empty()) {
        return ValidationResult::failure(
            ValidationError::INVALID_TRANSACTION_STRUCTURE,
            "Transaction has empty txid",
            height, tx.txid
        );
    }
    
    if (!isCoinbase) {
        if (tx.vin.empty()) {
            return ValidationResult::failure(
                ValidationError::INVALID_TRANSACTION_STRUCTURE,
                "Non-coinbase transaction has no inputs",
                height, tx.txid
            );
        }
    }
    
    if (tx.vout.empty()) {
        return ValidationResult::failure(
            ValidationError::INVALID_TRANSACTION_STRUCTURE,
            "Transaction has no outputs",
            height, tx.txid
        );
    }
    
    return ValidationResult::success();
}

ValidationResult BlockValidator::validateTransactionInputs(const Transaction& tx, uint32_t height) const {
    for (const auto& input : tx.vin) {
        // Check if UTXO exists
        const UTXO* utxo = utxoSet->getUTXO(input.prevout);
        if (!utxo) {
            return ValidationResult::failure(
                ValidationError::MISSING_INPUTS,
                "Input UTXO not found: " + input.prevout.txid + ":" + std::to_string(input.prevout.index),
                height, tx.txid
            );
        }
        
        // Check coinbase maturity
        if (utxo->isCoinbase && !utxo->isSpendable(height)) {
            return ValidationResult::failure(
                ValidationError::IMMATURE_COINBASE_SPEND,
                "Attempting to spend immature coinbase: " + input.prevout.txid + ":" + std::to_string(input.prevout.index) +
                " (maturity: " + std::to_string(utxo->height + COINBASE_MATURITY) + ", current: " + std::to_string(height) + ")",
                height, tx.txid
            );
        }
        
        // TODO: Signature verification would go here
        // For now, we assume inputs are valid if UTXO exists and is spendable
    }
    
    return ValidationResult::success();
}

ValidationResult BlockValidator::validateTransactionOutputs(const Transaction& tx) const {
    for (const auto& output : tx.vout) {
        // Check for negative or zero output values
        if (output.value == 0) {
            return ValidationResult::failure(
                ValidationError::NEGATIVE_OUTPUT_VALUE,
                "Transaction output has zero value",
                0, tx.txid
            );
        }
        
        // Check for valid address format
        if (output.pubKeyHash.empty()) {
            return ValidationResult::failure(
                ValidationError::INVALID_TRANSACTION_STRUCTURE,
                "Transaction output has empty address",
                0, tx.txid
            );
        }
    }
    
    return ValidationResult::success();
}

ValidationResult BlockValidator::validateBlockReward(const Block& block, uint32_t height) const {
    uint64_t expectedSubsidy = calculateBlockSubsidy(height);
    uint64_t totalFees = calculateTotalFees(block, height);
    uint64_t maxReward = expectedSubsidy + totalFees;
    
    uint64_t coinbaseOutput = block.transactions[0].getTotalOutput();
    
    if (coinbaseOutput > maxReward) {
        return ValidationResult::failure(
            ValidationError::INVALID_BLOCK_REWARD,
            "Coinbase output " + std::to_string(coinbaseOutput) + 
            " exceeds maximum reward " + std::to_string(maxReward) +
            " (subsidy: " + std::to_string(expectedSubsidy) + ", fees: " + std::to_string(totalFees) + ")",
            height
        );
    }
    
    return ValidationResult::success();
}

ValidationResult BlockValidator::checkDoubleSpends(const Block& block) const {
    std::unordered_set<std::string> spentOutputs;
    
    for (size_t i = 1; i < block.transactions.size(); ++i) { // Skip coinbase
        const auto& tx = block.transactions[i];
        
        for (const auto& input : tx.vin) {
            std::string outpointKey = input.prevout.txid + ":" + std::to_string(input.prevout.index);
            
            if (spentOutputs.count(outpointKey)) {
                return ValidationResult::failure(
                    ValidationError::DOUBLE_SPEND,
                    "Double spend detected: " + outpointKey,
                    0, tx.txid
                );
            }
            
            spentOutputs.insert(outpointKey);
        }
    }
    
    return ValidationResult::success();
}

ValidationResult BlockValidator::validateAndApplyBlock(const Block& block, uint32_t height) {
    // First validate the block
    auto result = validateBlock(block, height);
    if (!result.isValid) {
        return result;
    }
    
    // Apply all transactions to UTXO set
    for (const auto& tx : block.transactions) {
        if (!utxoSet->applyTransaction(tx, height)) {
            return ValidationResult::failure(
                ValidationError::UNKNOWN_ERROR,
                "Failed to apply transaction to UTXO set: " + tx.txid,
                height, tx.txid
            );
        }
    }
    
    Utils::logInfo("Block applied successfully: " + block.hash);
    return ValidationResult::success();
}

uint64_t BlockValidator::calculateBlockSubsidy(uint32_t height) const {
    uint64_t subsidy = 5000000000ULL; // 50 BTC in satoshis
    uint32_t halvings = height / 210000;
    
    // Prevent overflow by limiting halvings
    if (halvings >= 64) {
        return 0;
    }
    
    return subsidy >> halvings;
}

uint64_t BlockValidator::calculateTotalFees(const Block& block, uint32_t height) const {
    uint64_t totalFees = 0;
    
    // Skip coinbase transaction
    for (size_t i = 1; i < block.transactions.size(); ++i) {
        totalFees += utxoSet->calculateTransactionFee(block.transactions[i]);
    }
    
    return totalFees;
}

uint64_t BlockValidator::getMedianTimestamp(uint32_t height) const {
    std::vector<uint64_t> timestamps;
    
    // Get timestamps of last MEDIAN_TIME_SPAN blocks
    uint32_t startHeight = (height >= MEDIAN_TIME_SPAN) ? height - MEDIAN_TIME_SPAN : 0;
    
    for (uint32_t h = startHeight; h < height; ++h) {
        const Block* block = chainState->getBlockByHeight(h);
        if (block) {
            timestamps.push_back(block->header.timestamp);
        }
    }
    
    if (timestamps.empty()) {
        return 0;
    }
    
    std::sort(timestamps.begin(), timestamps.end());
    return timestamps[timestamps.size() / 2];
}

std::string BlockValidator::getErrorString(ValidationError error) const {
    switch (error) {
        case ValidationError::NONE: return "No error";
        case ValidationError::INVALID_BLOCK_HASH: return "Invalid block hash";
        case ValidationError::INVALID_PROOF_OF_WORK: return "Invalid proof of work";
        case ValidationError::INVALID_PREVIOUS_HASH: return "Invalid previous hash";
        case ValidationError::INVALID_TIMESTAMP: return "Invalid timestamp";
        case ValidationError::INVALID_MERKLE_ROOT: return "Invalid merkle root";
        case ValidationError::INVALID_BLOCK_SIZE: return "Invalid block size";
        case ValidationError::DUPLICATE_TRANSACTION: return "Duplicate transaction";
        case ValidationError::INVALID_TRANSACTION_STRUCTURE: return "Invalid transaction structure";
        case ValidationError::MISSING_INPUTS: return "Missing transaction inputs";
        case ValidationError::INVALID_SIGNATURES: return "Invalid signatures";
        case ValidationError::DOUBLE_SPEND: return "Double spend";
        case ValidationError::INSUFFICIENT_FUNDS: return "Insufficient funds";
        case ValidationError::INVALID_COINBASE: return "Invalid coinbase transaction";
        case ValidationError::IMMATURE_COINBASE_SPEND: return "Immature coinbase spend";
        case ValidationError::NEGATIVE_OUTPUT_VALUE: return "Negative output value";
        case ValidationError::ORPHAN_BLOCK: return "Orphan block";
        case ValidationError::INVALID_DIFFICULTY: return "Invalid difficulty";
        case ValidationError::INVALID_BLOCK_REWARD: return "Invalid block reward";
        case ValidationError::INVALID_FEES: return "Invalid fees";
        case ValidationError::UNKNOWN_ERROR: return "Unknown error";
        default: return "Unrecognized error";
    }
}

void BlockValidator::printValidationResult(const ValidationResult& result) const {
    if (result.isValid) {
        Utils::logInfo("✅ Validation successful");
    } else {
        std::string errorMsg = "❌ Validation failed: " + getErrorString(result.error);
        if (!result.errorMessage.empty()) {
            errorMsg += " - " + result.errorMessage;
        }
        if (result.errorHeight > 0) {
            errorMsg += " (height: " + std::to_string(result.errorHeight) + ")";
        }
        if (!result.errorTxid.empty()) {
            errorMsg += " (txid: " + result.errorTxid + ")";
        }
        Utils::logError(errorMsg);
    }
}

BlockValidator::ValidationStats BlockValidator::getValidationStats() const {
    return stats;
}

void BlockValidator::resetValidationStats() {
    stats = ValidationStats();
}

void BlockValidator::printValidationStats() const {
    std::cout << "\n=== Block Validation Statistics ===" << std::endl;
    std::cout << "Blocks validated: " << stats.blocksValidated << std::endl;
    std::cout << "Transactions validated: " << stats.transactionsValidated << std::endl;
    std::cout << "Validation errors: " << stats.validationErrors << std::endl;
    std::cout << "Total fees collected: " << stats.totalFees << " satoshis" << std::endl;
    std::cout << "Total subsidy issued: " << stats.totalSubsidy << " satoshis" << std::endl;
    std::cout << "===================================" << std::endl;
}

// BatchValidator implementation
ValidationResult BatchValidator::validateBlockBatch(const std::vector<Block>& blocks, uint32_t startHeight) {
    for (size_t i = 0; i < blocks.size(); ++i) {
        auto result = validator->validateBlock(blocks[i], startHeight + i);
        if (!result.isValid) {
            return result;
        }
    }
    return ValidationResult::success();
}

ValidationResult BatchValidator::validateAndApplyBlockBatch(const std::vector<Block>& blocks, uint32_t startHeight) {
    for (size_t i = 0; i < blocks.size(); ++i) {
        auto result = validator->validateAndApplyBlock(blocks[i], startHeight + i);
        if (!result.isValid) {
            return result;
        }
    }
    return ValidationResult::success();
}

} // namespace pragma
