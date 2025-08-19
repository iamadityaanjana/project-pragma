#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include <optional>
#include <set>
#include <cstdint>
#include "transaction.h"

namespace pragma {

/**
 * Represents an unspent transaction output with metadata
 */
struct UTXO {
    TxOut output;           // The actual output (value + scriptPubKey)
    uint32_t height;        // Block height where this UTXO was created
    bool isCoinbase;        // Whether this comes from a coinbase transaction
    uint32_t confirmations; // Number of confirmations (computed dynamically)
    
    UTXO() = default;
    UTXO(const TxOut& out, uint32_t h, bool coinbase)
        : output(out), height(h), isCoinbase(coinbase), confirmations(0) {}
    
    // Serialization
    std::vector<uint8_t> serialize() const;
    static UTXO deserialize(const std::vector<uint8_t>& data);
    
    // Validation
    bool isSpendable(uint32_t currentHeight, uint32_t coinbaseMaturity = 100) const;
    bool isConfirmed(uint32_t minConfirmations = 1) const;
};

/**
 * UTXO Set - manages all unspent transaction outputs
 */
class UTXOSet {
private:
    std::unordered_map<std::string, UTXO> utxos; // outpoint_string -> UTXO
    uint32_t currentHeight;
    uint64_t totalValue;
    size_t totalOutputs;
    
    // Helper functions
    void updateConfirmations();
    
public:
    UTXOSet();
    ~UTXOSet() = default;
    
    // Helper functions
    std::string outPointToString(const OutPoint& outpoint) const;
    OutPoint stringToOutPoint(const std::string& str) const;
    
    // Core UTXO operations
    bool addUTXO(const OutPoint& outpoint, const TxOut& output, uint32_t height, bool isCoinbase = false);
    bool removeUTXO(const OutPoint& outpoint);
    UTXO* getUTXO(const OutPoint& outpoint);
    const UTXO* getUTXO(const OutPoint& outpoint) const;
    bool hasUTXO(const OutPoint& outpoint) const;
    
    // Transaction application
    bool applyTransaction(const Transaction& tx, uint32_t height);
    bool undoTransaction(const Transaction& tx, const std::vector<UTXO>& spentUTXOs);
    
    // Block operations
    bool applyBlock(const std::vector<Transaction>& transactions, uint32_t height);
    bool undoBlock(const std::vector<Transaction>& transactions, 
                   const std::vector<std::vector<UTXO>>& spentUTXOsPerTx);
    
    // Transaction validation
    bool validateTransaction(const Transaction& tx) const;
    uint64_t calculateTransactionFee(const Transaction& tx) const;
    std::vector<UTXO> getSpentUTXOs(const Transaction& tx) const;
    
    // UTXO queries
    std::vector<UTXO> getUTXOsForAddress(const std::string& address) const;
    uint64_t getBalanceForAddress(const std::string& address) const;
    std::vector<OutPoint> getSpendableUTXOs(const std::string& address, uint64_t amount) const;
    
    // Set management
    void setCurrentHeight(uint32_t height);
    uint32_t getCurrentHeight() const;
    size_t size() const;
    uint64_t getTotalValue() const;
    bool isEmpty() const;
    void clear();
    
    // Statistics
    struct UTXOStats {
        size_t totalUTXOs;
        uint64_t totalValue;
        size_t coinbaseUTXOs;
        uint64_t coinbaseValue;
        size_t matureUTXOs;
        uint64_t matureValue;
        double averageUTXOValue;
        uint32_t currentHeight;
    };
    
    UTXOStats getStats() const;
    
    // Persistence
    bool saveToFile(const std::string& filename) const;
    bool loadFromFile(const std::string& filename);
    
    // Iteration
    class Iterator {
    private:
        std::unordered_map<std::string, UTXO>::const_iterator it;
        const UTXOSet* utxoSet;
        
    public:
        Iterator(std::unordered_map<std::string, UTXO>::const_iterator iter, const UTXOSet* set)
            : it(iter), utxoSet(set) {}
        
        bool operator!=(const Iterator& other) const { return it != other.it; }
        Iterator& operator++() { ++it; return *this; }
        
        std::pair<OutPoint, UTXO> operator*() const {
            return {utxoSet->stringToOutPoint(it->first), it->second};
        }
    };
    
    Iterator begin() const { return Iterator(utxos.begin(), this); }
    Iterator end() const { return Iterator(utxos.end(), this); }
    
    // Debug utilities
    void printUTXOSet() const;
    std::vector<std::string> getUTXOKeys() const;
    bool validateIntegrity() const;
    
    // Coinbase and subsidy
    static uint64_t getBlockSubsidy(uint32_t height);
    static bool isCoinbaseMatured(uint32_t utxoHeight, uint32_t currentHeight, uint32_t maturity = 100);
    
    // Spending policy
    bool canSpendUTXO(const OutPoint& outpoint, uint32_t currentHeight) const;
    std::vector<OutPoint> selectUTXOsForAmount(const std::string& address, uint64_t targetAmount) const;
};

/**
 * UTXO Cache - for performance optimization
 */
class UTXOCache {
private:
    UTXOSet* baseSet;
    std::unordered_map<std::string, UTXO*> cache; // cached changes (nullptr = removed)
    std::set<std::string> modified; // tracks what's been changed
    
public:
    UTXOCache(UTXOSet* base);
    ~UTXOCache();
    
    // Cache operations
    bool addUTXO(const OutPoint& outpoint, const TxOut& output, uint32_t height, bool isCoinbase = false);
    bool removeUTXO(const OutPoint& outpoint);
    const UTXO* getUTXO(const OutPoint& outpoint) const;
    bool hasUTXO(const OutPoint& outpoint) const;
    
    // Cache management
    void flush(); // Apply all cached changes to base set
    void clear(); // Clear all cached changes
    bool hasChanges() const;
    size_t getChangeCount() const;
    
    // Transaction operations (cached)
    bool validateTransaction(const Transaction& tx) const;
    bool applyTransaction(const Transaction& tx, uint32_t height);
};

} // namespace pragma
