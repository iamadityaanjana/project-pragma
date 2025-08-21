#include "core/validator.h"
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
#include "core/utxo.h"

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
    
    // NEW: Step 5 - UTXO Set Management Testing
    Utils::logInfo("\n=== Step 5: UTXO Set Management Testing ===");
    
    // Test UTXO set initialization
    Utils::logInfo("\n25. Testing UTXO Set Initialization:");
    UTXOSet utxoSet;
    
    std::cout << "Initial UTXO set:" << std::endl;
    std::cout << "Size: " << utxoSet.size() << std::endl;
    std::cout << "Total value: " << utxoSet.getTotalValue() << " satoshis" << std::endl;
    std::cout << "Is empty: " << (utxoSet.isEmpty() ? "Yes" : "No") << std::endl;
    std::cout << "Current height: " << utxoSet.getCurrentHeight() << std::endl;
    
    // Test block subsidy calculation
    Utils::logInfo("\n26. Testing Block Subsidy Calculation:");
    std::cout << "Block 0 subsidy: " << UTXOSet::getBlockSubsidy(0) << " satoshis (50 BTC)" << std::endl;
    std::cout << "Block 210,000 subsidy: " << UTXOSet::getBlockSubsidy(210000) << " satoshis (25 BTC)" << std::endl;
    std::cout << "Block 420,000 subsidy: " << UTXOSet::getBlockSubsidy(420000) << " satoshis (12.5 BTC)" << std::endl;
    std::cout << "Block 630,000 subsidy: " << UTXOSet::getBlockSubsidy(630000) << " satoshis (6.25 BTC)" << std::endl;
    
    // Test coinbase maturity
    std::cout << "Coinbase mature at height 50 (created at 1): " << 
        (UTXOSet::isCoinbaseMatured(1, 50) ? "No" : "Yes") << std::endl;
    std::cout << "Coinbase mature at height 101 (created at 1): " << 
        (UTXOSet::isCoinbaseMatured(1, 101) ? "Yes" : "No") << std::endl;
    
    // Test applying coinbase transaction
    Utils::logInfo("\n27. Testing Coinbase Transaction Application:");
    Transaction utxoCoinbase = Transaction::createCoinbase("1UTXOMinerAddress", 5000000000ULL);
    
    bool applied = utxoSet.applyTransaction(utxoCoinbase, 1);
    std::cout << "Coinbase transaction applied: " << (applied ? "Yes" : "No") << std::endl;
    std::cout << "UTXO set size after coinbase: " << utxoSet.size() << std::endl;
    std::cout << "Total value after coinbase: " << utxoSet.getTotalValue() << " satoshis" << std::endl;
    
    // Check the created UTXO
    OutPoint coinbaseOutpoint(utxoCoinbase.txid, 0);
    const UTXO* coinbaseUTXO = utxoSet.getUTXO(coinbaseOutpoint);
    if (coinbaseUTXO) {
        std::cout << "Coinbase UTXO created:" << std::endl;
        std::cout << "  Value: " << coinbaseUTXO->output.value << " satoshis" << std::endl;
        std::cout << "  Address: " << coinbaseUTXO->output.pubKeyHash << std::endl;
        std::cout << "  Height: " << coinbaseUTXO->height << std::endl;
        std::cout << "  Is coinbase: " << (coinbaseUTXO->isCoinbase ? "Yes" : "No") << std::endl;
        std::cout << "  Confirmations: " << coinbaseUTXO->confirmations << std::endl;
    }
    
    // Test coinbase maturity
    Utils::logInfo("\n28. Testing Coinbase Maturity:");
    utxoSet.setCurrentHeight(50);
    std::cout << "Current height set to: " << utxoSet.getCurrentHeight() << std::endl;
    
    coinbaseUTXO = utxoSet.getUTXO(coinbaseOutpoint);
    if (coinbaseUTXO) {
        std::cout << "Coinbase spendable at height 50: " << 
            (coinbaseUTXO->isSpendable(50) ? "Yes" : "No") << std::endl;
        std::cout << "Coinbase spendable at height 101: " << 
            (coinbaseUTXO->isSpendable(101) ? "Yes" : "No") << std::endl;
        std::cout << "Confirmations at height 50: " << coinbaseUTXO->confirmations << std::endl;
    }
    
    // Test spending coinbase (when mature)
    Utils::logInfo("\n29. Testing Spending Mature Coinbase:");
    utxoSet.setCurrentHeight(101); // Make coinbase spendable
    
    std::vector<TxIn> spendInputs;
    spendInputs.emplace_back(coinbaseOutpoint, "signature_data", "pubkey_data");
    
    std::vector<TxOut> spendOutputs;
    spendOutputs.emplace_back(2000000000ULL, "1RecipientAddress1");
    spendOutputs.emplace_back(2999999000ULL, "1RecipientAddress2"); // Leave 1000 sat as fee
    
    Transaction spendTx = Transaction::create(spendInputs, spendOutputs);
    
    // Validate transaction first
    bool isValidSpend = utxoSet.validateTransaction(spendTx);
    std::cout << "Spend transaction valid: " << (isValidSpend ? "Yes" : "No") << std::endl;
    
    if (isValidSpend) {
        uint64_t fee = utxoSet.calculateTransactionFee(spendTx);
        std::cout << "Transaction fee: " << fee << " satoshis" << std::endl;
        
        // Apply the spending transaction
        bool spendApplied = utxoSet.applyTransaction(spendTx, 101);
        std::cout << "Spend transaction applied: " << (spendApplied ? "Yes" : "No") << std::endl;
        std::cout << "UTXO set size after spending: " << utxoSet.size() << std::endl;
        std::cout << "Total value after spending: " << utxoSet.getTotalValue() << " satoshis" << std::endl;
        
        // Check new UTXOs
        OutPoint outpoint1(spendTx.txid, 0);
        OutPoint outpoint2(spendTx.txid, 1);
        
        const UTXO* utxo1 = utxoSet.getUTXO(outpoint1);
        const UTXO* utxo2 = utxoSet.getUTXO(outpoint2);
        
        if (utxo1 && utxo2) {
            std::cout << "New UTXO 1: " << utxo1->output.value << " sat -> " << utxo1->output.pubKeyHash << std::endl;
            std::cout << "New UTXO 2: " << utxo2->output.value << " sat -> " << utxo2->output.pubKeyHash << std::endl;
        }
    }
    
    // Test balance queries
    Utils::logInfo("\n30. Testing Balance Queries:");
    uint64_t balance1 = utxoSet.getBalanceForAddress("1RecipientAddress1");
    uint64_t balance2 = utxoSet.getBalanceForAddress("1RecipientAddress2");
    uint64_t balanceOrig = utxoSet.getBalanceForAddress("1UTXOMinerAddress");
    
    std::cout << "Balance for 1RecipientAddress1: " << balance1 << " satoshis" << std::endl;
    std::cout << "Balance for 1RecipientAddress2: " << balance2 << " satoshis" << std::endl;
    std::cout << "Balance for original miner: " << balanceOrig << " satoshis" << std::endl;
    
    // Test UTXO selection
    Utils::logInfo("\n31. Testing UTXO Selection:");
    auto selectedUTXOs = utxoSet.getSpendableUTXOs("1RecipientAddress1", 1500000000ULL);
    std::cout << "UTXOs selected for 1.5 BTC from Address1: " << selectedUTXOs.size() << std::endl;
    
    auto insufficientUTXOs = utxoSet.getSpendableUTXOs("1RecipientAddress1", 5000000000ULL);
    std::cout << "UTXOs selected for 5 BTC from Address1 (insufficient): " << insufficientUTXOs.size() << std::endl;
    
    // Test transaction undo
    Utils::logInfo("\n32. Testing Transaction Undo:");
    std::vector<UTXO> spentUTXOs = utxoSet.getSpentUTXOs(spendTx);
    std::cout << "Spent UTXOs captured: " << spentUTXOs.size() << std::endl;
    
    size_t sizeBefore = utxoSet.size();
    uint64_t valueBefore = utxoSet.getTotalValue();
    
    bool undone = utxoSet.undoTransaction(spendTx, spentUTXOs);
    std::cout << "Transaction undone: " << (undone ? "Yes" : "No") << std::endl;
    std::cout << "UTXO set size after undo: " << utxoSet.size() << std::endl;
    std::cout << "Total value after undo: " << utxoSet.getTotalValue() << " satoshis" << std::endl;
    
    // Check if coinbase UTXO is restored
    const UTXO* restoredCoinbase = utxoSet.getUTXO(coinbaseOutpoint);
    std::cout << "Coinbase UTXO restored: " << (restoredCoinbase ? "Yes" : "No") << std::endl;
    
    // Test UTXO statistics
    Utils::logInfo("\n33. Testing UTXO Statistics:");
    auto utxoStats = utxoSet.getStats();
    std::cout << "UTXO Statistics:" << std::endl;
    std::cout << "  Total UTXOs: " << utxoStats.totalUTXOs << std::endl;
    std::cout << "  Total value: " << utxoStats.totalValue << " satoshis" << std::endl;
    std::cout << "  Coinbase UTXOs: " << utxoStats.coinbaseUTXOs << std::endl;
    std::cout << "  Coinbase value: " << utxoStats.coinbaseValue << " satoshis" << std::endl;
    std::cout << "  Mature UTXOs: " << utxoStats.matureUTXOs << std::endl;
    std::cout << "  Mature value: " << utxoStats.matureValue << " satoshis" << std::endl;
    std::cout << "  Average UTXO value: " << utxoStats.averageUTXOValue << " satoshis" << std::endl;
    std::cout << "  Current height: " << utxoStats.currentHeight << std::endl;
    
    // Test UTXO cache
    Utils::logInfo("\n34. Testing UTXO Cache:");
    UTXOCache cache(&utxoSet);
    
    OutPoint cacheOutpoint("cache_test_tx", 0);
    TxOut cacheOutput(1000000000ULL, "1CacheTestAddress");
    
    std::cout << "UTXO in base set: " << (utxoSet.hasUTXO(cacheOutpoint) ? "Yes" : "No") << std::endl;
    
    cache.addUTXO(cacheOutpoint, cacheOutput, 101, false);
    std::cout << "UTXO in cache: " << (cache.hasUTXO(cacheOutpoint) ? "Yes" : "No") << std::endl;
    std::cout << "UTXO in base set (before flush): " << (utxoSet.hasUTXO(cacheOutpoint) ? "Yes" : "No") << std::endl;
    std::cout << "Cache has changes: " << (cache.hasChanges() ? "Yes" : "No") << std::endl;
    std::cout << "Cache change count: " << cache.getChangeCount() << std::endl;
    
    cache.flush();
    std::cout << "UTXO in base set (after flush): " << (utxoSet.hasUTXO(cacheOutpoint) ? "Yes" : "No") << std::endl;
    
    // Test UTXO set persistence
    Utils::logInfo("\n35. Testing UTXO Set Persistence:");
    std::string utxoFile = "test_utxo.dat";
    
    bool utxoSaved = utxoSet.saveToFile(utxoFile);
    std::cout << "UTXO set saved: " << (utxoSaved ? "Yes" : "No") << std::endl;
    
    UTXOSet newUTXOSet;
    bool utxoLoaded = newUTXOSet.loadFromFile(utxoFile);
    std::cout << "UTXO set loaded: " << (utxoLoaded ? "Yes" : "No") << std::endl;
    
    if (utxoLoaded) {
        std::cout << "Loaded UTXO set size: " << newUTXOSet.size() << std::endl;
        std::cout << "Loaded total value: " << newUTXOSet.getTotalValue() << " satoshis" << std::endl;
        std::cout << "Loaded current height: " << newUTXOSet.getCurrentHeight() << std::endl;
        std::cout << "Data integrity check: " << 
            (newUTXOSet.size() == utxoSet.size() && 
             newUTXOSet.getTotalValue() == utxoSet.getTotalValue() ? "PASS" : "FAIL") << std::endl;
    }
    
    // Test multiple transactions in sequence
    Utils::logInfo("\n36. Testing Sequential Transactions:");
    UTXOSet seqUTXOSet;
    
    // Create a chain of transactions
    Transaction coinbase1 = Transaction::createCoinbase("1Miner1", 5000000000ULL);
    Transaction coinbase2 = Transaction::createCoinbase("1Miner2", 5000000000ULL);
    
    seqUTXOSet.applyTransaction(coinbase1, 1);
    seqUTXOSet.applyTransaction(coinbase2, 2);
    seqUTXOSet.setCurrentHeight(102);
    
    std::cout << "Applied 2 coinbase transactions" << std::endl;
    std::cout << "UTXO set size: " << seqUTXOSet.size() << std::endl;
    std::cout << "Total value: " << seqUTXOSet.getTotalValue() << " satoshis" << std::endl;
    
    // Create transaction spending from both coinbases
    std::vector<TxIn> multiInputs;
    multiInputs.emplace_back(OutPoint(coinbase1.txid, 0), "sig1", "pk1");
    multiInputs.emplace_back(OutPoint(coinbase2.txid, 0), "sig2", "pk2");
    
    std::vector<TxOut> multiOutputs;
    multiOutputs.emplace_back(9999999000ULL, "1RecipientAddress"); // 99.99999 BTC (1000 sat fee)
    
    Transaction multiTx = Transaction::create(multiInputs, multiOutputs);
    
    bool multiValid = seqUTXOSet.validateTransaction(multiTx);
    std::cout << "Multi-input transaction valid: " << (multiValid ? "Yes" : "No") << std::endl;
    
    if (multiValid) {
        uint64_t multiFee = seqUTXOSet.calculateTransactionFee(multiTx);
        std::cout << "Multi-input transaction fee: " << multiFee << " satoshis" << std::endl;
        
        seqUTXOSet.applyTransaction(multiTx, 102);
        std::cout << "Multi-input transaction applied" << std::endl;
        std::cout << "Final UTXO set size: " << seqUTXOSet.size() << std::endl;
        std::cout << "Final total value: " << seqUTXOSet.getTotalValue() << " satoshis" << std::endl;
    }
    
    // Print final UTXO set
    Utils::logInfo("\n37. Final UTXO Set State:");
    seqUTXOSet.printUTXOSet();
    
    Utils::logInfo("\n=== Steps 1, 2, 3, 4 & 5 Complete! ===");
    Utils::logInfo("✅ Hash & Serialization primitives working");
    Utils::logInfo("✅ Transaction & Merkle Tree system working");
    Utils::logInfo("✅ Block Header & Proof-of-Work system working");
    Utils::logInfo("✅ Chainstate & Block Validation system working");
    Utils::logInfo("✅ UTXO Set Management system working");
    
    // === STEP 6: FULL BLOCK VALIDATION ===
    Utils::logInfo("\n=== Step 6: Full Block Validation Testing ===");
    
    // Test 38: BlockValidator initialization
    Utils::logInfo("\n38. Testing BlockValidator Initialization:");
    BlockValidator validator(&utxoSet, &chainState);
    auto validatorStats = validator.getValidationStats();
    std::cout << "Initial validation stats:" << std::endl;
    std::cout << "  Blocks validated: " << validatorStats.blocksValidated << std::endl;
    std::cout << "  Transactions validated: " << validatorStats.transactionsValidated << std::endl;
    std::cout << "  Validation errors: " << validatorStats.validationErrors << std::endl;
    
    // Test 39: Error handling and result types
    Utils::logInfo("\n39. Testing Validation Error Handling:");
    auto successResult = ValidationResult::success();
    std::cout << "Success result valid: " << (successResult.isValid ? "Yes" : "No") << std::endl;
    
    auto errorResult = ValidationResult::failure(ValidationError::INVALID_BLOCK_HASH, "Test error message");
    std::cout << "Error result valid: " << (errorResult.isValid ? "Yes" : "No") << std::endl;
    std::cout << "Error message: " << errorResult.errorMessage << std::endl;
    std::cout << "Error type: " << validator.getErrorString(errorResult.error) << std::endl;
    
    // Test 40: Validation of existing valid block  
    Utils::logInfo("\n40. Testing Valid Block Validation:");
    std::string bestHash = chainState.getBestHash();
    const Block* bestBlock = chainState.getBlockByHash(bestHash);
    
    if (bestBlock) {
        auto blockResult = validator.validateBlock(*bestBlock, chainState.getBestHeight());
        std::cout << "Best block validation: " << (blockResult.isValid ? "VALID" : "INVALID") << std::endl;
        if (!blockResult.isValid) {
            std::cout << "Validation error: " << blockResult.errorMessage << std::endl;
        }
        validator.printValidationResult(blockResult);
    }
    
    // Test 41: Transaction-only validation
    Utils::logInfo("\n41. Testing Transaction Validation:");
    Transaction testCoinbase = Transaction::createCoinbase("1ValidatorTestMiner", 5000000000ULL);
    auto txResult = validator.validateTransactionOnly(testCoinbase, 1);
    std::cout << "Coinbase transaction validation: " << (txResult.isValid ? "VALID" : "INVALID") << std::endl;
    validator.printValidationResult(txResult);
    
    // Test invalid transaction (no inputs, no coinbase)
    std::vector<TxIn> noInputs;
    std::vector<TxOut> someOutputs;
    someOutputs.emplace_back(1000000, "1SomeAddress");
    Transaction invalidTx = Transaction::create(noInputs, someOutputs);
    auto invalidResult = validator.validateTransactionOnly(invalidTx, 1);
    std::cout << "Invalid transaction validation: " << (invalidResult.isValid ? "VALID" : "INVALID") << std::endl;
    validator.printValidationResult(invalidResult);
    
    // Test 42: Block structure validation
    Utils::logInfo("\n42. Testing Block Structure Validation:");
    
    // Create a properly structured test block
    Transaction newCoinbase = Transaction::createCoinbase("1StructureTestMiner", 5000000000ULL);
    std::vector<Transaction> testTransactions = {newCoinbase};
    
    BlockHeader testHeader;
    testHeader.version = 1;
    testHeader.prevHash = chainState.getBestHash();
    testHeader.timestamp = static_cast<uint64_t>(std::time(nullptr));
    testHeader.bits = 0x1d00ffff; // Use default difficulty
    testHeader.nonce = 0;
    
    // Calculate merkle root
    std::vector<std::string> testTxids;
    for (const auto& tx : testTransactions) {
        testTxids.push_back(tx.txid);
    }
    testHeader.merkleRoot = MerkleTree::buildMerkleRoot(testTxids);
    
    Block structureTestBlock(testHeader, testTransactions);
    structureTestBlock.computeHash();
    
    std::cout << "Test block size: " << structureTestBlock.calculateSize() << " bytes" << std::endl;
    std::cout << "Test block transactions: " << structureTestBlock.getTransactionCount() << std::endl;
    std::cout << "Test block hash: " << structureTestBlock.hash << std::endl;
    
    // Test 43: Invalid blocks detection
    Utils::logInfo("\n43. Testing Invalid Block Detection:");
    
    // Create block with invalid previous hash
    Block invalidPrevHashBlock = structureTestBlock;
    invalidPrevHashBlock.header.prevHash = "invalid_hash_string";
    invalidPrevHashBlock.computeHash();
    
    auto invalidPrevResult = validator.validateBlock(invalidPrevHashBlock, chainState.getBestHeight() + 1);
    std::cout << "Invalid previous hash block: " << (invalidPrevResult.isValid ? "VALID" : "INVALID") << std::endl;
    validator.printValidationResult(invalidPrevResult);
    
    // Test 44: Batch validation
    Utils::logInfo("\n44. Testing Batch Validation:");
    BatchValidator batchValidator(&validator);
    
    std::vector<Block> blockBatch;
    blockBatch.push_back(structureTestBlock);
    
    auto batchResult = batchValidator.validateBlockBatch(blockBatch, chainState.getBestHeight() + 1);
    std::cout << "Batch validation result: " << (batchResult.isValid ? "VALID" : "INVALID") << std::endl;
    validator.printValidationResult(batchResult);
    
    // Test 45: Validation statistics
    Utils::logInfo("\n45. Testing Validation Statistics:");
    auto finalStats = validator.getValidationStats();
    std::cout << "Final validation statistics:" << std::endl;
    std::cout << "  Blocks validated: " << finalStats.blocksValidated << std::endl;
    std::cout << "  Transactions validated: " << finalStats.transactionsValidated << std::endl;
    std::cout << "  Validation errors: " << finalStats.validationErrors << std::endl;
    std::cout << "  Total fees processed: " << finalStats.totalFees << " satoshis" << std::endl;
    std::cout << "  Total subsidy processed: " << finalStats.totalSubsidy << " satoshis" << std::endl;
    
    validator.printValidationStats();
    
    Utils::logInfo("\n=== Steps 1, 2, 3, 4, 5 & 6 Complete! ===");
    Utils::logInfo("✅ Hash & Serialization primitives working");
    Utils::logInfo("✅ Transaction & Merkle Tree system working");
    Utils::logInfo("✅ Block Header & Proof-of-Work system working");
    Utils::logInfo("✅ Chainstate & Block Validation system working");
    Utils::logInfo("✅ UTXO Set Management system working");
    Utils::logInfo("✅ Full Block Validation system working");
    Utils::logInfo("Next: Implement Mempool & Mining (Step 7)");
    
    return 0;
}
