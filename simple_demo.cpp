#include "src/core/chainstate.h"
#include "src/core/mempool.h"
#include "src/core/block.h"
#include "src/core/transaction.h"
#include "src/wallet/wallet.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <memory>
#include <mutex>

using namespace pragma;

class SimpleBlockchainDemo {
private:
    std::unique_ptr<ChainState> chainState_;
    std::unique_ptr<Mempool> mempool_;
    std::unique_ptr<Wallet> wallet_;
    std::mutex mtx_;
    bool mining_;
    std::thread miningThread_;
    
public:
    SimpleBlockchainDemo() : mining_(false) {
        chainState_ = std::make_unique<ChainState>();
        mempool_ = std::make_unique<Mempool>();
        wallet_ = std::make_unique<Wallet>("demo_wallet");
    }
    
    ~SimpleBlockchainDemo() {
        stopMining();
    }
    
    void initialize() {
        std::lock_guard<std::mutex> lock(mtx_);
        
        // Create and add genesis block if needed
        if (chainState_->getTotalBlocks() == 0) {
            std::cout << "Creating genesis block..." << std::endl;
            
            // Create a demo address for genesis
            PrivateKey genesisKey = PrivateKey::generateRandom();
            std::string genesisAddress = genesisKey.getPublicKey().getAddress();
            
            Block genesis = Block::createGenesis(genesisAddress, 50 * 100000000ULL); // 50 BTC in satoshis
            
            if (chainState_->addBlock(genesis)) {
                std::cout << "Genesis block created: " << genesis.hash << std::endl;
            } else {
                std::cout << "Failed to create genesis block" << std::endl;
            }
        }
        
        std::cout << "Blockchain initialized with " << chainState_->getTotalBlocks() << " blocks" << std::endl;
        std::cout << "Current best block: " << chainState_->getBestHash() << std::endl;
        std::cout << "Current height: " << chainState_->getBestHeight() << std::endl;
    }
    
    void createDemoAddresses() {
        std::lock_guard<std::mutex> lock(mtx_);
        
        std::cout << "\nCreating demo addresses..." << std::endl;
        
        // Create some demo addresses
        for (int i = 0; i < 3; i++) {
            PrivateKey key = PrivateKey::generateRandom();
            std::string address = key.getPublicKey().getAddress();
            std::cout << "Address " << (i + 1) << ": " << address << std::endl;
        }
    }
    
    void startMining() {
        std::lock_guard<std::mutex> lock(mtx_);
        
        if (mining_) {
            std::cout << "Mining already started" << std::endl;
            return;
        }
        
        mining_ = true;
        miningThread_ = std::thread(&SimpleBlockchainDemo::miningLoop, this);
        std::cout << "Mining started..." << std::endl;
    }
    
    void stopMining() {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            mining_ = false;
        }
        
        if (miningThread_.joinable()) {
            miningThread_.join();
        }
        std::cout << "Mining stopped" << std::endl;
    }
    
    void miningLoop() {
        while (mining_) {
            try {
                // Mine a new block
                std::lock_guard<std::mutex> lock(mtx_);
                
                auto bestTip = chainState_->getBestTip();
                if (!bestTip) {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    continue;
                }
                
                // Create miner address
                PrivateKey minerKey = PrivateKey::generateRandom();
                std::string minerAddress = minerKey.getPublicKey().getAddress();
                
                // Get pending transactions from mempool
                std::vector<Transaction> txs;
                // Note: Real mempool integration would go here
                
                // Create new block
                Block newBlock = Block::create(bestTip->block, txs, minerAddress, 50 * 100000000ULL);
                
                std::cout << "Mining block " << (bestTip->height + 1) << "..." << std::endl;
                
                // Mine the block (this will take some time)
                newBlock.mine(100000); // Mine with limited iterations for demo
                
                if (newBlock.meetsTarget()) {
                    if (chainState_->addBlock(newBlock)) {
                        std::cout << "✓ Mined block " << newBlock.hash << " at height " << (bestTip->height + 1) << std::endl;
                        std::cout << "  Miner: " << minerAddress << std::endl;
                        std::cout << "  Reward: 50 BTC" << std::endl;
                    } else {
                        std::cout << "✗ Failed to add mined block to chain" << std::endl;
                    }
                }
                
            } catch (const std::exception& e) {
                std::cout << "Mining error: " << e.what() << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }
            
            // Wait before mining next block
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }
    }
    
    void printStatus() {
        std::lock_guard<std::mutex> lock(mtx_);
        
        std::cout << "\n=== Blockchain Status ===" << std::endl;
        std::cout << "Total blocks: " << chainState_->getTotalBlocks() << std::endl;
        std::cout << "Best height: " << chainState_->getBestHeight() << std::endl;
        std::cout << "Best hash: " << chainState_->getBestHash() << std::endl;
        std::cout << "Mining: " << (mining_ ? "Active" : "Inactive") << std::endl;
        std::cout << "========================\n" << std::endl;
    }
};

int main(int argc, char* argv[]) {
    std::cout << "Pragma Simple Blockchain Demo" << std::endl;
    std::cout << "=============================" << std::endl;
    
    try {
        SimpleBlockchainDemo demo;
        
        // Initialize blockchain
        demo.initialize();
        
        // Create demo addresses
        demo.createDemoAddresses();
        
        // Print initial status
        demo.printStatus();
        
        // Start mining
        demo.startMining();
        
        // Run for demo period
        std::cout << "Running demo for 60 seconds..." << std::endl;
        
        for (int i = 0; i < 6; i++) {
            std::this_thread::sleep_for(std::chrono::seconds(10));
            demo.printStatus();
        }
        
        // Stop mining
        demo.stopMining();
        
        // Final status
        demo.printStatus();
        
        std::cout << "Demo completed successfully!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "Demo error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
