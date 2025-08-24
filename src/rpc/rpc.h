#pragma once

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include "../wallet/wallet.h"
#include "../core/chainstate.h"
#include "../core/mempool.h"

namespace pragma {

// Forward declarations
class BlockValidator;

/**
 * JSON-RPC server for blockchain operations
 */
class RPCServer {
public:
    struct Config {
        std::string bindAddress = "127.0.0.1";
        uint16_t port = 8332;
        bool enableAuth = true;
        std::string username = "pragma";
        std::string password = "pragma123";
        int maxConnections = 100;
        bool enableSSL = false;
        std::string certFile;
        std::string keyFile;
    };

    explicit RPCServer(const Config& config);
    RPCServer(); // Default constructor
    ~RPCServer();

    // Server management
    bool start();
    void stop();
    bool isRunning() const { return running_; }

    // Set blockchain components
    void setChainState(std::shared_ptr<ChainState> chainState) { chainState_ = chainState; }
    void setMempool(std::shared_ptr<Mempool> mempool) { mempool_ = mempool; }
    void setWalletManager(std::shared_ptr<WalletManager> walletManager) { walletManager_ = walletManager; }
    void setValidator(std::shared_ptr<BlockValidator> validator) { validator_ = validator; }

    // JSON-RPC method registration
    using RPCMethod = std::function<std::string(const std::string& params)>;
    void registerMethod(const std::string& method, RPCMethod handler);

private:
    Config config_;
    std::atomic<bool> running_;
    std::thread serverThread_;
    std::shared_ptr<ChainState> chainState_;
    std::shared_ptr<Mempool> mempool_;
    std::shared_ptr<WalletManager> walletManager_;
    std::shared_ptr<BlockValidator> validator_;
    std::unordered_map<std::string, RPCMethod> methods_;
    mutable std::mutex serverMutex_;

    // Internal methods
    void serverLoop();
    void handleConnection(int clientSocket);
    std::string processRequest(const std::string& request);
    std::string createResponse(const std::string& id, const std::string& result, const std::string& error = "");
    bool authenticate(const std::string& auth);
    void registerDefaultMethods();
};

/**
 * RPC command implementations
 */
class RPCCommands {
public:
    explicit RPCCommands(std::shared_ptr<ChainState> chainState,
                        std::shared_ptr<Mempool> mempool,
                        std::shared_ptr<WalletManager> walletManager);

    // Blockchain information
    std::string getBlockchainInfo(const std::string& params);
    std::string getBestBlockHash(const std::string& params);
    std::string getBlockCount(const std::string& params);
    std::string getDifficulty(const std::string& params);
    std::string getBlock(const std::string& params);
    std::string getBlockHash(const std::string& params);
    std::string getBlockHeader(const std::string& params);
    std::string getChainTips(const std::string& params);

    // Transaction operations
    std::string getRawTransaction(const std::string& params);
    std::string sendRawTransaction(const std::string& params);
    std::string decodeRawTransaction(const std::string& params);
    std::string createRawTransaction(const std::string& params);
    std::string signRawTransaction(const std::string& params);

    // UTXO operations
    std::string getUTXOStats(const std::string& params);
    std::string getTxOut(const std::string& params);
    std::string getTxOutProof(const std::string& params);
    std::string verifyTxOutProof(const std::string& params);

    // Mempool operations
    std::string getMempoolInfo(const std::string& params);
    std::string getRawMempool(const std::string& params);
    std::string getMempoolEntry(const std::string& params);
    std::string getMempoolAncestors(const std::string& params);
    std::string getMempoolDescendants(const std::string& params);

    // Mining operations
    std::string getBlockTemplate(const std::string& params);
    std::string submitBlock(const std::string& params);
    std::string getMiningInfo(const std::string& params);
    std::string estimateFee(const std::string& params);

    // Wallet operations
    std::string getWalletInfo(const std::string& params);
    std::string getNewAddress(const std::string& params);
    std::string getBalance(const std::string& params);
    std::string listUnspent(const std::string& params);
    std::string sendToAddress(const std::string& params);
    std::string sendMany(const std::string& params);
    std::string listTransactions(const std::string& params);
    std::string getTransaction(const std::string& params);
    std::string importPrivKey(const std::string& params);
    std::string dumpPrivKey(const std::string& params);
    std::string encryptWallet(const std::string& params);
    std::string walletPassphrase(const std::string& params);
    std::string walletLock(const std::string& params);
    std::string backupWallet(const std::string& params);

    // Network operations
    std::string getNetworkInfo(const std::string& params);
    std::string getPeerInfo(const std::string& params);
    std::string addNode(const std::string& params);
    std::string disconnectNode(const std::string& params);
    std::string getConnectionCount(const std::string& params);

    // Utility operations
    std::string validateAddress(const std::string& params);
    std::string verifyMessage(const std::string& params);
    std::string signMessage(const std::string& params);
    std::string estimateSmartFee(const std::string& params);
    std::string ping(const std::string& params);
    std::string uptime(const std::string& params);
    std::string help(const std::string& params);

private:
    std::shared_ptr<ChainState> chainState_;
    std::shared_ptr<Mempool> mempool_;
    std::shared_ptr<WalletManager> walletManager_;

    // Helper methods
    std::string parseJSON(const std::string& json);
    std::string createJSONResponse(const std::string& result);
    std::string createJSONError(int code, const std::string& message);
    std::vector<std::string> splitParams(const std::string& params);
    bool isValidHash(const std::string& hash);
    std::shared_ptr<Wallet> getDefaultWallet();
};

/**
 * Simple JSON parser/generator for RPC
 */
class JSONParser {
public:
    struct Value {
        enum Type { STRING, NUMBER, BOOLEAN, ARRAY, OBJECT, NULL_TYPE };
        Type type;
        std::string stringValue;
        double numberValue;
        bool boolValue;
        std::vector<Value> arrayValue;
        std::unordered_map<std::string, Value> objectValue;

        Value() : type(NULL_TYPE), numberValue(0), boolValue(false) {}
        explicit Value(const std::string& s) : type(STRING), stringValue(s), numberValue(0), boolValue(false) {}
        explicit Value(double n) : type(NUMBER), numberValue(n), boolValue(false) {}
        explicit Value(bool b) : type(BOOLEAN), numberValue(0), boolValue(b) {}
    };

    static Value parse(const std::string& json);
    static std::string stringify(const Value& value);
    static std::string escape(const std::string& str);
    static std::string unescape(const std::string& str);

private:
    static Value parseValue(const std::string& json, size_t& pos);
    static Value parseString(const std::string& json, size_t& pos);
    static Value parseNumber(const std::string& json, size_t& pos);
    static Value parseArray(const std::string& json, size_t& pos);
    static Value parseObject(const std::string& json, size_t& pos);
    static void skipWhitespace(const std::string& json, size_t& pos);
};

/**
 * CLI interface for wallet operations
 */
class CLI {
public:
    explicit CLI(std::shared_ptr<WalletManager> walletManager);
    
    // Main CLI loop
    void run();
    void processCommand(const std::string& command);
    
    // Command handlers
    void showHelp();
    void createWallet(const std::vector<std::string>& args);
    void loadWallet(const std::vector<std::string>& args);
    void getInfo();
    void getBalance();
    void getNewAddress(const std::vector<std::string>& args);
    void listAddresses();
    void sendToAddress(const std::vector<std::string>& args);
    void listTransactions(const std::vector<std::string>& args);
    void importPrivateKey(const std::vector<std::string>& args);
    void exportPrivateKey(const std::vector<std::string>& args);
    void listUnspent();
    void encryptWallet(const std::vector<std::string>& args);
    void unlockWallet(const std::vector<std::string>& args);
    void lockWallet();
    void backupWallet(const std::vector<std::string>& args);
    void exit();

private:
    std::shared_ptr<WalletManager> walletManager_;
    std::string currentWallet_;
    bool running_;
    
    std::vector<std::string> parseCommand(const std::string& command);
    std::shared_ptr<Wallet> getCurrentWallet();
    void printError(const std::string& error);
    void printSuccess(const std::string& message);
    std::string formatAmount(uint64_t amount);
    std::string getPassword(const std::string& prompt);
};

} // namespace pragma
