#include "transaction.h"
#include "../primitives/serialize.h"
#include "../primitives/hash.h"
#include "../primitives/utils.h"
#include <algorithm>

namespace pragma {

// TxOut implementation
std::vector<uint8_t> TxOut::serialize() const {
    std::vector<std::vector<uint8_t>> parts;
    
    // Serialize value (8 bytes, little-endian)
    parts.push_back(Serialize::encodeUint64LE(value));
    
    // Serialize pubKeyHash (length-prefixed string)
    parts.push_back(Serialize::encodeString(pubKeyHash));
    
    return Serialize::combine(parts);
}

TxOut TxOut::deserialize(const std::vector<uint8_t>& data, size_t& offset) {
    TxOut txout;
    
    // Deserialize value
    txout.value = Serialize::decodeUint64LE(data, offset);
    offset += 8;
    
    // Deserialize pubKeyHash
    auto [pkHash, pkHashSize] = Serialize::decodeString(data, offset);
    txout.pubKeyHash = pkHash;
    offset += pkHashSize;
    
    return txout;
}

bool TxOut::operator==(const TxOut& other) const {
    return value == other.value && pubKeyHash == other.pubKeyHash;
}

// OutPoint implementation
std::vector<uint8_t> OutPoint::serialize() const {
    std::vector<std::vector<uint8_t>> parts;
    
    // Serialize txid (length-prefixed string)
    parts.push_back(Serialize::encodeString(txid));
    
    // Serialize index (4 bytes, little-endian)
    parts.push_back(Serialize::encodeUint32LE(index));
    
    return Serialize::combine(parts);
}

OutPoint OutPoint::deserialize(const std::vector<uint8_t>& data, size_t& offset) {
    OutPoint outpoint;
    
    // Deserialize txid
    auto [id, idSize] = Serialize::decodeString(data, offset);
    outpoint.txid = id;
    offset += idSize;
    
    // Deserialize index
    outpoint.index = Serialize::decodeUint32LE(data, offset);
    offset += 4;
    
    return outpoint;
}

bool OutPoint::operator==(const OutPoint& other) const {
    return txid == other.txid && index == other.index;
}

bool OutPoint::operator<(const OutPoint& other) const {
    if (txid != other.txid) {
        return txid < other.txid;
    }
    return index < other.index;
}

// TxIn implementation
std::vector<uint8_t> TxIn::serialize() const {
    std::vector<std::vector<uint8_t>> parts;
    
    // Serialize prevout
    parts.push_back(prevout.serialize());
    
    // Serialize signature (length-prefixed string)
    parts.push_back(Serialize::encodeString(sig));
    
    // Serialize pubKey (length-prefixed string)
    parts.push_back(Serialize::encodeString(pubKey));
    
    return Serialize::combine(parts);
}

TxIn TxIn::deserialize(const std::vector<uint8_t>& data, size_t& offset) {
    TxIn txin;
    
    // Deserialize prevout
    txin.prevout = OutPoint::deserialize(data, offset);
    
    // Deserialize signature
    auto [signature, sigSize] = Serialize::decodeString(data, offset);
    txin.sig = signature;
    offset += sigSize;
    
    // Deserialize pubKey
    auto [publicKey, pkSize] = Serialize::decodeString(data, offset);
    txin.pubKey = publicKey;
    offset += pkSize;
    
    return txin;
}

bool TxIn::operator==(const TxIn& other) const {
    return prevout == other.prevout && sig == other.sig && pubKey == other.pubKey;
}

// Transaction implementation
Transaction Transaction::createCoinbase(const std::string& minerAddress, uint64_t reward) {
    Transaction tx;
    tx.isCoinbase = true;
    
    // Coinbase has no inputs (or a special null input)
    // For simplicity, we'll have no inputs for coinbase
    
    // Create output to miner
    TxOut minerOutput(reward, minerAddress);
    tx.vout.push_back(minerOutput);
    
    // Compute transaction ID
    tx.computeTxid();
    
    return tx;
}

Transaction Transaction::create(const std::vector<TxIn>& inputs, const std::vector<TxOut>& outputs) {
    Transaction tx;
    tx.isCoinbase = false;
    tx.vin = inputs;
    tx.vout = outputs;
    
    // Compute transaction ID
    tx.computeTxid();
    
    return tx;
}

std::vector<uint8_t> Transaction::serialize() const {
    std::vector<std::vector<uint8_t>> parts;
    
    // Serialize coinbase flag (1 byte)
    parts.push_back({static_cast<uint8_t>(isCoinbase ? 1 : 0)});
    
    // Serialize number of inputs
    parts.push_back(Serialize::encodeVarInt(vin.size()));
    
    // Serialize each input
    for (const auto& input : vin) {
        parts.push_back(input.serialize());
    }
    
    // Serialize number of outputs
    parts.push_back(Serialize::encodeVarInt(vout.size()));
    
    // Serialize each output
    for (const auto& output : vout) {
        parts.push_back(output.serialize());
    }
    
    return Serialize::combine(parts);
}

Transaction Transaction::deserialize(const std::vector<uint8_t>& data) {
    Transaction tx;
    size_t offset = 0;
    
    // Deserialize coinbase flag
    if (offset >= data.size()) {
        throw std::runtime_error("Transaction deserialize: insufficient data for coinbase flag");
    }
    tx.isCoinbase = (data[offset] == 1);
    offset += 1;
    
    // Deserialize number of inputs
    auto [numInputs, inputCountSize] = Serialize::decodeVarInt(data, offset);
    offset += inputCountSize;
    
    // Deserialize each input
    for (uint64_t i = 0; i < numInputs; ++i) {
        TxIn input = TxIn::deserialize(data, offset);
        tx.vin.push_back(input);
    }
    
    // Deserialize number of outputs
    auto [numOutputs, outputCountSize] = Serialize::decodeVarInt(data, offset);
    offset += outputCountSize;
    
    // Deserialize each output
    for (uint64_t i = 0; i < numOutputs; ++i) {
        TxOut output = TxOut::deserialize(data, offset);
        tx.vout.push_back(output);
    }
    
    // Compute transaction ID
    tx.computeTxid();
    
    return tx;
}

void Transaction::computeTxid() {
    // Serialize transaction data (without the txid field)
    auto serialized = serialize();
    
    // Compute double SHA-256 hash
    txid = Hash::dbl_sha256(serialized);
}

std::string Transaction::computeTxid() const {
    // Serialize transaction data (without the txid field)
    auto serialized = serialize();
    
    // Compute double SHA-256 hash
    return Hash::dbl_sha256(serialized);
}

uint64_t Transaction::getTotalInput() const {
    // Note: This would require UTXO set lookup in a real implementation
    // For now, we'll return 0 as a placeholder
    Utils::logWarning("getTotalInput() requires UTXO set - returning 0");
    return 0;
}

uint64_t Transaction::getTotalOutput() const {
    uint64_t total = 0;
    for (const auto& output : vout) {
        total += output.value;
    }
    return total;
}

uint64_t Transaction::getFee() const {
    if (isCoinbase) {
        return 0; // Coinbase transactions have no fee
    }
    
    // Note: This would require UTXO set lookup in a real implementation
    uint64_t totalIn = getTotalInput();
    uint64_t totalOut = getTotalOutput();
    
    if (totalIn >= totalOut) {
        return totalIn - totalOut;
    }
    
    return 0;
}

bool Transaction::isValid() const {
    // Basic validation checks
    
    // Check for empty outputs
    if (vout.empty()) {
        return false;
    }
    
    // Coinbase transactions should have no inputs
    if (isCoinbase && !vin.empty()) {
        return false;
    }
    
    // Non-coinbase transactions should have inputs
    if (!isCoinbase && vin.empty()) {
        return false;
    }
    
    // Check for valid output values
    for (const auto& output : vout) {
        if (output.value == 0) {
            return false; // No zero-value outputs allowed
        }
        if (output.pubKeyHash.empty()) {
            return false; // Must have a destination
        }
    }
    
    // Check for duplicate inputs (no double spending within transaction)
    if (!isCoinbase) {
        std::vector<OutPoint> outpoints;
        for (const auto& input : vin) {
            outpoints.push_back(input.prevout);
        }
        
        std::sort(outpoints.begin(), outpoints.end());
        auto it = std::unique(outpoints.begin(), outpoints.end());
        if (it != outpoints.end()) {
            return false; // Duplicate inputs found
        }
    }
    
    return true;
}

bool Transaction::operator==(const Transaction& other) const {
    return vin == other.vin && 
           vout == other.vout && 
           isCoinbase == other.isCoinbase &&
           txid == other.txid;
}

} // namespace pragma
