# Pragma Bitcoin-like Blockchain System

A complete Bitcoin-like blockchain implementation with synchronized multi-node support, mining, transactions, and full RPC compatibility.

## 🚀 Features

### Core Blockchain Features
- **Synchronized Multi-Node Network**: Multiple nodes with shared blockchain state
- **Bitcoin-like Mining**: Proof-of-work mining with configurable block rewards (default 50 BTC)
- **Transaction Broadcasting**: Real-time transaction propagation across network
- **Balance Synchronization**: Consistent wallet balances across all nodes
- **State Persistence**: Automatic save/load of blockchain and wallet state
- **Mempool Management**: Transaction pool with fee-based prioritization

### Network & P2P
- **P2P Networking**: Node discovery and peer-to-peer communication
- **Automatic Synchronization**: New nodes sync with existing network
- **Real-time Broadcasting**: Immediate propagation of blocks and transactions
- **Peer Management**: Connect/disconnect from network peers

### RPC API
- **Full Bitcoin-compatible RPC**: Standard Bitcoin RPC methods
- **Wallet Operations**: Address generation, balance queries, transactions
- **Mining Control**: Start/stop mining, get mining information
- **Blockchain Queries**: Block and transaction lookups
- **Network Information**: Peer status, network statistics

## 📁 Project Structure

```
pragma/
├── src/
│   ├── core/
│   │   ├── synchronized_blockchain.h/cpp  # Main blockchain manager
│   │   ├── chainstate.h/cpp              # Blockchain state management
│   │   ├── mempool.h                     # Transaction pool
│   │   └── types.h                       # Core data structures
│   ├── network/
│   │   ├── p2p.h                         # P2P networking (advanced)
│   │   └── p2p_simple.h                  # Simplified P2P for demo
│   ├── rpc/
│   │   ├── rpc.h                         # RPC server framework
│   │   └── synchronized_rpc.h            # Synchronized RPC implementation
│   └── wallet/
│       └── wallet_manager.h              # Wallet management
├── pragma_node.cpp                       # Main application
├── test_blockchain.sh                    # Comprehensive test script
├── Makefile                              # Build configuration
└── README.md                             # This file
```

## 🛠 Building the System

### Prerequisites
- C++17 compatible compiler (g++, clang++)
- POSIX-compliant system (Linux, macOS)
- curl (for testing RPC calls)
- jq (optional, for JSON formatting)

### Build Instructions

```bash
# Clone or navigate to the project directory
cd project-pragma

# Build the application
make clean && make

# The executable will be created as 'pragma_node'
```

## 🚀 Running Nodes

### Single Node Mode
```bash
# Run a single node on default ports
./pragma_node single

# RPC: localhost:8332
# P2P: localhost:8333
```

### Multi-Node Mode
```bash
# Run two connected nodes
./pragma_node multi

# Node 1: RPC 8332, P2P 8333
# Node 2: RPC 8334, P2P 8335
```

### Custom Node Configuration
```bash
# Run custom node
./pragma_node node <nodeId> <rpcPort> <p2pPort>

# Example:
./pragma_node node mynode 8336 8337
```

### Connect to Existing Network
```bash
# Connect to existing nodes
./pragma_node connect <nodeId> <rpcPort> <p2pPort> <seed1:port1> [seed2:port2] ...

# Example:
./pragma_node connect node2 8336 8337 127.0.0.1:8333
```

## 📡 RPC API Usage

### Authentication
All RPC calls use HTTP Basic Authentication:
- Username: `pragma`
- Password: `local`

### Basic RPC Call Format
```bash
curl -u pragma:local -H "Content-Type: application/json" \
     -d '{"method":"<method>","params":[<params>],"id":1}' \
     http://localhost:<port>
```

### Essential RPC Commands

#### Node Information
```bash
# Get general node info
curl -u pragma:local -d '{"method":"getinfo"}' http://localhost:8332

# Get blockchain information
curl -u pragma:local -d '{"method":"getblockchaininfo"}' http://localhost:8332

# Get network information
curl -u pragma:local -d '{"method":"getnetworkinfo"}' http://localhost:8332
```

#### Wallet Operations
```bash
# Generate new address
curl -u pragma:local -d '{"method":"getnewaddress"}' http://localhost:8332

# Get total wallet balance
curl -u pragma:local -d '{"method":"getbalance"}' http://localhost:8332

# Get balance for specific address
curl -u pragma:local -d '{"method":"getaddressbalance","params":["<address>"]}' http://localhost:8332

# List all addresses with balances
curl -u pragma:local -d '{"method":"listaddresses"}' http://localhost:8332
```

#### Transactions
```bash
# Send transaction
curl -u pragma:local -d '{"method":"sendtoaddress","params":["<from_address>","<to_address>",<amount>]}' http://localhost:8332

# Get transaction details
curl -u pragma:local -d '{"method":"gettransaction","params":["<tx_hash>"]}' http://localhost:8332
```

#### Mining
```bash
# Start mining to address
curl -u pragma:local -d '{"method":"startmining","params":["<miner_address>"]}' http://localhost:8332

# Stop mining
curl -u pragma:local -d '{"method":"stopmining"}' http://localhost:8332

# Get mining information
curl -u pragma:local -d '{"method":"getmininginfo"}' http://localhost:8332
```

#### Blockchain Queries
```bash
# Get current block count
curl -u pragma:local -d '{"method":"getblockcount"}' http://localhost:8332

# Get best block hash
curl -u pragma:local -d '{"method":"getbestblockhash"}' http://localhost:8332

# Get block by hash
curl -u pragma:local -d '{"method":"getblock","params":["<block_hash>"]}' http://localhost:8332

# Get block hash by height
curl -u pragma:local -d '{"method":"getblockhash","params":[<height>]}' http://localhost:8332
```

#### Network Management
```bash
# Connect to peer
curl -u pragma:local -d '{"method":"addnode","params":["<address:port>","add"]}' http://localhost:8332

# Get peer information
curl -u pragma:local -d '{"method":"getpeerinfo"}' http://localhost:8332
```

## 🧪 Testing the System

### Automated Test Suite
```bash
# Run comprehensive test script
./test_blockchain.sh
```

This script demonstrates:
- Multi-node synchronization
- Mining rewards
- Transaction broadcasting
- Balance synchronization
- State persistence
- All RPC functionality

### Manual Testing Workflow

1. **Start Nodes**:
   ```bash
   ./pragma_node multi
   ```

2. **Create Addresses**:
   ```bash
   # Node 1
   ALICE=$(curl -s -u pragma:local -d '{"method":"getnewaddress"}' http://localhost:8332 | jq -r '.result')
   BOB=$(curl -s -u pragma:local -d '{"method":"getnewaddress"}' http://localhost:8332 | jq -r '.result')
   ```

3. **Start Mining**:
   ```bash
   curl -u pragma:local -d "{\"method\":\"startmining\",\"params\":[\"$ALICE\"]}" http://localhost:8332
   ```

4. **Wait for Blocks** (mining happens automatically)

5. **Check Balances on Both Nodes**:
   ```bash
   # Node 1
   curl -u pragma:local -d "{\"method\":\"getaddressbalance\",\"params\":[\"$ALICE\"]}" http://localhost:8332
   
   # Node 2 (should be identical)
   curl -u pragma:local -d "{\"method\":\"getaddressbalance\",\"params\":[\"$ALICE\"]}" http://localhost:8334
   ```

6. **Send Transaction**:
   ```bash
   curl -u pragma:local -d "{\"method\":\"sendtoaddress\",\"params\":[\"$ALICE\",\"$BOB\",25.0]}" http://localhost:8332
   ```

7. **Verify Synchronization** (check both nodes again)

## 🏗 Architecture Overview

### Synchronized Blockchain Manager
The heart of the system, managing:
- Shared blockchain state across nodes
- Thread-safe operations with mutex protection
- Real-time synchronization
- Mining coordination
- Transaction broadcasting

### Key Components

#### ChainState
- Manages blockchain structure
- Validates blocks and transactions
- Handles chain reorganization
- Maintains best chain tip

#### Mempool
- Transaction pool management
- Fee-based prioritization
- Validation and broadcasting
- Memory optimization

#### WalletManager
- Address generation and management
- Balance tracking
- Transaction creation
- Persistent storage

#### P2P Network
- Peer discovery and management
- Block and transaction broadcasting
- Network synchronization
- Connection handling

#### RPC Server
- Bitcoin-compatible API
- JSON-RPC protocol
- Authentication and security
- Comprehensive method coverage

## 🔧 Configuration

### Node Configuration
```cpp
SynchronizedBlockchainManager::NodeConfig config;
config.nodeId = "node1";
config.rpcPort = 8332;
config.p2pPort = 8333;
config.dataDir = "./pragma_data";
config.enableMining = true;
config.blockReward = 50.0;
config.seedNodes = {"127.0.0.1:8333"};
```

### Mining Configuration
- **Block Reward**: 50 BTC (configurable)
- **Difficulty**: 1 (simplified for demo)
- **Block Time**: Variable (based on transaction volume)
- **Automatic Start**: Configurable per node

### Network Configuration
- **Default RPC Port**: 8332
- **Default P2P Port**: 8333
- **Max Connections**: Unlimited (simplified)
- **Protocol**: Custom Bitcoin-like protocol

## 🚨 Bitcoin-like Behavior

This system implements core Bitcoin features:

### ✅ Implemented Features
- **Decentralized Network**: Multiple independent nodes
- **Proof of Work**: Mining with nonce-based difficulty
- **UTXO Model**: Transaction inputs/outputs (simplified)
- **Block Chain**: Linked blocks with previous hash references
- **Consensus**: Longest chain rule
- **Transaction Broadcasting**: P2P propagation
- **Wallet Functionality**: Address generation, balance tracking
- **RPC Interface**: Bitcoin-compatible API
- **Persistence**: Blockchain and wallet state storage

### 📋 Key Differences from Bitcoin
- **Simplified Mining**: Basic proof-of-work (for demonstration)
- **No Script Engine**: Simplified transaction validation
- **Centralized Networking**: Simplified P2P (production would use full networking)
- **No SPV**: Full nodes only
- **Fixed Supply**: No halving implemented

### 🔄 Transaction Flow
1. User creates transaction via RPC
2. Transaction validated and added to mempool
3. Transaction broadcast to network peers
4. Miner includes transaction in new block
5. Block mined and broadcast to network
6. All nodes validate and add block to chain
7. Balances updated across all nodes

### 🔗 Synchronization Process
1. New node connects to seed nodes
2. Requests blockchain from peers
3. Downloads and validates all blocks
4. Catches up to network tip
5. Participates in real-time sync
6. Maintains consistent state with network

## 🧰 Development and Debugging

### Logging
The system provides extensive logging:
- Node startup and shutdown
- Block mining and validation
- Transaction processing
- Network events
- RPC calls and responses

### Debug Commands
```bash
# Check node status
curl -u pragma:local -d '{"method":"ping"}' http://localhost:8332

# Get detailed info
curl -u pragma:local -d '{"method":"getinfo"}' http://localhost:8332
```

### State Inspection
- Blockchain state: `pragma_data_<node>/chainstate.dat`
- Wallet state: `pragma_data_<node>/wallet.dat`
- Logs: Console output with timestamps

## 🤝 Contributing

### Code Structure
- Follow existing naming conventions
- Use RAII and modern C++ practices
- Maintain thread safety with mutexes
- Add comprehensive error handling

### Testing
- Add test cases to `test_blockchain.sh`
- Verify multi-node synchronization
- Test edge cases and error conditions
- Validate RPC API compatibility

## 📄 License

This project is for educational and demonstration purposes. It implements Bitcoin-like functionality for learning blockchain concepts.

## 🙋‍♂️ Support

For questions or issues:
1. Check the test script output for common scenarios
2. Review RPC API documentation above
3. Examine log output for debugging information
4. Verify node connectivity and synchronization

---

**🎉 Welcome to the Pragma Bitcoin-like Blockchain System!**

This implementation demonstrates core blockchain concepts with real multi-node synchronization, mining, and transaction processing. Perfect for learning how Bitcoin-like systems work under the hood!me—let’s build a **minimal blockchain in C++ with a real consensus system (Proof-of-Work)** and a clean path to extend later. Below is a **step-by-step roadmap** with concrete tasks, key data structures, and snippets you can drop in.

---

# 0 Tech Setup

* **Build**: CMake
* **Hashing & crypto**: OpenSSL (SHA-256, ECDSA) or Crypto++
* **Networking**: Boost.Asio (TCP)
* **Storage**: LevelDB/RocksDB (or start with flat files)
* **Logging/Tests**: spdlog + GoogleTest (optional but recommended)

---

# 1 Core Data Structures

### 1.1 Block Header & Block

```cpp
struct BlockHeader {
    uint32_t version;
    std::string prevHash;   // 32-byte hex
    std::string merkleRoot; // 32-byte hex
    uint64_t timestamp;     // unix
    uint32_t bits;          // compact target
    uint32_t nonce;
};

struct Block {
    BlockHeader header;
    std::vector<std::string> txids; // or full transactions
    std::string hash;               // double SHA-256(header)
};
```

### 1.2 Transactions (UTXO model)

```cpp
struct TxOut { uint64_t value; std::string pubKeyHash; };
struct OutPoint { std::string txid; uint32_t index; };

struct TxIn {
    OutPoint prevout;
    std::string sig;       // DER-encoded ECDSA
    std::string pubKey;    // for validation
};

struct Transaction {
    std::vector<TxIn> vin;
    std::vector<TxOut> vout;
    bool isCoinbase;       // coinbase has no vin
    std::string txid;      // hash of serialized tx
};
```

### 1.3 UTXO Set

* Map from `OutPoint` → `TxOut`.
* Stored on disk (LevelDB); cached in memory.

---

# 2 Hashing & Merkle

### 2.1 Double SHA-256

```cpp
std::string dbl_sha256(const std::string& bytes);
```

### 2.2 Merkle Root

* Build pairwise hashes of txids until one root remains.
* If odd count, duplicate last hash for that level.

---

# 3 Proof-of-Work (Consensus)

### 3.1 Target / Bits

* Represent difficulty target in **compact** `bits` (like Bitcoin), or start simple with **N leading zeros** and later switch to compact.

### 3.2 Mining Loop

```cpp
bool meetsTarget(const BlockHeader& h);  // compare dbl_sha256(header) to target
void mine(Block& b) {
    for (uint64_t n = 0; ; ++n) {
        b.header.nonce = n;
        b.hash = hashHeader(b.header);
        if (meetsTarget(b.header)) break;
        if (n % 1000000 == 0) /* yield/log */;
    }
}
```

### 3.3 Difficulty Adjustment (every `DIFF_WINDOW` blocks)

* Target block time: e.g., **30 seconds** (tweakable).
* Every `N` blocks:

  * `actualTime = time(last) - time(N-back)`
  * `newTarget = oldTarget * actualTime / (N * targetBlockTime)`
  * Clamp to avoid >4× jumps per window.

### 3.4 Fork Choice Rule

* **Heaviest chain rule**: choose the chain with **most cumulative work** (sum of `1/target` per block), not just longest length.

---

# 4 Validation Rules

### 4.1 Block Checks

* Header hash ≤ target.
* `prevHash` links to known block.
* `timestamp` ≤ median of last 11 blocks + drift limit.
* 1 coinbase tx only; coinbase reward = subsidy + fees.
* Merkle root matches txids.

### 4.2 Tx Checks

* Each `TxIn.prevout` exists in UTXO set and is unspent.
* Signatures verify (ECDSA over tx digest).
* Sum(inputs) ≥ Sum(outputs); fee = diff (nonnegative).
* No double spends within block.
* Update UTXO: spend inputs, add outputs.

### 4.3 Reorg Handling

* Maintain side-chains. If a side-chain’s cumulative work exceeds the active chain:

  * **Disconnect** blocks to fork point (rollback UTXO changes),
  * **Connect** new chain (apply UTXO changes).

---

# 5 Mempool & Mining

### 5.1 Mempool

* Validate incoming transactions; if valid against current UTXO → store in mempool.
* Evict if too big (by fee/size).

### 5.2 Block Template

* Start with coinbase (address = miner key).
* Select top-fee transactions from mempool (respect dependencies).
* Build merkle root, fill header, mine.

---

# 6 P2P Networking (Minimal)

### 6.1 Messages

* `version` / `verack` (handshake)
* `inv` (announce tx/block hashes)
* `getdata` (request bodies)
* `tx`, `block` (objects)
* `getheaders` / `headers` (sync)
* `ping` / `pong`

### 6.2 Flow

1. Connect peers (Boost.Asio).
2. Exchange `version` → `verack`.
3. Header sync with `getheaders`.
4. Request missing blocks with `getdata`.
5. Relay: on new tx/block → send `inv`; peers ask `getdata`.

### 6.3 DoS Guard (basic)

* Per-peer rate limits, duplicate suppression, ban score for invalids.

---

# 7 Persistence

* **Blocks**: append-only files (`blkNNNN.dat`) + index (hash → file offset).
* **UTXO**: LevelDB (key: `txid:index`, value: `TxOut`).
* **Chainstate**: tip hash, cumulative work, height.

---

# 8 CLI / Node Modes

* `node --datadir ... --port ...`
* Commands (RPC or simple CLI):

  * `getblockchaininfo`, `getblock <hash>`, `getmempoolinfo`
  * `sendtoaddress <addr> <amount>`
  * `getnewaddress`, `getbalance`
  * `generate <n>` (mine N blocks)

---

# 9 Security Basics

* ECDSA keys (secp256k1). Store encrypted (password + PBKDF2).
* Reject **non-standard** scripts initially (start with P2PKH only).
* Strict serialization & input length checks to avoid crashes.

---

# 10 Step-By-Step Build Order (do this in sequence)

1. **Serialize/Hash Primitives**

   * Byte buffers, varints, double-SHA256, hex utils.
   * Unit tests.

2. **Tx & Merkle**

   * Serialize tx; compute `txid`.
   * Merkle tree builder + tests.

3. **Block Header & PoW**

   * Header hash, target comparison, simple mining loop.
   * Command: `generate 1` creates a genesis child.

4. **Block & Chainstate**

   * In-memory chain indexed by hash; store height + cumulative work.
   * Validate/link blocks; persist to disk.

5. **UTXO Set**

   * Apply/undo transactions.
   * Coinbase creation & subsidy schedule.
   * Tests for spend/undo.

6. **Full Block Validation**

   * All rules in §4; reject bad blocks.

7. **Mempool**

   * Validate tx against UTXO; store with fee index.
   * Tx selection policy.

8. **Miner**

   * Build block template from mempool + coinbase.
   * PoW loop with interrupt on new best tip.

9. **Difficulty Retarget**

   * Implement windowed retargeting; test with synthetic timestamps.

10. **P2P**

* Handshake; headers sync; block/tx relay.
* Reorg logic with side-chains.
* Basic DoS protections.

11. **Wallet (Keys & Addresses)**

* Generate secp256k1 keypairs; P2PKH address encoding (Base58Check).
* `sendtoaddress` crafts tx, signs inputs.

12. **Polish & Tooling**

* Logging, metrics (hashrate, mempool size).
* Snapshot/restore, config file, graceful shutdown.

---

# 11 Minimal Snippets You’ll Need

### 11.1 Compact Target → Big Integer

```cpp
// Parse "bits" to 256-bit target (use a bigint lib or custom)
BigInt targetFromBits(uint32_t bits);
bool meetsTarget(const BlockHeader& h) {
    auto H = BigInt::from_hex(dbl_sha256(serializeHeader(h)));
    return H <= targetFromBits(h.bits);
}
```

### 11.2 Cumulative Work (for fork choice)

```cpp
// work = floor(2^256 / (target+1))
BigInt blockWork(const BigInt& target);
chainTip.cumWork = prev.cumWork + blockWork(targetFromBits(header.bits));
```

### 11.3 Coinbase Reward (simple halving schedule)

```cpp
uint64_t subsidyAtHeight(uint32_t height) {
    uint64_t base = 50'0000'0000ULL; // 50 coins, 8 decimals
    uint32_t halvings = height / 210000;
    return base >> halvings;
}
```

---

# 12 Testing Playbook

* **Unit**: hash, merkle, tx validity, UTXO apply/undo, target math.
* **Integration**:

  * Mine a few blocks, spend coinbase after maturity, reorg to longer chain.
  * Two nodes: start one behind, sync headers+blocks; verify mempool relay.
* **Adversarial**:

  * Invalid sigs, overspend, duplicate spends, wrong merkle root, bad timestamp, low work headers.

---

# 13 Extensions (after PoW works)

* **Proof-of-Stake**: validator set, stake selection, slashing.
* **PBFT/Raft** (private chains): leader election, round-based commits.
* **Scripts**: basic Script interpreter (OP\_DUP, OP\_HASH160, OP\_EQUALVERIFY, OP\_CHECKSIG).
* **Pruning**: discard old block bodies after checkpoints.
* **Light clients**: headers-only mode + merkle proofs.

---

