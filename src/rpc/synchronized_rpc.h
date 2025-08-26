#include "../rpc/rpc.h"
#include "synchronized_blockchain.h"
#include <nlohmann/json.hpp>
#include <thread>
#include <chrono>

namespace pragma {

class SynchronizedRPCServer {
private:
    std::unique_ptr<RPCServer> rpcServer_;
    SynchronizedBlockchainManager* blockchain_;
    uint16_t port_;
    std::string nodeId_;
    
public:
    SynchronizedRPCServer(uint16_t port, const std::string& nodeId) 
        : port_(port), nodeId_(nodeId) {
        blockchain_ = GlobalBlockchainManager::getInstance();
        if (!blockchain_) {
            throw std::runtime_error("Blockchain not initialized");
        }
        
        // Initialize RPC server
        RPCConfig config;
        config.port = port;
        config.host = "127.0.0.1";
        config.username = "pragma";
        config.password = "local";
        
        rpcServer_ = std::make_unique<RPCServer>(config);
        setupRPCMethods();
    }
    
    void setupRPCMethods() {
        // Blockchain info methods
        rpcServer_->registerMethod("getblockchaininfo", [this](const nlohmann::json& params) {
            auto stats = blockchain_->getNetworkStats();
            nlohmann::json result;
            result["chain"] = "pragma";
            result["blocks"] = stats.blockHeight;
            result["bestblockhash"] = stats.bestBlockHash;
            result["difficulty"] = 1;
            result["mediantime"] = std::time(nullptr);
            result["verificationprogress"] = stats.synchronized ? 1.0 : 0.5;
            result["chainwork"] = "0x" + std::string(64, '0');
            result["size_on_disk"] = 0;
            result["pruned"] = false;
            result["warnings"] = "";
            return result;
        });
        
        // Network info methods
        rpcServer_->registerMethod("getnetworkinfo", [this](const nlohmann::json& params) {
            auto stats = blockchain_->getNetworkStats();
            nlohmann::json result;
            result["version"] = 210000;
            result["subversion"] = "/Pragma:1.0.0/";
            result["protocolversion"] = 70015;
            result["localservices"] = "0000000000000409";
            result["localrelay"] = true;
            result["timeoffset"] = 0;
            result["connections"] = stats.connectedPeers;
            result["networkactive"] = true;
            result["networks"] = nlohmann::json::array();
            result["relayfee"] = 0.00001000;
            result["incrementalfee"] = 0.00001000;
            result["localaddresses"] = nlohmann::json::array();
            result["warnings"] = "";
            return result;
        });
        
        // Peer info
        rpcServer_->registerMethod("getpeerinfo", [this](const nlohmann::json& params) {
            auto peers = blockchain_->getConnectedPeers();
            nlohmann::json result = nlohmann::json::array();
            
            for (size_t i = 0; i < peers.size(); ++i) {
                nlohmann::json peer;
                peer["id"] = i;
                peer["addr"] = peers[i];
                peer["addrbind"] = "127.0.0.1:" + std::to_string(port_);
                peer["services"] = "0000000000000409";
                peer["relaytxes"] = true;
                peer["lastsend"] = std::time(nullptr);
                peer["lastrecv"] = std::time(nullptr);
                peer["bytessent"] = 1000;
                peer["bytesrecv"] = 1000;
                peer["conntime"] = std::time(nullptr) - 300;
                peer["timeoffset"] = 0;
                peer["pingtime"] = 0.001;
                peer["version"] = 70015;
                peer["subver"] = "/Pragma:1.0.0/";
                peer["inbound"] = false;
                peer["addnode"] = false;
                peer["startingheight"] = blockchain_->getBlockHeight();
                peer["banscore"] = 0;
                peer["synced_headers"] = blockchain_->getBlockHeight();
                peer["synced_blocks"] = blockchain_->getBlockHeight();
                result.push_back(peer);
            }
            
            return result;
        });
        
        // Wallet methods
        rpcServer_->registerMethod("getnewaddress", [this](const nlohmann::json& params) {
            std::string label = params.size() > 0 ? params[0].get<std::string>() : "";
            std::string address = blockchain_->createNewAddress();
            return nlohmann::json(address);
        });
        
        rpcServer_->registerMethod("getbalance", [this](const nlohmann::json& params) {
            std::string account = params.size() > 0 ? params[0].get<std::string>() : "*";
            
            if (account == "*") {
                // Get total balance of all addresses
                auto addresses = blockchain_->listWalletAddresses();
                double total = 0.0;
                for (const auto& addr : addresses) {
                    total += blockchain_->getBalance(addr);
                }
                return nlohmann::json(total);
            } else {
                return nlohmann::json(blockchain_->getBalance(account));
            }
        });
        
        rpcServer_->registerMethod("getaddressbalance", [this](const nlohmann::json& params) {
            if (params.size() == 0) {
                throw std::runtime_error("Address parameter required");
            }
            std::string address = params[0].get<std::string>();
            double balance = blockchain_->getBalance(address);
            return nlohmann::json(balance);
        });
        
        rpcServer_->registerMethod("listaddresses", [this](const nlohmann::json& params) {
            auto addresses = blockchain_->listWalletAddresses();
            nlohmann::json result = nlohmann::json::array();
            
            for (const auto& addr : addresses) {
                nlohmann::json addressInfo;
                addressInfo["address"] = addr;
                addressInfo["balance"] = blockchain_->getBalance(addr);
                result.push_back(addressInfo);
            }
            
            return result;
        });
        
        // Transaction methods
        rpcServer_->registerMethod("sendtoaddress", [this](const nlohmann::json& params) {
            if (params.size() < 3) {
                throw std::runtime_error("sendtoaddress requires: fromaddress, toaddress, amount");
            }
            
            std::string fromAddress = params[0].get<std::string>();
            std::string toAddress = params[1].get<std::string>();
            double amount = params[2].get<double>();
            
            std::string txHash = blockchain_->sendTransaction(fromAddress, toAddress, amount);
            if (txHash.empty()) {
                throw std::runtime_error("Failed to send transaction");
            }
            
            return nlohmann::json(txHash);
        });
        
        rpcServer_->registerMethod("gettransaction", [this](const nlohmann::json& params) {
            if (params.size() == 0) {
                throw std::runtime_error("Transaction hash required");
            }
            
            std::string txHash = params[0].get<std::string>();
            auto tx = blockchain_->getTransaction(txHash);
            
            nlohmann::json result;
            result["txid"] = tx.hash;
            result["confirmations"] = 1; // Simplified
            result["time"] = tx.timestamp;
            result["blocktime"] = tx.timestamp;
            
            nlohmann::json details = nlohmann::json::array();
            for (const auto& output : tx.outputs) {
                nlohmann::json detail;
                detail["account"] = "";
                detail["address"] = output.address;
                detail["category"] = "receive";
                detail["amount"] = output.amount;
                details.push_back(detail);
            }
            result["details"] = details;
            
            return result;
        });
        
        // Mining methods
        rpcServer_->registerMethod("startmining", [this](const nlohmann::json& params) {
            if (params.size() == 0) {
                throw std::runtime_error("Miner address required");
            }
            
            std::string minerAddress = params[0].get<std::string>();
            blockchain_->startMining(minerAddress);
            
            nlohmann::json result;
            result["status"] = "Mining started";
            result["address"] = minerAddress;
            return result;
        });
        
        rpcServer_->registerMethod("stopmining", [this](const nlohmann::json& params) {
            blockchain_->stopMining();
            
            nlohmann::json result;
            result["status"] = "Mining stopped";
            return result;
        });
        
        rpcServer_->registerMethod("getmininginfo", [this](const nlohmann::json& params) {
            nlohmann::json result;
            result["blocks"] = blockchain_->getBlockHeight();
            result["currentblocksize"] = 1000;
            result["currentblocktx"] = 1;
            result["difficulty"] = 1.0;
            result["networkhashps"] = 1000.0;
            result["pooledtx"] = 0;
            result["chain"] = "pragma";
            result["generate"] = blockchain_->isMining();
            result["genproclimit"] = 1;
            result["hashespersec"] = blockchain_->isMining() ? 1000.0 : 0.0;
            return result;
        });
        
        // Block methods
        rpcServer_->registerMethod("getblock", [this](const nlohmann::json& params) {
            if (params.size() == 0) {
                throw std::runtime_error("Block hash required");
            }
            
            std::string blockHash = params[0].get<std::string>();
            auto block = blockchain_->getBlock(blockHash);
            
            nlohmann::json result;
            result["hash"] = block.hash;
            result["height"] = block.height;
            result["version"] = 1;
            result["merkleroot"] = block.merkleRoot;
            result["time"] = block.timestamp;
            result["nonce"] = block.nonce;
            result["bits"] = "1d00ffff";
            result["difficulty"] = 1.0;
            result["chainwork"] = "0x" + std::string(64, '0');
            result["previousblockhash"] = block.previousHash;
            result["size"] = 285;
            result["strippedsize"] = 285;
            result["weight"] = 1140;
            
            nlohmann::json txArray = nlohmann::json::array();
            for (const auto& tx : block.transactions) {
                txArray.push_back(tx.hash);
            }
            result["tx"] = txArray;
            
            return result;
        });
        
        rpcServer_->registerMethod("getblockhash", [this](const nlohmann::json& params) {
            if (params.size() == 0) {
                throw std::runtime_error("Block height required");
            }
            
            uint32_t height = params[0].get<uint32_t>();
            auto block = blockchain_->getBlockByHeight(height);
            return nlohmann::json(block.hash);
        });
        
        rpcServer_->registerMethod("getbestblockhash", [this](const nlohmann::json& params) {
            return nlohmann::json(blockchain_->getBestBlockHash());
        });
        
        rpcServer_->registerMethod("getblockcount", [this](const nlohmann::json& params) {
            return nlohmann::json(blockchain_->getBlockHeight());
        });
        
        // Node management
        rpcServer_->registerMethod("addnode", [this](const nlohmann::json& params) {
            if (params.size() < 2) {
                throw std::runtime_error("addnode requires: address, command");
            }
            
            std::string address = params[0].get<std::string>();
            std::string command = params[1].get<std::string>();
            
            if (command == "add" || command == "onetry") {
                // Parse address:port
                size_t colonPos = address.find(':');
                if (colonPos != std::string::npos) {
                    std::string host = address.substr(0, colonPos);
                    uint16_t port = static_cast<uint16_t>(std::stoi(address.substr(colonPos + 1)));
                    
                    bool success = blockchain_->connectToPeer(host, port);
                    
                    nlohmann::json result;
                    result["status"] = success ? "Connected" : "Failed";
                    result["address"] = address;
                    return result;
                }
            }
            
            throw std::runtime_error("Invalid address format or command");
        });
        
        // Debug methods
        rpcServer_->registerMethod("getinfo", [this](const nlohmann::json& params) {
            auto stats = blockchain_->getNetworkStats();
            nlohmann::json result;
            result["version"] = 210000;
            result["protocolversion"] = 70015;
            result["blocks"] = stats.blockHeight;
            result["timeoffset"] = 0;
            result["connections"] = stats.connectedPeers;
            result["proxy"] = "";
            result["difficulty"] = 1.0;
            result["testnet"] = false;
            result["relayfee"] = 0.00001000;
            result["errors"] = "";
            result["node_id"] = nodeId_;
            result["synchronized"] = stats.synchronized;
            result["mining"] = blockchain_->isMining();
            result["total_supply"] = stats.totalSupply;
            return result;
        });
        
        rpcServer_->registerMethod("ping", [this](const nlohmann::json& params) {
            nlohmann::json result;
            result["status"] = "pong";
            result["node_id"] = nodeId_;
            result["timestamp"] = std::time(nullptr);
            return result;
        });
    }
    
    bool start() {
        return rpcServer_->start();
    }
    
    void stop() {
        if (rpcServer_) {
            rpcServer_->stop();
        }
    }
    
    uint16_t getPort() const {
        return port_;
    }
    
    std::string getNodeId() const {
        return nodeId_;
    }
};

} // namespace pragma
