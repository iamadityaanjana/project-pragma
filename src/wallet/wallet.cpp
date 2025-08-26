#include "wallet.h"
#include "../primitives/serialize.h"
#include "../primitives/utils.h"
#include <random>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <iostream>

namespace pragma {

// PrivateKey implementation
PrivateKey::PrivateKey() : keyData_(32, 0) {}

PrivateKey::PrivateKey(const std::vector<uint8_t>& keyData) : keyData_(keyData) {
    if (keyData_.size() != 32) {
        keyData_.resize(32, 0);
    }
}

PrivateKey PrivateKey::generateRandom() {
    std::vector<uint8_t> keyData(32);
    
    // Use time-based seed instead of random_device to avoid hanging
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    auto seed = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
    std::mt19937 gen(static_cast<uint32_t>(seed));
    std::uniform_int_distribution<uint8_t> dis(0, 255);
    
    for (auto& byte : keyData) {
        byte = dis(gen);
    }
    
    return PrivateKey(keyData);
}

PrivateKey PrivateKey::fromWIF(const std::string& wif) {
    // Simplified WIF decoding (normally would use base58 with checksum)
    if (wif.length() < 64) {
        return PrivateKey();
    }
    
    std::vector<uint8_t> keyData;
    keyData.reserve(32);
    
    for (size_t i = 0; i < 64 && i + 1 < wif.length(); i += 2) {
        std::string byteStr = wif.substr(i, 2);
        uint8_t byte = static_cast<uint8_t>(std::stoul(byteStr, nullptr, 16));
        keyData.push_back(byte);
    }
    
    return PrivateKey(keyData);
}

std::string PrivateKey::toWIF() const {
    std::stringstream ss;
    ss << std::hex;
    for (const auto& byte : keyData_) {
        ss << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    return ss.str();
}

std::vector<uint8_t> PrivateKey::getPublicKey() const {
    // Simplified public key derivation (normally would use elliptic curve cryptography)
    std::vector<uint8_t> pubKey;
    pubKey.reserve(33); // Compressed public key
    
    pubKey.push_back(0x02); // Compressed key prefix
    
    // Simple hash-based derivation for demo purposes
    auto hash = Hash::sha256(keyData_);
    pubKey.insert(pubKey.end(), hash.begin(), hash.begin() + 32);
    
    return pubKey;
}

std::vector<uint8_t> PrivateKey::sign(const std::vector<uint8_t>& data) const {
    // Simplified signing (normally would use ECDSA)
    std::vector<uint8_t> signature;
    signature.reserve(64);
    
    // Create deterministic signature based on private key and data
    auto combined = keyData_;
    combined.insert(combined.end(), data.begin(), data.end());
    auto hash1 = Hash::sha256(combined);
    auto hash2 = Hash::sha256(hash1);
    
    signature.insert(signature.end(), hash1.begin(), hash1.begin() + 32);
    signature.insert(signature.end(), hash2.begin(), hash2.begin() + 32);
    
    return signature;
}

// Address implementation
Address::Address(const std::string& address) : address_(address) {}

Address Address::fromPublicKey(const std::vector<uint8_t>& pubKey) {
    // Simplified address generation (normally would use RIPEMD160(SHA256(pubKey)))
    auto hash = Hash::sha256(pubKey);
    std::stringstream ss;
    ss << "1"; // Version byte for P2PKH
    ss << std::hex;
    for (size_t i = 0; i < 20 && i < hash.size(); ++i) {
        ss << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    return Address(ss.str());
}

Address Address::fromPrivateKey(const PrivateKey& privKey) {
    return fromPublicKey(privKey.getPublicKey());
}

std::vector<uint8_t> Address::getScriptPubKey() const {
    // Simplified P2PKH script: OP_DUP OP_HASH160 <pubKeyHash> OP_EQUALVERIFY OP_CHECKSIG
    std::vector<uint8_t> script;
    script.push_back(0x76); // OP_DUP
    script.push_back(0xa9); // OP_HASH160
    script.push_back(0x14); // Push 20 bytes
    
    // Extract hash from address (skip version byte)
    std::string hashStr = address_.substr(1);
    for (size_t i = 0; i < hashStr.length() && i + 1 < hashStr.length(); i += 2) {
        std::string byteStr = hashStr.substr(i, 2);
        uint8_t byte = static_cast<uint8_t>(std::stoul(byteStr, nullptr, 16));
        script.push_back(byte);
    }
    
    script.push_back(0x88); // OP_EQUALVERIFY
    script.push_back(0xac); // OP_CHECKSIG
    
    return script;
}

bool Address::isValid() const {
    return address_.length() > 25 && address_[0] == '1';
}

// Wallet implementation
Wallet::Wallet(const std::string& walletPath) 
    : walletPath_(walletPath.empty() ? "wallet.dat" : walletPath),
      balance_(0), unconfirmedBalance_(0), immatureBalance_(0),
      encrypted_(false), unlocked_(true), unlockTimeout_(0) {
    initializeWallet();
}

Wallet::~Wallet() {
    if (unlocked_) {
        save();
    }
}

bool Wallet::create(const std::string& passphrase) {
    std::lock_guard<std::mutex> lock(walletMutex_);
    
    // Generate the first address (using internal method to avoid deadlock)
    generateNewAddressInternal("default");
    
    if (!passphrase.empty()) {
        encryptWallet(passphrase);
    }
    
    return saveToFile();
}

bool Wallet::load(const std::string& passphrase) {
    std::lock_guard<std::mutex> lock(walletMutex_);
    
    if (!loadFromFile()) {
        return false;
    }
    
    if (encrypted_ && !passphrase.empty()) {
        return unlock(passphrase);
    }
    
    return true;
}

bool Wallet::save() const {
    std::lock_guard<std::mutex> lock(walletMutex_);
    return saveToFile();
}

Address Wallet::generateNewAddress(const std::string& label) {
    std::lock_guard<std::mutex> lock(walletMutex_);
    return generateNewAddressInternal(label);
}

Address Wallet::generateNewAddressInternal(const std::string& label) {
    auto privateKey = PrivateKey::generateRandom();
    WalletKey walletKey(privateKey, label);
    
    std::string addressStr = walletKey.address.toString();
    keys_[addressStr] = walletKey;
    
    return walletKey.address;
}

Address Wallet::generateChangeAddress() {
    auto addr = generateNewAddress("change");
    keys_[addr.toString()].isChangeAddress = true;
    return addr;
}

std::vector<Address> Wallet::getAddresses() const {
    std::lock_guard<std::mutex> lock(walletMutex_);
    
    std::vector<Address> addresses;
    for (const auto& pair : keys_) {
        if (!pair.second.isChangeAddress) {
            addresses.push_back(pair.second.address);
        }
    }
    return addresses;
}

std::vector<WalletKey> Wallet::getAllKeys() const {
    std::lock_guard<std::mutex> lock(walletMutex_);
    
    std::vector<WalletKey> keys;
    for (const auto& pair : keys_) {
        keys.push_back(pair.second);
    }
    return keys;
}

bool Wallet::importPrivateKey(const PrivateKey& key, const std::string& label) {
    std::lock_guard<std::mutex> lock(walletMutex_);
    
    if (!key.isValid()) {
        return false;
    }
    
    WalletKey walletKey(key, label);
    std::string addressStr = walletKey.address.toString();
    keys_[addressStr] = walletKey;
    
    return true;
}

uint64_t Wallet::getBalance() const {
    std::lock_guard<std::mutex> lock(walletMutex_);
    return balance_;
}

uint64_t Wallet::getUnconfirmedBalance() const {
    std::lock_guard<std::mutex> lock(walletMutex_);
    return unconfirmedBalance_;
}

std::vector<WalletUTXO> Wallet::getUTXOs() const {
    std::lock_guard<std::mutex> lock(walletMutex_);
    return utxos_;
}

std::vector<WalletUTXO> Wallet::getSpendableUTXOs(uint64_t minAmount) const {
    std::lock_guard<std::mutex> lock(walletMutex_);
    
    std::vector<WalletUTXO> spendable;
    for (const auto& utxo : utxos_) {
        if (utxo.spendable && utxo.amount >= minAmount && utxo.confirmations > 0) {
            spendable.push_back(utxo);
        }
    }
    
    // Sort by amount (largest first for efficient coin selection)
    std::sort(spendable.begin(), spendable.end(), 
              [](const WalletUTXO& a, const WalletUTXO& b) {
                  return a.amount > b.amount;
              });
    
    return spendable;
}

Wallet::CoinSelectionResult Wallet::selectCoins(uint64_t targetAmount, uint64_t feeRate) const {
    CoinSelectionResult result;
    
    auto spendableUTXOs = getSpendableUTXOs();
    uint64_t totalSelected = 0;
    
    // Simple greedy coin selection
    for (const auto& utxo : spendableUTXOs) {
        result.selectedCoins.push_back(utxo);
        totalSelected += utxo.amount;
        
        // Estimate fee based on transaction size
        uint64_t estimatedFee = (result.selectedCoins.size() * 148 + 34 + 10) * feeRate / 1000;
        
        if (totalSelected >= targetAmount + estimatedFee) {
            result.totalAmount = totalSelected;
            result.changeAmount = totalSelected - targetAmount - estimatedFee;
            result.success = true;
            break;
        }
    }
    
    return result;
}

std::string Wallet::sendToAddress(const Address& address, uint64_t amount, 
                                 const std::string& comment, uint64_t feeRate) {
    std::vector<std::pair<Address, uint64_t>> recipients = {{address, amount}};
    return sendMany(recipients, comment, feeRate);
}

std::string Wallet::sendMany(const std::vector<std::pair<Address, uint64_t>>& recipients,
                            const std::string& comment, uint64_t feeRate) {
    std::lock_guard<std::mutex> lock(walletMutex_);
    
    // Calculate total amount needed
    uint64_t totalAmount = 0;
    for (const auto& recipient : recipients) {
        totalAmount += recipient.second;
    }
    
    // Select coins
    auto coinSelection = selectCoins(totalAmount, feeRate);
    if (!coinSelection.success) {
        return ""; // Insufficient funds
    }
    
    // Create transaction
    Transaction tx;
    
    // Add inputs
    for (const auto& utxo : coinSelection.selectedCoins) {
        TxIn input;
        input.prevout.txid = utxo.txid;
        input.prevout.index = utxo.vout;
        input.sig = ""; // Will be filled during signing
        input.pubKey = ""; // Will be filled during signing
        tx.vin.push_back(input);
    }
    
    // Add outputs
    for (const auto& recipient : recipients) {
        TxOut output;
        output.value = recipient.second;
        output.pubKeyHash = recipient.first.toString();
        tx.vout.push_back(output);
    }
    
    // Add change output if needed
    if (coinSelection.changeAmount > 0) {
        auto changeAddress = generateChangeAddress();
        TxOut changeOutput;
        changeOutput.value = coinSelection.changeAmount;
        changeOutput.pubKeyHash = changeAddress.toString();
        tx.vout.push_back(changeOutput);
    }
    
    // Sign transaction
    if (!signTransaction(tx)) {
        return ""; // Signing failed
    }
    
    // Store transaction record
    WalletTransaction walletTx;
    walletTx.tx = tx;
    walletTx.txid = tx.txid;
    walletTx.amount = -static_cast<int64_t>(totalAmount);
    walletTx.fee = coinSelection.totalAmount - totalAmount - coinSelection.changeAmount;
    walletTx.timestamp = std::time(nullptr);
    walletTx.comment = comment;
    walletTx.status = WalletTransaction::PENDING;
    
    transactions_[walletTx.txid] = walletTx;
    
    return walletTx.txid;
}

bool Wallet::signTransaction(Transaction& tx) const {
    // Simplified transaction signing
    for (size_t i = 0; i < tx.vin.size(); ++i) {
        auto& input = tx.vin[i];
        
        // Find the private key for this input (simplified)
        // In practice, we'd need to look up the previous transaction output
        // and match it to one of our addresses
        for (const auto& keyPair : keys_) {
            // Create a simple signature
            auto signature = keyPair.second.privateKey.sign({0x01}); // Simplified
            auto pubKey = keyPair.second.privateKey.getPublicKey();
            
            // Convert signature to hex string for storage
            input.sig = Hash::toHex(signature);
            input.pubKey = Hash::toHex(pubKey);
            break; // Use first key for simplicity
        }
    }
    
    return true;
}

std::vector<WalletTransaction> Wallet::getTransactions(int32_t count, int32_t skip) const {
    std::lock_guard<std::mutex> lock(walletMutex_);
    
    std::vector<WalletTransaction> txs;
    for (const auto& pair : transactions_) {
        txs.push_back(pair.second);
    }
    
    // Sort by timestamp (newest first)
    std::sort(txs.begin(), txs.end(), 
              [](const WalletTransaction& a, const WalletTransaction& b) {
                  return a.timestamp > b.timestamp;
              });
    
    // Apply skip and count
    if (skip >= static_cast<int32_t>(txs.size())) {
        return {};
    }
    
    auto start = txs.begin() + skip;
    auto end = (count > 0 && skip + count < static_cast<int32_t>(txs.size())) 
               ? start + count : txs.end();
    
    return std::vector<WalletTransaction>(start, end);
}

Wallet::WalletInfo Wallet::getInfo() const {
    std::lock_guard<std::mutex> lock(walletMutex_);
    
    WalletInfo info;
    info.walletName = walletPath_;
    info.balance = balance_;
    info.unconfirmedBalance = unconfirmedBalance_;
    info.immatureBalance = immatureBalance_;
    info.transactionCount = static_cast<uint32_t>(transactions_.size());
    info.addressCount = static_cast<uint32_t>(keys_.size());
    info.keyCount = static_cast<uint32_t>(keys_.size());
    info.encrypted = encrypted_;
    info.keystoreSize = keys_.size() * 64; // Approximate
    
    return info;
}

bool Wallet::encryptWallet(const std::string& passphrase) {
    std::lock_guard<std::mutex> lock(walletMutex_);
    
    if (encrypted_) {
        return false; // Already encrypted
    }
    
    encryptionKey_ = Hash::sha256(std::vector<uint8_t>(passphrase.begin(), passphrase.end()));
    encrypted_ = true;
    unlocked_ = true; // Keep unlocked after encryption
    
    return saveToFile();
}

bool Wallet::unlock(const std::string& passphrase, uint32_t timeoutSeconds) {
    std::lock_guard<std::mutex> lock(walletMutex_);
    
    if (!encrypted_) {
        return true; // Not encrypted
    }
    
    std::string providedKey = Hash::sha256(std::vector<uint8_t>(passphrase.begin(), passphrase.end()));
    if (providedKey != encryptionKey_) {
        return false; // Wrong passphrase
    }
    
    unlocked_ = true;
    if (timeoutSeconds > 0) {
        unlockTimeout_ = std::time(nullptr) + timeoutSeconds;
    } else {
        unlockTimeout_ = 0; // No timeout
    }
    
    return true;
}

void Wallet::lock() {
    std::lock_guard<std::mutex> lock(walletMutex_);
    unlocked_ = false;
    unlockTimeout_ = 0;
}

void Wallet::initializeWallet() {
    // Initialize with empty state
    balance_ = 0;
    unconfirmedBalance_ = 0;
    immatureBalance_ = 0;
    encrypted_ = false;
    unlocked_ = true;
}

bool Wallet::loadFromFile() {
    std::ifstream file(walletPath_, std::ios::binary);
    if (!file.is_open()) {
        return false; // File doesn't exist, will be created on save
    }
    
    // Simplified wallet file format
    // In practice, this would use a proper serialization format
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        
        if (line.substr(0, 4) == "KEY:") {
            // Parse key entry: KEY:address:privatekey:label:timestamp:ischange
            std::istringstream iss(line.substr(4));
            std::string address, privKeyHex, label, timestampStr, isChangeStr;
            
            if (std::getline(iss, address, ':') &&
                std::getline(iss, privKeyHex, ':') &&
                std::getline(iss, label, ':') &&
                std::getline(iss, timestampStr, ':') &&
                std::getline(iss, isChangeStr)) {
                
                auto privKey = PrivateKey::fromWIF(privKeyHex);
                WalletKey walletKey(privKey, label);
                walletKey.creationTime = std::stoull(timestampStr);
                walletKey.isChangeAddress = (isChangeStr == "1");
                
                keys_[address] = walletKey;
            }
        } else if (line.substr(0, 9) == "ENCRYPTED") {
            encrypted_ = true;
            unlocked_ = false;
        }
    }
    
    return true;
}

bool Wallet::saveToFile() const {
    std::ofstream file(walletPath_, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    // Save encryption status
    if (encrypted_) {
        file << "ENCRYPTED\n";
    }
    
    // Save keys
    for (const auto& pair : keys_) {
        const auto& address = pair.first;
        const auto& key = pair.second;
        
        std::string wif = key.privateKey.toWIF();
        
        file << "KEY:" << address << ":" << wif << ":"
             << key.label << ":" << key.creationTime << ":"
             << (key.isChangeAddress ? "1" : "0") << "\n";
    }
    
    file.close();
    return true;
}

void Wallet::updateBalance() {
    balance_ = 0;
    unconfirmedBalance_ = 0;
    immatureBalance_ = 0;
    
    for (const auto& utxo : utxos_) {
        if (utxo.confirmations > 0) {
            balance_ += utxo.amount;
        } else {
            unconfirmedBalance_ += utxo.amount;
        }
    }
}

// WalletManager implementation
WalletManager& WalletManager::getInstance() {
    static WalletManager instance;
    return instance;
}

std::shared_ptr<Wallet> WalletManager::createWallet(const std::string& name, const std::string& passphrase) {
    std::lock_guard<std::mutex> lock(managerMutex_);
    
    if (wallets_.find(name) != wallets_.end()) {
        return nullptr; // Wallet already exists
    }
    
    auto wallet = std::make_shared<Wallet>(name + ".dat");
    
    if (wallet->create(passphrase)) {
        wallets_[name] = wallet;
        if (defaultWallet_.empty()) {
            defaultWallet_ = name;
        }
        return wallet;
    }
    
    return nullptr;
}

std::shared_ptr<Wallet> WalletManager::loadWallet(const std::string& name, const std::string& passphrase) {
    std::lock_guard<std::mutex> lock(managerMutex_);
    
    auto it = wallets_.find(name);
    if (it != wallets_.end()) {
        return it->second; // Already loaded
    }
    
    auto wallet = std::make_shared<Wallet>(name + ".dat");
    if (wallet->load(passphrase)) {
        wallets_[name] = wallet;
        return wallet;
    }
    
    return nullptr;
}

std::shared_ptr<Wallet> WalletManager::getWallet(const std::string& name) const {
    std::lock_guard<std::mutex> lock(managerMutex_);
    
    std::string walletName = name.empty() ? defaultWallet_ : name;
    auto it = wallets_.find(walletName);
    return (it != wallets_.end()) ? it->second : nullptr;
}

std::vector<std::string> WalletManager::listWallets() const {
    std::lock_guard<std::mutex> lock(managerMutex_);
    
    std::vector<std::string> names;
    for (const auto& pair : wallets_) {
        names.push_back(pair.first);
    }
    return names;
}

// Missing Wallet method implementations
PrivateKey Wallet::getPrivateKey(const Address& address) const {
    if (isLocked()) {
        return PrivateKey(); // Return empty private key if wallet is locked
    }
    
    auto it = keys_.find(address.toString());
    if (it != keys_.end()) {
        return it->second.privateKey;
    }
    return PrivateKey(); // Return empty private key if not found
}

bool Wallet::hasPrivateKey(const Address& address) const {
    return keys_.find(address.toString()) != keys_.end();
}

bool Wallet::backup(const std::string& backupPath) const {
    try {
        std::ofstream file(backupPath, std::ios::binary);
        if (!file.is_open()) {
            return false;
        }
        
        // For now, write the wallet path as placeholder (simplified backup)
        std::string data = walletPath_;
        file.write(data.c_str(), data.size());
        file.close();
        
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

void WalletManager::setDefaultWallet(const std::string& name) {
    std::lock_guard<std::mutex> lock(managerMutex_);
    if (wallets_.find(name) != wallets_.end()) {
        defaultWallet_ = name;
    }
}

} // namespace pragma
