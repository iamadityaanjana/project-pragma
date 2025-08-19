#include <iostream>
#include "primitives/hash.h"
#include "primitives/serialize.h"
#include "primitives/utils.h"
#include "core/transaction.h"
#include "core/merkle.h"

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
    Utils::logInfo("Next: Implement Block Header & Proof-of-Work structures");
    
    return 0;
}
