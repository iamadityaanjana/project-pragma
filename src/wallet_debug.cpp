#include "wallet/wallet.h"
#include <iostream>

using namespace pragma;

int main() {
    std::cout << "Testing wallet creation..." << std::endl;
    
    try {
        std::cout << "1. Getting WalletManager instance..." << std::endl;
        auto& walletManager = WalletManager::getInstance();
        
        std::cout << "2. Creating wallet..." << std::endl;
        auto wallet = walletManager.createWallet("test", "");
        
        if (wallet) {
            std::cout << "✅ Wallet created successfully!" << std::endl;
            
            std::cout << "3. Generating address..." << std::endl;
            auto addr = wallet->generateNewAddress("test");
            std::cout << "✅ Address generated: " << addr.toString() << std::endl;
            
        } else {
            std::cout << "❌ Failed to create wallet" << std::endl;
        }
        
        std::cout << "4. Listing wallets..." << std::endl;
        auto wallets = walletManager.listWallets();
        std::cout << "Found " << wallets.size() << " wallets" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "❌ Exception: " << e.what() << std::endl;
    }
    
    std::cout << "Test complete." << std::endl;
    return 0;
}
