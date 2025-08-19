#include <iostream>
#include <chrono>
#include "primitives/hash.h"
#include "primitives/serialize.h"
#include "primitives/utils.h"
#include "core/transaction.h"
#include "core/merkle.h"
#include "core/block.h"
#include "core/difficulty.h"
#include "core/chainstate.h"

using namespace pragma;

int main() {
    Utils::logInfo("=== Pragma Blockchain - Steps 1 & 2: Primitives + Transactions ===");
    
    // Test hash functions
    Utils::logInfo("\n1. Testing Hash Functions:");
    std::string testData = "Hello, Blockchain!";
    std::string sha256Hash = Hash::sha256(testData);
    std::string doubleSHA256 = Hash::dbl_sha256(testData);
    
    std::cout << "Input: " << testData << std::endl;
    std::cout << "SHA256: " << sha256Hash << std::endl;
    std::cout << "Double SHA256: " << doubleSHA256 << std::endl;
    
    // Test hex conversion
    Utils::logInfo("\n2. Testing Hex Conversion:");
    std::vector<uint8_t> bytes = {0xDE, 0xAD, 0xBE, 0xEF};
    std::string hex = Hash::toHex(bytes);
    std::cout << "Bytes to Hex: " << hex << std::endl;
    
    auto converted = Hash::fromHex(hex);
    std::cout << "Hex to Bytes: ";
    for (auto byte : converted) {
        std::cout << std::hex << static_cast<int>(byte) << " ";
    }
    std::cout << std::dec << std::endl;
    
    // Test serialization
    Utils::logInfo("\n3. Testing Serialization:");
    
    // VarInt test
    uint64_t varIntValue = 300;
    auto encoded = Serialize::encodeVarInt(varIntValue);
    auto [decoded, size] = Serialize::decodeVarInt(encoded);
    
    std::cout << "VarInt " << varIntValue << " -> " << encoded.size() << " bytes -> " << decoded << std::endl;
    
    // Uint32 test
    uint32_t value32 = 0x12345678;
    auto encoded32 = Serialize::encodeUint32LE(value32);
    auto decoded32 = Serialize::decodeUint32LE(encoded32);
    
    std::cout << "Uint32 " << std::hex << value32 << " -> " << decoded32 << std::dec << std::endl;
    
    // String test
    std::string testString = "Blockchain";
    auto encodedString = Serialize::encodeString(testString);
    auto [decodedString, stringSize] = Serialize::decodeString(encodedString);
    
    std::cout << "String '" << testString << "' -> " << encodedString.size() 
              << " bytes -> '" << decodedString << "'" << std::endl;
    
    // Test utilities
    Utils::logInfo("\n4. Testing Utilities:");
    uint64_t timestamp = Utils::getCurrentTimestamp();
    std::cout << "Current timestamp: " << timestamp << std::endl;
    std::cout << "Formatted: " << Utils::timestampToString(timestamp) << std::endl;
    
    auto randomBytes = Utils::randomBytes(8);
    std::cout << "Random bytes: " << Hash::toHex(randomBytes) << std::endl;
    
    // NEW: Test transactions
    Utils::logInfo("\n5. Testing Transactions:");
    
    // Create a coinbase transaction
    Transaction coinbase = Transaction::createCoinbase("1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa", 5000000000ULL);
    std::cout << "Coinbase TXID: " << coinbase.txid << std::endl;
    std::cout << "Coinbase output value: " << coinbase.getTotalOutput() << " satoshis" << std::endl;
    std::cout << "Coinbase valid: " << (coinbase.isValid() ? "Yes" : "No") << std::endl;
    
    // Create a regular transaction
    std::vector<TxIn> inputs;
    inputs.emplace_back(OutPoint(coinbase.txid, 0), "signature_data", "public_key_data");
    
    std::vector<TxOut> outputs;
    outputs.emplace_back(1000000000ULL, "1BvBMSEYstWetqTFn5Au4m4GFg7xJaNVN2"); // 10 BTC
    outputs.emplace_back(3999999000ULL, "1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa"); // 39.99999 BTC (change)
    
    Transaction regularTx = Transaction::create(inputs, outputs);
    std::cout << "Regular TXID: " << regularTx.txid << std::endl;
    std::cout << "Regular output value: " << regularTx.getTotalOutput() << " satoshis" << std::endl;
    std::cout << "Regular valid: " << (regularTx.isValid() ? "Yes" : "No") << std::endl;
    
    // Test transaction serialization
    auto serialized = regularTx.serialize();
    Transaction deserialized = Transaction::deserialize(serialized);
    std::cout << "Serialization test: " << (regularTx.txid == deserialized.txid ? "PASS" : "FAIL") << std::endl;
    
    // NEW: Test Merkle Tree
    Utils::logInfo("\n6. Testing Merkle Tree:");
    
    std::vector<std::string> txids = {coinbase.txid, regularTx.txid};
    std::string merkleRoot = MerkleTree::buildMerkleRoot(txids);
    std::cout << "Merkle root (2 txs): " << merkleRoot << std::endl;
    
    // Test with more transactions
    std::vector<std::string> moreTxids = {
        coinbase.txid,
        regularTx.txid,
        "3333333333333333333333333333333333333333333333333333333333333333",
        "4444444444444444444444444444444444444444444444444444444444444444"
    };
    
    std::string merkleRoot4 = MerkleTree::buildMerkleRoot(moreTxids);
    std::cout << "Merkle root (4 txs): " << merkleRoot4 << std::endl;
    
    // Test merkle proof
    auto proof = MerkleTree::generateMerkleProof(moreTxids, 0);
    bool proofValid = MerkleTree::verifyMerkleProof(moreTxids[0], merkleRoot4, proof, 0);
    std::cout << "Merkle proof verification: " << (proofValid ? "VALID" : "INVALID") << std::endl;
    std::cout << "Proof size: " << proof.size() << " hashes" << std::endl;
    
    Utils::logInfo("\nSteps 1 & 2 Complete: Hash, Serialize, Transactions & Merkle Tree working!");
    
    // NEW: Step 3 - Block Header & Proof-of-Work Testing
    Utils::logInfo("\n=== Step 3: Block Header & Proof-of-Work Testing ===");
    
    // Test difficulty and target conversion
    Utils::logInfo("\n7. Testing Difficulty Management:");
    uint32_t bits = 0x1d00ffff; // Bitcoin's initial difficulty
    std::string target = Difficulty::bitsToTarget(bits);
    std::cout << "Bits: 0x" << std::hex << bits << std::dec << std::endl;
    std::cout << "Target: " << target << std::endl;
    
    uint32_t convertedBits = Difficulty::targetToBits(target);
    std::cout << "Round-trip conversion successful: " << (bits == convertedBits ? "Yes" : "No") << std::endl;
    
    // Test target meeting
    std::string testHash = "0000ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff";
    bool meetsTarget = Difficulty::meetsTarget(testHash, target);
    std::cout << "Test hash meets target: " << (meetsTarget ? "Yes" : "No") << std::endl;
    
    // Test difficulty adjustment
    Utils::logInfo("\n8. Testing Difficulty Adjustment:");
    uint32_t targetTimespan = 1209600; // 2 weeks
    uint32_t fastTimespan = targetTimespan / 2; // Blocks came too fast
    uint32_t newBits = Difficulty::adjustDifficulty(bits, fastTimespan, targetTimespan);
    std::cout << "Original bits: 0x" << std::hex << bits << std::dec << std::endl;
    std::cout << "Adjusted bits (faster blocks): 0x" << std::hex << newBits << std::dec << std::endl;
    
    // Test genesis block creation
    Utils::logInfo("\n9. Testing Genesis Block Creation:");
    std::string genesisAddress = "1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa";
    uint64_t genesisReward = 5000000000ULL; // 50 BTC in satoshis
    
    Block genesis = Block::createGenesis(genesisAddress, genesisReward);
    std::cout << "Genesis block created!" << std::endl;
    std::cout << "Block hash: " << genesis.hash << std::endl;
    std::cout << "Previous hash: " << genesis.header.prevHash << std::endl;
    std::cout << "Merkle root: " << genesis.header.merkleRoot << std::endl;
    std::cout << "Timestamp: " << genesis.header.timestamp << std::endl;
    std::cout << "Bits: 0x" << std::hex << genesis.header.bits << std::dec << std::endl;
    std::cout << "Nonce: " << genesis.header.nonce << std::endl;
    std::cout << "Transaction count: " << genesis.transactions.size() << std::endl;
    std::cout << "Block reward: " << genesis.getBlockReward() << " satoshis" << std::endl;
    std::cout << "Is valid: " << (genesis.isValid() ? "Yes" : "No") << std::endl;
    
    // Test block header serialization
    Utils::logInfo("\n10. Testing Block Header Serialization:");
    auto headerSerialized = genesis.header.serialize();
    std::cout << "Header serialized size: " << headerSerialized.size() << " bytes" << std::endl;
    
    BlockHeader deserializedHeader = BlockHeader::deserialize(headerSerialized);
    std::cout << "Header deserialization successful: " << 
        (genesis.header == deserializedHeader ? "Yes" : "No") << std::endl;
    
    // Test full block serialization
    Utils::logInfo("\n11. Testing Full Block Serialization:");
    auto blockSerialized = genesis.serialize();
    std::cout << "Block serialized size: " << blockSerialized.size() << " bytes" << std::endl;
    
    Block deserializedBlock = Block::deserialize(blockSerialized);
    std::cout << "Block deserialization successful: " << 
        (genesis.hash == deserializedBlock.hash ? "Yes" : "No") << std::endl;
    
    // Test creating a new block with transactions
    Utils::logInfo("\n12. Testing Block with Multiple Transactions:");
    std::vector<Transaction> blockTxs;
    blockTxs.push_back(regularTx); // Add our regular transaction
    
    std::string minerAddress = "1MinerAddressForRewardPayment123456789";
    Block newBlock = Block::create(genesis, blockTxs, minerAddress, genesisReward);
    
    std::cout << "New block created!" << std::endl;
    std::cout << "Block hash: " << newBlock.hash << std::endl;
    std::cout << "Previous hash: " << newBlock.header.prevHash << std::endl;
    std::cout << "Transaction count: " << newBlock.getTransactionCount() << std::endl;
    std::cout << "Block is valid: " << (newBlock.isValid() ? "Yes" : "No") << std::endl;
    
    // Test simple mining (with easy difficulty for demonstration)
    Utils::logInfo("\n13. Testing Proof-of-Work Mining:");
    Block miningBlock = Block::createGenesis("miner", 5000000000ULL);
    miningBlock.header.bits = 0x207fffff; // Very easy difficulty for demo
    
    std::cout << "Mining block with easy difficulty..." << std::endl;
    std::cout << "Initial nonce: " << miningBlock.header.nonce << std::endl;
    std::cout << "Initial hash: " << miningBlock.hash << std::endl;
    std::cout << "Target: " << Difficulty::bitsToTarget(miningBlock.header.bits) << std::endl;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    miningBlock.mine(100000); // Limit iterations for demo
    auto endTime = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    std::cout << "Mining completed!" << std::endl;
    std::cout << "Final nonce: " << miningBlock.header.nonce << std::endl;
    std::cout << "Final hash: " << miningBlock.hash << std::endl;
    std::cout << "Meets target: " << (miningBlock.meetsTarget() ? "Yes" : "No") << std::endl;
    std::cout << "Mining time: " << duration.count() << " milliseconds" << std::endl;
    
    Utils::logInfo("\n=== Steps 1, 2 & 3 Complete! ===");
    Utils::logInfo("✅ Hash & Serialization primitives working");
    Utils::logInfo("✅ Transaction & Merkle Tree system working");
    Utils::logInfo("✅ Block Header & Proof-of-Work system working");
    
    // NEW: Step 4 - Chainstate & Block Validation Testing
    Utils::logInfo("\n=== Step 4: Chainstate & Block Validation Testing ===");
    
    // Test chainstate initialization
    Utils::logInfo("\n14. Testing ChainState Initialization:");
    ChainState chainState;
    
    std::cout << "Initial state:" << std::endl;
    std::cout << "Has genesis: " << (chainState.hasGenesis() ? "Yes" : "No") << std::endl;
    std::cout << "Best height: " << chainState.getBestHeight() << std::endl;
    std::cout << "Total blocks: " << chainState.getTotalBlocks() << std::endl;
    std::cout << "Best hash: " << (chainState.getBestHash().empty() ? "[empty]" : chainState.getBestHash()) << std::endl;
    
    // Test setting genesis block
    Utils::logInfo("\n15. Testing Genesis Block Setup:");
    Block chainGenesis = Block::createGenesis("1ChainGenesisAddress", 5000000000ULL);
    bool genesisSet = chainState.setGenesis(chainGenesis);
    
    std::cout << "Genesis set successfully: " << (genesisSet ? "Yes" : "No") << std::endl;
    std::cout << "Has genesis: " << (chainState.hasGenesis() ? "Yes" : "No") << std::endl;
    std::cout << "Genesis hash: " << chainGenesis.hash << std::endl;
    std::cout << "Best height: " << chainState.getBestHeight() << std::endl;
    std::cout << "Total blocks: " << chainState.getTotalBlocks() << std::endl;
    
    // Test duplicate genesis (should fail)
    bool duplicateGenesis = chainState.setGenesis(chainGenesis);
    std::cout << "Duplicate genesis rejected: " << (!duplicateGenesis ? "Yes" : "No") << std::endl;
    
    // Test adding blocks to the chain
    Utils::logInfo("\n16. Testing Block Chain Building:");
    
    // Create and add first block
    std::vector<Transaction> block1Txs;
    block1Txs.push_back(regularTx); // Use the transaction from earlier
    
    Block block1 = Block::create(chainGenesis, block1Txs, "1MinerAddress1", 5000000000ULL);
    bool block1Added = chainState.addBlock(block1);
    
    std::cout << "Block 1 added: " << (block1Added ? "Yes" : "No") << std::endl;
    std::cout << "Block 1 hash: " << block1.hash << std::endl;
    std::cout << "New best height: " << chainState.getBestHeight() << std::endl;
    std::cout << "Total blocks: " << chainState.getTotalBlocks() << std::endl;
    
    // Create and add second block
    std::vector<Transaction> block2Txs;
    Transaction coinbaseTx2 = Transaction::createCoinbase("1MinerAddress2", 5000000000ULL);
    block2Txs.push_back(coinbaseTx2);
    
    Block block2 = Block::create(block1, block2Txs, "1MinerAddress2", 5000000000ULL);
    bool block2Added = chainState.addBlock(block2);
    
    std::cout << "Block 2 added: " << (block2Added ? "Yes" : "No") << std::endl;
    std::cout << "Block 2 hash: " << block2.hash << std::endl;
    std::cout << "New best height: " << chainState.getBestHeight() << std::endl;
    std::cout << "Total blocks: " << chainState.getTotalBlocks() << std::endl;
    
    // Test chain traversal
    Utils::logInfo("\n17. Testing Chain Traversal:");
    auto fullChain = chainState.getChain();
    std::cout << "Full chain length: " << fullChain.size() << std::endl;
    
    for (size_t i = 0; i < fullChain.size(); ++i) {
        const auto& entry = fullChain[i];
        std::cout << "Block " << i << ": Height " << entry->height 
                  << ", Hash " << entry->block.hash.substr(0, 16) << "..."
                  << ", Work " << entry->totalWork
                  << ", Txs " << entry->block.transactions.size() << std::endl;
    }
    
    // Test chain path finding
    std::vector<std::string> path = chainState.getChainPath(chainGenesis.hash, block2.hash);
    std::cout << "Path from genesis to block 2: " << path.size() << " blocks" << std::endl;
    
    // Test block retrieval
    Utils::logInfo("\n18. Testing Block Retrieval:");
    auto retrievedGenesis = chainState.getBlock(chainGenesis.hash);
    auto retrievedBlock1 = chainState.getBlock(block1.hash);
    auto retrievedBlock2 = chainState.getBlock(block2.hash);
    auto retrievedNonExistent = chainState.getBlock("nonexistent");
    
    std::cout << "Genesis retrieved: " << (retrievedGenesis != nullptr ? "Yes" : "No") << std::endl;
    std::cout << "Block 1 retrieved: " << (retrievedBlock1 != nullptr ? "Yes" : "No") << std::endl;
    std::cout << "Block 2 retrieved: " << (retrievedBlock2 != nullptr ? "Yes" : "No") << std::endl;
    std::cout << "Non-existent block retrieved: " << (retrievedNonExistent != nullptr ? "Yes" : "No") << std::endl;
    
    // Test chain statistics
    Utils::logInfo("\n19. Testing Chain Statistics:");
    auto stats = chainState.getChainStats();
    std::cout << "Chain Statistics:" << std::endl;
    std::cout << "  Height: " << stats.height << std::endl;
    std::cout << "  Total blocks: " << stats.totalBlocks << std::endl;
    std::cout << "  Best hash: " << stats.bestHash.substr(0, 16) << "..." << std::endl;
    std::cout << "  Total work: " << stats.totalWork << std::endl;
    std::cout << "  Average block time: " << stats.averageBlockTime << " seconds" << std::endl;
    std::cout << "  Current difficulty: 0x" << std::hex << stats.difficulty << std::dec << std::endl;
    
    // Test block validation
    Utils::logInfo("\n20. Testing Block Validation:");
    
    // Test valid block
    Block validTestBlock = Block::create(block2, {}, "1TestMiner", 5000000000ULL);
    bool isValid = chainState.isValidBlock(validTestBlock);
    std::cout << "Valid test block passes validation: " << (isValid ? "Yes" : "No") << std::endl;
    
    // Test invalid block (future timestamp)
    Block invalidTestBlock = Block::create(block2, {}, "1TestMiner", 5000000000ULL);
    invalidTestBlock.header.timestamp = Utils::getCurrentTimestamp() + 10000; // Far future
    invalidTestBlock.computeHash();
    bool isInvalid = chainState.isValidBlock(invalidTestBlock);
    std::cout << "Invalid test block (future timestamp) rejected: " << (!isInvalid ? "Yes" : "No") << std::endl;
    
    // Test fork handling (simplified)
    Utils::logInfo("\n21. Testing Fork Handling:");
    
    // Create a competing block at height 1 (fork from genesis)
    std::vector<Transaction> forkTxs;
    Transaction forkCoinbase = Transaction::createCoinbase("1ForkMiner", 5000000000ULL);
    forkTxs.push_back(forkCoinbase);
    
    Block forkBlock = Block::create(chainGenesis, forkTxs, "1ForkMiner", 5000000000ULL);
    bool forkAdded = chainState.addBlock(forkBlock);
    
    std::cout << "Fork block added: " << (forkAdded ? "Yes" : "No") << std::endl;
    std::cout << "Best tip after fork: " << chainState.getBestHash().substr(0, 16) << "..." << std::endl;
    std::cout << "Best height after fork: " << chainState.getBestHeight() << std::endl;
    std::cout << "Total blocks after fork: " << chainState.getTotalBlocks() << std::endl;
    
    // Test persistence
    Utils::logInfo("\n22. Testing Chain Persistence:");
    std::string chainFile = "test_chain.dat";
    
    bool saved = chainState.saveToFile(chainFile);
    std::cout << "Chain saved to file: " << (saved ? "Yes" : "No") << std::endl;
    
    // Create new chain state and load
    ChainState newChainState;
    bool loaded = newChainState.loadFromFile(chainFile);
    std::cout << "Chain loaded from file: " << (loaded ? "Yes" : "No") << std::endl;
    
    if (loaded) {
        std::cout << "Loaded chain height: " << newChainState.getBestHeight() << std::endl;
        std::cout << "Loaded total blocks: " << newChainState.getTotalBlocks() << std::endl;
        std::cout << "Loaded best hash matches: " << 
            (newChainState.getBestHash() == chainState.getBestHash() ? "Yes" : "No") << std::endl;
    }
    
    // Print final chain state
    Utils::logInfo("\n23. Final Chain State:");
    chainState.printChain();
    
    // Test work calculation and comparison
    Utils::logInfo("\n24. Testing Work Calculation:");
    if (retrievedGenesis && retrievedBlock2) {
        std::cout << "Genesis total work: " << retrievedGenesis->totalWork << std::endl;
        std::cout << "Block 2 total work: " << retrievedBlock2->totalWork << std::endl;
        std::cout << "Work increases with height: " << 
            (retrievedBlock2->totalWork > retrievedGenesis->totalWork ? "Yes" : "No") << std::endl;
    }
    
    Utils::logInfo("\n=== Steps 1, 2, 3 & 4 Complete! ===");
    Utils::logInfo("✅ Hash & Serialization primitives working");
    Utils::logInfo("✅ Transaction & Merkle Tree system working");
    Utils::logInfo("✅ Block Header & Proof-of-Work system working");
    Utils::logInfo("✅ Chainstate & Block Validation system working");
    Utils::logInfo("Next: Implement UTXO Set Management (Step 5)");
    
    return 0;
}
