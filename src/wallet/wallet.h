#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <fstream>
#include "../primitives/hash.h"
#include "../core/transaction.h"
#include "../core/utxo.h"

// Forward declaration
namespace pragma {
    class Block;
}

namespace pragma {

/**
 * Represents a private key for cryptographic operations
 */
class PrivateKey {
public:
    PrivateKey();
    explicit PrivateKey(const std::vector<uint8_t>& keyData);
    
    // Generate a random private key
    static PrivateKey generateRandom();
    
    // Import from WIF (Wallet Import Format)
    static PrivateKey fromWIF(const std::string& wif);
    
    // Export to WIF
    std::string toWIF() const;
    
    // Get public key
    std::vector<uint8_t> getPublicKey() const;
    
    // Sign data
    std::vector<uint8_t> sign(const std::vector<uint8_t>& data) const;
    
    // Get the raw key data
    const std::vector<uint8_t>& getData() const { return keyData_; }
    
    bool isValid() const { return keyData_.size() == 32; }

private:
    std::vector<uint8_t> keyData_;
};

/**
 * Represents a Bitcoin-style address
 */
class Address {
public:
    Address() = default;
    explicit Address(const std::string& address);
    
    // Create from public key
    static Address fromPublicKey(const std::vector<uint8_t>& pubKey);
    
    // Create from private key
    static Address fromPrivateKey(const PrivateKey& privKey);
    
    // Get the address string
    const std::string& toString() const { return address_; }
    
    // Get script pubkey for this address
    std::vector<uint8_t> getScriptPubKey() const;
    
    bool isValid() const;
    bool operator==(const Address& other) const { return address_ == other.address_; }

private:
    std::string address_;
};

/**
 * Represents a key-value pair in the wallet
 */
struct WalletKey {
    PrivateKey privateKey;
    Address address;
    std::string label;
    uint64_t creationTime;
    bool isChangeAddress;
    
    WalletKey() : creationTime(0), isChangeAddress(false) {}
    WalletKey(const PrivateKey& priv, const std::string& lbl = "")
        : privateKey(priv), address(Address::fromPrivateKey(priv)), 
          label(lbl), creationTime(std::time(nullptr)), isChangeAddress(false) {}
};

/**
 * Wallet transaction record
 */
struct WalletTransaction {
    Transaction tx;
    std::string txid;
    int64_t amount;           // Net amount (positive = received, negative = sent)
    uint64_t fee;            // Transaction fee (0 if received)
    uint32_t blockHeight;    // 0 if unconfirmed
    std::string blockHash;   // Empty if unconfirmed
    uint64_t timestamp;
    int32_t confirmations;
    std::string comment;
    
    enum Status {
        PENDING,
        CONFIRMED,
        CONFLICTED,
        ABANDONED
    };
    Status status;
    
    WalletTransaction() : amount(0), fee(0), blockHeight(0), timestamp(0), 
                         confirmations(0), status(PENDING) {}
};

/**
 * UTXO record for the wallet
 */
struct WalletUTXO {
    std::string txid;
    uint32_t vout;
    uint64_t amount;
    Address address;
    std::string scriptPubKey;
    uint32_t confirmations;
    bool spendable;
    bool solvable;
    
    WalletUTXO() : vout(0), amount(0), confirmations(0), spendable(true), solvable(true) {}
};

/**
 * Main Wallet class for managing keys, addresses, and transactions
 */
class Wallet {
public:
    explicit Wallet(const std::string& walletPath = "");
    ~Wallet();
    
    // Wallet management
    bool create(const std::string& passphrase = "");
    bool load(const std::string& passphrase = "");
    bool save() const;
    bool backup(const std::string& backupPath) const;
    
    // Key and address management
    Address generateNewAddress(const std::string& label = "");
    Address generateChangeAddress();
    std::vector<Address> getAddresses() const;
    std::vector<WalletKey> getAllKeys() const;
    bool importPrivateKey(const PrivateKey& key, const std::string& label = "");
    bool importAddress(const Address& address, const std::string& label = "");
    PrivateKey getPrivateKey(const Address& address) const;
    bool hasPrivateKey(const Address& address) const;
    
    // Balance and UTXO management
    uint64_t getBalance() const;
    uint64_t getUnconfirmedBalance() const;
    uint64_t getImmatureBalance() const;
    std::vector<WalletUTXO> getUTXOs() const;
    std::vector<WalletUTXO> getSpendableUTXOs(uint64_t minAmount = 0) const;
    
    // Transaction management
    std::string sendToAddress(const Address& address, uint64_t amount, 
                             const std::string& comment = "", uint64_t feeRate = 1000);
    std::string sendMany(const std::vector<std::pair<Address, uint64_t>>& recipients,
                        const std::string& comment = "", uint64_t feeRate = 1000);
    Transaction createTransaction(const std::vector<std::pair<Address, uint64_t>>& outputs,
                                 uint64_t feeRate = 1000) const;
    bool signTransaction(Transaction& tx) const;
    
    // Transaction history
    std::vector<WalletTransaction> getTransactions(int32_t count = 100, int32_t skip = 0) const;
    WalletTransaction getTransaction(const std::string& txid) const;
    
    // Wallet info
    struct WalletInfo {
        std::string walletName;
        uint64_t balance;
        uint64_t unconfirmedBalance;
        uint64_t immatureBalance;
        uint32_t transactionCount;
        uint32_t addressCount;
        uint32_t keyCount;
        bool encrypted;
        uint64_t keystoreSize;
    };
    WalletInfo getInfo() const;
    
    // Encryption
    bool encryptWallet(const std::string& passphrase);
    bool changePassphrase(const std::string& oldPassphrase, const std::string& newPassphrase);
    bool unlock(const std::string& passphrase, uint32_t timeoutSeconds = 0);
    void lock();
    bool isLocked() const { return encrypted_ && !unlocked_; }
    
    // UTXO set integration
    void setUTXOSet(std::shared_ptr<UTXOSet> utxoSet) { utxoSet_ = utxoSet; }
    void updateFromBlock(const Block& block, uint32_t height);
    void updateFromMempool(const Transaction& tx);
    
    // Coin selection
    struct CoinSelectionResult {
        std::vector<WalletUTXO> selectedCoins;
        uint64_t totalAmount;
        uint64_t changeAmount;
        bool success;
        
        CoinSelectionResult() : totalAmount(0), changeAmount(0), success(false) {}
    };
    CoinSelectionResult selectCoins(uint64_t targetAmount, uint64_t feeRate = 1000) const;

private:
    // Internal methods
    void initializeWallet();
    bool loadFromFile();
    bool saveToFile() const;
    std::vector<uint8_t> encrypt(const std::vector<uint8_t>& data) const;
    std::vector<uint8_t> decrypt(const std::vector<uint8_t>& data) const;
    void updateBalance();
    uint64_t calculateFee(const Transaction& tx, uint64_t feeRate) const;
    
    // Wallet data
    std::string walletPath_;
    std::unordered_map<std::string, WalletKey> keys_;  // address -> key
    std::unordered_map<std::string, WalletTransaction> transactions_;  // txid -> transaction
    std::vector<WalletUTXO> utxos_;
    
    // Wallet state
    uint64_t balance_;
    uint64_t unconfirmedBalance_;
    uint64_t immatureBalance_;
    bool encrypted_;
    bool unlocked_;
    std::string encryptionKey_;
    uint64_t unlockTimeout_;
    
    // External dependencies
    std::shared_ptr<UTXOSet> utxoSet_;
    
    // Thread safety
    mutable std::mutex walletMutex_;
    
    // Constants
    static constexpr uint64_t COIN = 100000000;  // 1 BTC = 100M satoshis
    static constexpr uint32_t COINBASE_MATURITY = 100;
};

/**
 * Wallet Manager for handling multiple wallets
 */
class WalletManager {
public:
    static WalletManager& getInstance();
    
    // Wallet management
    std::shared_ptr<Wallet> createWallet(const std::string& name, const std::string& passphrase = "");
    std::shared_ptr<Wallet> loadWallet(const std::string& name, const std::string& passphrase = "");
    bool unloadWallet(const std::string& name);
    std::vector<std::string> listWallets() const;
    std::shared_ptr<Wallet> getWallet(const std::string& name = "") const;
    
    // Default wallet
    void setDefaultWallet(const std::string& name);
    std::string getDefaultWallet() const { return defaultWallet_; }

private:
    WalletManager() = default;
    std::unordered_map<std::string, std::shared_ptr<Wallet>> wallets_;
    std::string defaultWallet_;
    mutable std::mutex managerMutex_;
};

} // namespace pragma
