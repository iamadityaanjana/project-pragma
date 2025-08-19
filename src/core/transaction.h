#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace pragma {

/**
 * Transaction output - represents value sent to an address
 */
struct TxOut {
    uint64_t value;         // Amount in satoshis (8 decimal places)
    std::string pubKeyHash; // Public key hash (address)
    
    TxOut() : value(0) {}
    TxOut(uint64_t val, const std::string& pkHash) : value(val), pubKeyHash(pkHash) {}
    
    // Serialization
    std::vector<uint8_t> serialize() const;
    static TxOut deserialize(const std::vector<uint8_t>& data, size_t& offset);
    
    bool operator==(const TxOut& other) const;
};

/**
 * Outpoint - references a specific transaction output
 */
struct OutPoint {
    std::string txid;    // Transaction ID (32-byte hex string)
    uint32_t index;      // Output index within the transaction
    
    OutPoint() : index(0) {}
    OutPoint(const std::string& id, uint32_t idx) : txid(id), index(idx) {}
    
    // Serialization
    std::vector<uint8_t> serialize() const;
    static OutPoint deserialize(const std::vector<uint8_t>& data, size_t& offset);
    
    bool operator==(const OutPoint& other) const;
    bool operator<(const OutPoint& other) const; // For use in maps
};

/**
 * Transaction input - spends a previous output
 */
struct TxIn {
    OutPoint prevout;       // Previous output being spent
    std::string sig;        // DER-encoded ECDSA signature
    std::string pubKey;     // Public key for signature verification
    
    TxIn() = default;
    TxIn(const OutPoint& prev, const std::string& signature, const std::string& publicKey)
        : prevout(prev), sig(signature), pubKey(publicKey) {}
    
    // Serialization
    std::vector<uint8_t> serialize() const;
    static TxIn deserialize(const std::vector<uint8_t>& data, size_t& offset);
    
    bool operator==(const TxIn& other) const;
};

/**
 * Transaction - represents a complete transaction
 */
struct Transaction {
    std::vector<TxIn> vin;   // Transaction inputs
    std::vector<TxOut> vout; // Transaction outputs
    bool isCoinbase;         // True if this is a coinbase transaction
    std::string txid;        // Transaction ID (computed from serialized data)
    
    Transaction() : isCoinbase(false) {}
    
    // Create a coinbase transaction (mining reward)
    static Transaction createCoinbase(const std::string& minerAddress, uint64_t reward);
    
    // Create a regular transaction
    static Transaction create(const std::vector<TxIn>& inputs, const std::vector<TxOut>& outputs);
    
    // Serialization
    std::vector<uint8_t> serialize() const;
    static Transaction deserialize(const std::vector<uint8_t>& data);
    
    // Compute transaction ID from serialized data
    void computeTxid();
    std::string computeTxid() const;
    
    // Get total input value (requires UTXO set lookup)
    uint64_t getTotalInput() const; // Note: This will need UTXO set access
    
    // Get total output value
    uint64_t getTotalOutput() const;
    
    // Get transaction fee
    uint64_t getFee() const; // Note: This will need UTXO set access
    
    // Validation
    bool isValid() const;
    
    bool operator==(const Transaction& other) const;
};

} // namespace pragma
