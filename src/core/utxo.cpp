#include "utxo.h"
#include "../primitives/serialize.h"
#include "../primitives/utils.h"
#include "../primitives/hash.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <sstream>

namespace pragma {

// UTXO Implementation
std::vector<uint8_t> UTXO::serialize() const {
    std::vector<uint8_t> result;
    
    // Serialize TxOut
    auto outputData = output.serialize();
    auto outputSize = Serialize::encodeVarInt(outputData.size());
    result.insert(result.end(), outputSize.begin(), outputSize.end());
    result.insert(result.end(), outputData.begin(), outputData.end());
    
    // Serialize height
    auto heightData = Serialize::encodeUint32LE(height);
    result.insert(result.end(), heightData.begin(), heightData.end());
    
    // Serialize isCoinbase
    result.push_back(isCoinbase ? 1 : 0);
    
    return result;
}

UTXO UTXO::deserialize(const std::vector<uint8_t>& data) {
    size_t offset = 0;
    
    // Deserialize TxOut
    auto [outputSize, sizeBytes] = Serialize::decodeVarInt(std::vector<uint8_t>(data.begin() + offset, data.end()));
    offset += sizeBytes;
    
    std::vector<uint8_t> outputData(data.begin() + offset, data.begin() + offset + outputSize);
    size_t outputOffset = 0;
    TxOut txOut = TxOut::deserialize(outputData, outputOffset);
    offset += outputSize;
    
    // Deserialize height
    uint32_t height = Serialize::decodeUint32LE(std::vector<uint8_t>(data.begin() + offset, data.begin() + offset + 4));
    offset += 4;
    
    // Deserialize isCoinbase
    bool isCoinbase = data[offset] != 0;
    
    return UTXO(txOut, height, isCoinbase);
}

bool UTXO::isSpendable(uint32_t currentHeight, uint32_t coinbaseMaturity) const {
    if (isCoinbase) {
        return (currentHeight >= height + coinbaseMaturity);
    }
    return true;
}

bool UTXO::isConfirmed(uint32_t minConfirmations) const {
    return confirmations >= minConfirmations;
}

// UTXOSet Implementation
UTXOSet::UTXOSet() : currentHeight(0), totalValue(0), totalOutputs(0) {
}

std::string UTXOSet::outPointToString(const OutPoint& outpoint) const {
    return outpoint.txid + ":" + std::to_string(outpoint.index);
}

OutPoint UTXOSet::stringToOutPoint(const std::string& str) const {
    size_t colonPos = str.find(':');
    if (colonPos == std::string::npos) {
        return OutPoint("", 0);
    }
    
    std::string txid = str.substr(0, colonPos);
    uint32_t index = std::stoul(str.substr(colonPos + 1));
    return OutPoint(txid, index);
}

bool UTXOSet::addUTXO(const OutPoint& outpoint, const TxOut& output, uint32_t height, bool isCoinbase) {
    std::string key = outPointToString(outpoint);
    
    if (utxos.find(key) != utxos.end()) {
        Utils::logWarning("UTXO already exists: " + key);
        return false;
    }
    
    UTXO utxo(output, height, isCoinbase);
    utxo.confirmations = (currentHeight >= height) ? (currentHeight - height + 1) : 0;
    
    utxos[key] = utxo;
    totalValue += output.value;
    totalOutputs++;
    
    Utils::logDebug("Added UTXO: " + key + " value: " + std::to_string(output.value));
    return true;
}

bool UTXOSet::removeUTXO(const OutPoint& outpoint) {
    std::string key = outPointToString(outpoint);
    
    auto it = utxos.find(key);
    if (it == utxos.end()) {
        Utils::logWarning("UTXO not found for removal: " + key);
        return false;
    }
    
    totalValue -= it->second.output.value;
    totalOutputs--;
    utxos.erase(it);
    
    Utils::logDebug("Removed UTXO: " + key);
    return true;
}

UTXO* UTXOSet::getUTXO(const OutPoint& outpoint) {
    std::string key = outPointToString(outpoint);
    auto it = utxos.find(key);
    return (it != utxos.end()) ? &it->second : nullptr;
}

const UTXO* UTXOSet::getUTXO(const OutPoint& outpoint) const {
    std::string key = outPointToString(outpoint);
    auto it = utxos.find(key);
    return (it != utxos.end()) ? &it->second : nullptr;
}

bool UTXOSet::hasUTXO(const OutPoint& outpoint) const {
    std::string key = outPointToString(outpoint);
    return utxos.find(key) != utxos.end();
}

bool UTXOSet::applyTransaction(const Transaction& tx, uint32_t height) {
    // First validate the transaction can be applied
    if (!tx.isCoinbase && !validateTransaction(tx)) {
        Utils::logError("Cannot apply invalid transaction: " + tx.txid);
        return false;
    }
    
    // Remove spent UTXOs (for non-coinbase transactions)
    if (!tx.isCoinbase) {
        for (const auto& input : tx.vin) {
            if (!removeUTXO(input.prevout)) {
                Utils::logError("Failed to remove spent UTXO in transaction: " + tx.txid);
                return false;
            }
        }
    }
    
    // Add new UTXOs from outputs
    for (size_t i = 0; i < tx.vout.size(); ++i) {
        OutPoint newOutpoint(tx.txid, static_cast<uint32_t>(i));
        if (!addUTXO(newOutpoint, tx.vout[i], height, tx.isCoinbase)) {
            Utils::logError("Failed to add new UTXO in transaction: " + tx.txid);
            return false;
        }
    }
    
    Utils::logInfo("Applied transaction: " + tx.txid);
    return true;
}

bool UTXOSet::undoTransaction(const Transaction& tx, const std::vector<UTXO>& spentUTXOs) {
    // Remove UTXOs created by this transaction
    for (size_t i = 0; i < tx.vout.size(); ++i) {
        OutPoint outpoint(tx.txid, static_cast<uint32_t>(i));
        if (!removeUTXO(outpoint)) {
            Utils::logWarning("UTXO not found when undoing transaction: " + tx.txid);
        }
    }
    
    // Restore spent UTXOs (for non-coinbase transactions)
    if (!tx.isCoinbase && spentUTXOs.size() == tx.vin.size()) {
        for (size_t i = 0; i < tx.vin.size(); ++i) {
            const auto& input = tx.vin[i];
            const auto& utxo = spentUTXOs[i];
            
            if (!addUTXO(input.prevout, utxo.output, utxo.height, utxo.isCoinbase)) {
                Utils::logError("Failed to restore spent UTXO when undoing transaction: " + tx.txid);
                return false;
            }
        }
    }
    
    Utils::logInfo("Undid transaction: " + tx.txid);
    return true;
}

bool UTXOSet::applyBlock(const std::vector<Transaction>& transactions, uint32_t height) {
    setCurrentHeight(height);
    
    for (const auto& tx : transactions) {
        if (!applyTransaction(tx, height)) {
            Utils::logError("Failed to apply transaction in block at height: " + std::to_string(height));
            return false;
        }
    }
    
    updateConfirmations();
    Utils::logInfo("Applied block at height: " + std::to_string(height) + " with " + std::to_string(transactions.size()) + " transactions");
    return true;
}

bool UTXOSet::undoBlock(const std::vector<Transaction>& transactions, 
                        const std::vector<std::vector<UTXO>>& spentUTXOsPerTx) {
    if (transactions.size() != spentUTXOsPerTx.size()) {
        Utils::logError("Mismatch between transactions and spent UTXOs when undoing block");
        return false;
    }
    
    // Undo transactions in reverse order
    for (int i = transactions.size() - 1; i >= 0; --i) {
        if (!undoTransaction(transactions[i], spentUTXOsPerTx[i])) {
            Utils::logError("Failed to undo transaction when undoing block");
            return false;
        }
    }
    
    if (currentHeight > 0) {
        setCurrentHeight(currentHeight - 1);
    }
    
    updateConfirmations();
    Utils::logInfo("Undid block, new height: " + std::to_string(currentHeight));
    return true;
}

bool UTXOSet::validateTransaction(const Transaction& tx) const {
    if (tx.isCoinbase) {
        return true; // Coinbase transactions are validated elsewhere
    }
    
    if (tx.vin.empty() || tx.vout.empty()) {
        Utils::logError("Transaction has no inputs or outputs");
        return false;
    }
    
    uint64_t totalInput = 0;
    uint64_t totalOutput = 0;
    
    // Validate inputs
    for (const auto& input : tx.vin) {
        const UTXO* utxo = getUTXO(input.prevout);
        if (!utxo) {
            Utils::logError("Input references non-existent UTXO: " + outPointToString(input.prevout));
            return false;
        }
        
        if (!utxo->isSpendable(currentHeight)) {
            Utils::logError("Input references unspendable UTXO (coinbase maturity): " + outPointToString(input.prevout));
            return false;
        }
        
        totalInput += utxo->output.value;
    }
    
    // Calculate total output
    for (const auto& output : tx.vout) {
        if (output.value == 0) {
            Utils::logError("Transaction output has zero value");
            return false;
        }
        totalOutput += output.value;
    }
    
    // Check input >= output (allows for fees)
    if (totalInput < totalOutput) {
        Utils::logError("Transaction inputs less than outputs");
        return false;
    }
    
    return true;
}

uint64_t UTXOSet::calculateTransactionFee(const Transaction& tx) const {
    if (tx.isCoinbase) {
        return 0;
    }
    
    uint64_t totalInput = 0;
    uint64_t totalOutput = tx.getTotalOutput();
    
    for (const auto& input : tx.vin) {
        const UTXO* utxo = getUTXO(input.prevout);
        if (utxo) {
            totalInput += utxo->output.value;
        }
    }
    
    return (totalInput >= totalOutput) ? (totalInput - totalOutput) : 0;
}

std::vector<UTXO> UTXOSet::getSpentUTXOs(const Transaction& tx) const {
    std::vector<UTXO> spentUTXOs;
    
    if (tx.isCoinbase) {
        return spentUTXOs; // Coinbase transactions don't spend UTXOs
    }
    
    for (const auto& input : tx.vin) {
        const UTXO* utxo = getUTXO(input.prevout);
        if (utxo) {
            spentUTXOs.push_back(*utxo);
        }
    }
    
    return spentUTXOs;
}

std::vector<UTXO> UTXOSet::getUTXOsForAddress(const std::string& address) const {
    std::vector<UTXO> result;
    
    for (const auto& pair : utxos) {
        if (pair.second.output.pubKeyHash == address) {
            result.push_back(pair.second);
        }
    }
    
    return result;
}

uint64_t UTXOSet::getBalanceForAddress(const std::string& address) const {
    uint64_t balance = 0;
    
    for (const auto& pair : utxos) {
        if (pair.second.output.pubKeyHash == address && 
            pair.second.isSpendable(currentHeight)) {
            balance += pair.second.output.value;
        }
    }
    
    return balance;
}

std::vector<OutPoint> UTXOSet::getSpendableUTXOs(const std::string& address, uint64_t amount) const {
    std::vector<std::pair<OutPoint, uint64_t>> candidates;
    
    // Collect all spendable UTXOs for this address
    for (const auto& pair : utxos) {
        if (pair.second.output.pubKeyHash == address && 
            pair.second.isSpendable(currentHeight)) {
            OutPoint outpoint = stringToOutPoint(pair.first);
            candidates.emplace_back(outpoint, pair.second.output.value);
        }
    }
    
    // Sort by value (smallest first for better change)
    std::sort(candidates.begin(), candidates.end(), 
              [](const auto& a, const auto& b) { return a.second < b.second; });
    
    std::vector<OutPoint> selected;
    uint64_t total = 0;
    
    for (const auto& candidate : candidates) {
        selected.push_back(candidate.first);
        total += candidate.second;
        
        if (total >= amount) {
            break;
        }
    }
    
    return (total >= amount) ? selected : std::vector<OutPoint>();
}

void UTXOSet::setCurrentHeight(uint32_t height) {
    currentHeight = height;
    updateConfirmations();
}

uint32_t UTXOSet::getCurrentHeight() const {
    return currentHeight;
}

size_t UTXOSet::size() const {
    return utxos.size();
}

uint64_t UTXOSet::getTotalValue() const {
    return totalValue;
}

bool UTXOSet::isEmpty() const {
    return utxos.empty();
}

void UTXOSet::clear() {
    utxos.clear();
    totalValue = 0;
    totalOutputs = 0;
    currentHeight = 0;
}

void UTXOSet::updateConfirmations() {
    for (auto& pair : utxos) {
        UTXO& utxo = pair.second;
        utxo.confirmations = (currentHeight >= utxo.height) ? (currentHeight - utxo.height + 1) : 0;
    }
}

UTXOSet::UTXOStats UTXOSet::getStats() const {
    UTXOStats stats = {};
    stats.totalUTXOs = utxos.size();
    stats.totalValue = totalValue;
    stats.currentHeight = currentHeight;
    
    uint64_t coinbaseValue = 0;
    size_t coinbaseCount = 0;
    uint64_t matureValue = 0;
    size_t matureCount = 0;
    
    for (const auto& pair : utxos) {
        const UTXO& utxo = pair.second;
        
        if (utxo.isCoinbase) {
            coinbaseValue += utxo.output.value;
            coinbaseCount++;
        }
        
        if (utxo.isSpendable(currentHeight)) {
            matureValue += utxo.output.value;
            matureCount++;
        }
    }
    
    stats.coinbaseUTXOs = coinbaseCount;
    stats.coinbaseValue = coinbaseValue;
    stats.matureUTXOs = matureCount;
    stats.matureValue = matureValue;
    stats.averageUTXOValue = stats.totalUTXOs > 0 ? 
        static_cast<double>(stats.totalValue) / stats.totalUTXOs : 0.0;
    
    return stats;
}

bool UTXOSet::saveToFile(const std::string& filename) const {
    try {
        std::ofstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            Utils::logError("Cannot open file for writing: " + filename);
            return false;
        }
        
        // Save metadata
        file.write(reinterpret_cast<const char*>(&currentHeight), sizeof(currentHeight));
        file.write(reinterpret_cast<const char*>(&totalValue), sizeof(totalValue));
        file.write(reinterpret_cast<const char*>(&totalOutputs), sizeof(totalOutputs));
        
        // Save UTXOs
        size_t utxoCount = utxos.size();
        file.write(reinterpret_cast<const char*>(&utxoCount), sizeof(utxoCount));
        
        for (const auto& pair : utxos) {
            // Save key (outpoint string)
            size_t keySize = pair.first.size();
            file.write(reinterpret_cast<const char*>(&keySize), sizeof(keySize));
            file.write(pair.first.c_str(), keySize);
            
            // Save UTXO
            auto utxoData = pair.second.serialize();
            size_t utxoSize = utxoData.size();
            file.write(reinterpret_cast<const char*>(&utxoSize), sizeof(utxoSize));
            file.write(reinterpret_cast<const char*>(utxoData.data()), utxoSize);
        }
        
        file.close();
        Utils::logInfo("UTXO set saved to: " + filename);
        return true;
        
    } catch (const std::exception& e) {
        Utils::logError("Error saving UTXO set: " + std::string(e.what()));
        return false;
    }
}

bool UTXOSet::loadFromFile(const std::string& filename) {
    try {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            Utils::logWarning("Cannot open file for reading: " + filename);
            return false;
        }
        
        // Clear current state
        clear();
        
        // Load metadata
        file.read(reinterpret_cast<char*>(&currentHeight), sizeof(currentHeight));
        file.read(reinterpret_cast<char*>(&totalValue), sizeof(totalValue));
        file.read(reinterpret_cast<char*>(&totalOutputs), sizeof(totalOutputs));
        
        // Load UTXOs
        size_t utxoCount;
        file.read(reinterpret_cast<char*>(&utxoCount), sizeof(utxoCount));
        
        for (size_t i = 0; i < utxoCount; ++i) {
            // Load key
            size_t keySize;
            file.read(reinterpret_cast<char*>(&keySize), sizeof(keySize));
            
            std::string key(keySize, '\0');
            file.read(&key[0], keySize);
            
            // Load UTXO
            size_t utxoSize;
            file.read(reinterpret_cast<char*>(&utxoSize), sizeof(utxoSize));
            
            std::vector<uint8_t> utxoData(utxoSize);
            file.read(reinterpret_cast<char*>(utxoData.data()), utxoSize);
            
            UTXO utxo = UTXO::deserialize(utxoData);
            utxos[key] = utxo;
        }
        
        file.close();
        updateConfirmations();
        
        Utils::logInfo("UTXO set loaded from: " + filename);
        Utils::logInfo("Loaded " + std::to_string(utxoCount) + " UTXOs");
        return true;
        
    } catch (const std::exception& e) {
        Utils::logError("Error loading UTXO set: " + std::string(e.what()));
        return false;
    }
}

void UTXOSet::printUTXOSet() const {
    std::cout << "\n=== UTXO Set ===\n";
    std::cout << "Current height: " << currentHeight << std::endl;
    std::cout << "Total UTXOs: " << utxos.size() << std::endl;
    std::cout << "Total value: " << totalValue << " satoshis" << std::endl;
    
    auto stats = getStats();
    std::cout << "Coinbase UTXOs: " << stats.coinbaseUTXOs << " (" << stats.coinbaseValue << " satoshis)" << std::endl;
    std::cout << "Mature UTXOs: " << stats.matureUTXOs << " (" << stats.matureValue << " satoshis)" << std::endl;
    std::cout << "Average UTXO value: " << stats.averageUTXOValue << " satoshis" << std::endl;
    
    if (utxos.size() <= 10) { // Only print details for small sets
        for (const auto& pair : utxos) {
            const UTXO& utxo = pair.second;
            std::cout << "  " << pair.first 
                      << ": " << utxo.output.value << " sat"
                      << " -> " << utxo.output.pubKeyHash
                      << " (height: " << utxo.height
                      << ", coinbase: " << (utxo.isCoinbase ? "yes" : "no")
                      << ", confirmations: " << utxo.confirmations << ")"
                      << std::endl;
        }
    }
}

std::vector<std::string> UTXOSet::getUTXOKeys() const {
    std::vector<std::string> keys;
    for (const auto& pair : utxos) {
        keys.push_back(pair.first);
    }
    return keys;
}

bool UTXOSet::validateIntegrity() const {
    uint64_t calculatedValue = 0;
    size_t calculatedCount = 0;
    
    for (const auto& pair : utxos) {
        calculatedValue += pair.second.output.value;
        calculatedCount++;
    }
    
    return (calculatedValue == totalValue) && (calculatedCount == totalOutputs);
}

uint64_t UTXOSet::getBlockSubsidy(uint32_t height) {
    uint64_t baseSubsidy = 5000000000ULL; // 50 coins in satoshis
    uint32_t halvings = height / 210000;   // Halve every 210,000 blocks
    
    if (halvings >= 64) {
        return 0; // Prevent overflow
    }
    
    return baseSubsidy >> halvings;
}

bool UTXOSet::isCoinbaseMatured(uint32_t utxoHeight, uint32_t currentHeight, uint32_t maturity) {
    return currentHeight >= utxoHeight + maturity;
}

bool UTXOSet::canSpendUTXO(const OutPoint& outpoint, uint32_t currentHeight) const {
    const UTXO* utxo = getUTXO(outpoint);
    if (!utxo) {
        return false;
    }
    
    return utxo->isSpendable(currentHeight);
}

std::vector<OutPoint> UTXOSet::selectUTXOsForAmount(const std::string& address, uint64_t targetAmount) const {
    return getSpendableUTXOs(address, targetAmount);
}

// UTXOCache Implementation
UTXOCache::UTXOCache(UTXOSet* base) : baseSet(base) {
}

UTXOCache::~UTXOCache() {
    // Clean up allocated UTXOs in cache
    for (auto& pair : cache) {
        delete pair.second;
    }
}

bool UTXOCache::addUTXO(const OutPoint& outpoint, const TxOut& output, uint32_t height, bool isCoinbase) {
    std::string key = baseSet->outPointToString(outpoint);
    
    // Clean up any existing cached UTXO
    auto it = cache.find(key);
    if (it != cache.end()) {
        delete it->second;
    }
    
    cache[key] = new UTXO(output, height, isCoinbase);
    modified.insert(key);
    return true;
}

bool UTXOCache::removeUTXO(const OutPoint& outpoint) {
    std::string key = baseSet->outPointToString(outpoint);
    
    // Clean up any existing cached UTXO
    auto it = cache.find(key);
    if (it != cache.end()) {
        delete it->second;
    }
    
    cache[key] = nullptr; // nullptr indicates removal
    modified.insert(key);
    return true;
}

const UTXO* UTXOCache::getUTXO(const OutPoint& outpoint) const {
    std::string key = baseSet->outPointToString(outpoint);
    
    auto it = cache.find(key);
    if (it != cache.end()) {
        return it->second; // May be nullptr if removed
    }
    
    return baseSet->getUTXO(outpoint);
}

bool UTXOCache::hasUTXO(const OutPoint& outpoint) const {
    const UTXO* utxo = getUTXO(outpoint);
    return utxo != nullptr;
}

void UTXOCache::flush() {
    for (const std::string& key : modified) {
        OutPoint outpoint = baseSet->stringToOutPoint(key);
        auto it = cache.find(key);
        
        if (it != cache.end()) {
            if (it->second == nullptr) {
                // Remove from base set
                baseSet->removeUTXO(outpoint);
            } else {
                // Add to base set
                baseSet->addUTXO(outpoint, it->second->output, it->second->height, it->second->isCoinbase);
            }
        }
    }
    
    clear();
}

void UTXOCache::clear() {
    for (auto& pair : cache) {
        delete pair.second;
    }
    cache.clear();
    modified.clear();
}

bool UTXOCache::hasChanges() const {
    return !modified.empty();
}

size_t UTXOCache::getChangeCount() const {
    return modified.size();
}

bool UTXOCache::validateTransaction(const Transaction& tx) const {
    if (tx.isCoinbase) {
        return true;
    }
    
    uint64_t totalInput = 0;
    uint64_t totalOutput = tx.getTotalOutput();
    
    for (const auto& input : tx.vin) {
        const UTXO* utxo = getUTXO(input.prevout);
        if (!utxo) {
            return false;
        }
        
        if (!utxo->isSpendable(baseSet->getCurrentHeight())) {
            return false;
        }
        
        totalInput += utxo->output.value;
    }
    
    return totalInput >= totalOutput;
}

bool UTXOCache::applyTransaction(const Transaction& tx, uint32_t height) {
    if (!tx.isCoinbase && !validateTransaction(tx)) {
        return false;
    }
    
    // Remove spent UTXOs
    if (!tx.isCoinbase) {
        for (const auto& input : tx.vin) {
            if (!removeUTXO(input.prevout)) {
                return false;
            }
        }
    }
    
    // Add new UTXOs
    for (size_t i = 0; i < tx.vout.size(); ++i) {
        OutPoint newOutpoint(tx.txid, static_cast<uint32_t>(i));
        if (!addUTXO(newOutpoint, tx.vout[i], height, tx.isCoinbase)) {
            return false;
        }
    }
    
    return true;
}

} // namespace pragma
