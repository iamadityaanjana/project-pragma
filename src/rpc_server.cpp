#include "wallet/wallet.h"
#include "rpc/rpc.h"
#include "core/chainstate.h"
#include "core/mempool.h"
#include "core/utxo.h"
#include "core/validator.h"
#include <iostream>
#include <signal.h>
#include <thread>
#include <chrono>

using namespace pragma;

// Global server instance for signal handling
std::unique_ptr<RPCServer> g_rpcServer;

void signalHandler(int signal) {
    std::cout << "\nShutting down RPC server..." << std::endl;
    if (g_rpcServer) {
        g_rpcServer->stop();
    }
    exit(0);
}

int main(int argc, char* argv[]) {
    std::cout << "🌐 Pragma Blockchain RPC Server" << std::endl;
    std::cout << "===============================" << std::endl;
    
    // Setup signal handling
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // Parse command line arguments
    std::string configFile;
    uint16_t port = 8332;
    std::string bindAddress = "127.0.0.1";
    bool enableAuth = true;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--port" && i + 1 < argc) {
            port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--bind" && i + 1 < argc) {
            bindAddress = argv[++i];
        } else if (arg == "--no-auth") {
            enableAuth = false;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  --port <port>      Set RPC port (default: 8332)" << std::endl;
            std::cout << "  --bind <address>   Set bind address (default: 127.0.0.1)" << std::endl;
            std::cout << "  --no-auth          Disable authentication" << std::endl;
            std::cout << "  --help             Show this help" << std::endl;
            return 0;
        }
    }
    
    try {
        // Initialize blockchain components
        auto chainState = std::make_shared<ChainState>();
        auto utxoSet = std::make_shared<UTXOSet>();
        auto validator = std::make_shared<BlockValidator>(utxoSet.get(), chainState.get());
        auto mempool = std::make_shared<Mempool>(utxoSet.get(), validator.get());
        auto& walletManagerRef = WalletManager::getInstance();
        auto walletManager = std::shared_ptr<WalletManager>(&walletManagerRef, [](WalletManager*){});
        
        // Create default wallet if none exists
        auto wallets = walletManager->listWallets();
        if (wallets.empty()) {
            std::cout << "Creating default wallet..." << std::endl;
            auto defaultWallet = walletManager->createWallet("default", "");
            if (defaultWallet) {
                walletManager->setDefaultWallet("default");
                
                // Generate some initial addresses
                defaultWallet->generateNewAddress("primary");
                defaultWallet->generateNewAddress("mining");
                
                std::cout << "✅ Default wallet created with 2 addresses" << std::endl;
            }
        } else {
            std::cout << "Found existing wallets: ";
            for (const auto& name : wallets) {
                std::cout << name << " ";
            }
            std::cout << std::endl;
        }
        
        // Configure RPC server
        RPCServer::Config config;
        config.port = port;
        config.bindAddress = bindAddress;
        config.enableAuth = enableAuth;
        
        g_rpcServer = std::make_unique<RPCServer>(config);
        
        // Set blockchain components
        g_rpcServer->setChainState(chainState);
        g_rpcServer->setMempool(mempool);
        g_rpcServer->setWalletManager(walletManager);
        
        // Create RPC commands handler
        auto rpcCommands = std::make_shared<RPCCommands>(chainState, mempool, walletManager);
        
        // Register RPC methods
        g_rpcServer->registerMethod("getblockchaininfo", [rpcCommands](const std::string& params) {
            return rpcCommands->getBlockchainInfo(params);
        });
        
        g_rpcServer->registerMethod("getbestblockhash", [rpcCommands](const std::string& params) {
            return rpcCommands->getBestBlockHash(params);
        });
        
        g_rpcServer->registerMethod("getblockcount", [rpcCommands](const std::string& params) {
            return rpcCommands->getBlockCount(params);
        });
        
        g_rpcServer->registerMethod("getbalance", [rpcCommands](const std::string& params) {
            return rpcCommands->getBalance(params);
        });
        
        g_rpcServer->registerMethod("getnewaddress", [rpcCommands](const std::string& params) {
            return rpcCommands->getNewAddress(params);
        });
        
        g_rpcServer->registerMethod("sendtoaddress", [rpcCommands](const std::string& params) {
            return rpcCommands->sendToAddress(params);
        });
        
        g_rpcServer->registerMethod("listunspent", [rpcCommands](const std::string& params) {
            return rpcCommands->listUnspent(params);
        });
        
        g_rpcServer->registerMethod("getwalletinfo", [rpcCommands](const std::string& params) {
            return rpcCommands->getWalletInfo(params);
        });
        
        // Start the server
        std::cout << std::endl;
        std::cout << "Starting RPC server..." << std::endl;
        std::cout << "Listen address: " << bindAddress << ":" << port << std::endl;
        std::cout << "Authentication: " << (enableAuth ? "Enabled" : "Disabled") << std::endl;
        
        if (enableAuth) {
            std::cout << "Username: pragma" << std::endl;
            std::cout << "Password: pragma123" << std::endl;
        }
        
        std::cout << std::endl;
        std::cout << "Example curl commands:" << std::endl;
        std::cout << "  curl -X POST -H \"Content-Type: application/json\" \\" << std::endl;
        std::cout << "       -d '{\"method\":\"getbalance\",\"params\":[],\"id\":1}' \\" << std::endl;
        std::cout << "       http://";
        if (enableAuth) {
            std::cout << "pragma:pragma123@";
        }
        std::cout << bindAddress << ":" << port << std::endl;
        std::cout << std::endl;
        
        if (!g_rpcServer->start()) {
            std::cerr << "❌ Failed to start RPC server" << std::endl;
            return 1;
        }
        
        std::cout << "✅ RPC server is running. Press Ctrl+C to stop." << std::endl;
        
        // Keep the server running
        while (g_rpcServer->isRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
