#include "wallet/wallet.h"
#include "rpc/rpc.h"
#include <iostream>

using namespace pragma;

int main(int argc, char* argv[]) {
    std::cout << "🪙 Pragma Blockchain Wallet" << std::endl;
    std::cout << "===========================" << std::endl;
    
    // Initialize wallet manager
    auto& walletManagerRef = WalletManager::getInstance();
    auto walletManager = std::shared_ptr<WalletManager>(&walletManagerRef, [](WalletManager*){});
    
    // Create CLI interface
    CLI cli(walletManager);
    
    // Check if running in batch mode (with arguments)
    if (argc > 1) {
        std::string command;
        for (int i = 1; i < argc; ++i) {
            if (i > 1) command += " ";
            command += argv[i];
        }
        
        std::cout << "Executing command: " << command << std::endl;
        cli.processCommand(command);
        return 0;
    }
    
    // Run interactive CLI
    std::cout << std::endl;
    cli.run();
    
    return 0;
}
