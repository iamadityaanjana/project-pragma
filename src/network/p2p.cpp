#include "p2p.h"
#include "peer.h"
#include "protocol.h"
#include "../core/block.h"
#include "../core/transaction.h"
#include "../core/chainstate.h"
#include "../core/mempool.h"
#include "../primitives/utils.h"
#include <iostream>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <random>

namespace pragma {

std::string messageTypeToString(MessageType type) {
    switch(type) {
        case MessageType::VERSION: return "VERSION";
        case MessageType::VERACK: return "VERACK";
        case MessageType::INV: return "INV";
        case MessageType::GETDATA: return "GETDATA";
        case MessageType::TX: return "TX";
        case MessageType::BLOCK: return "BLOCK";
        case MessageType::PING: return "PING";
        case MessageType::PONG: return "PONG";
        case MessageType::ADDR: return "ADDR";
        case MessageType::REJECT: return "REJECT";
        default: return "UNKNOWN";
    }
}

void parseAddress(const std::string& addressStr, std::string& ip, uint16_t& port) {
    size_t colonPos = addressStr.find(':');
    if (colonPos != std::string::npos) {
        ip = addressStr.substr(0, colonPos);
        port = std::stoi(addressStr.substr(colonPos + 1));
    } else {
        ip = addressStr;
        port = 8333; // Default port
    }
}

bool isValidIP(const std::string& ip) {
    // Simple IP validation
    return !ip.empty() && ip != "0.0.0.0";
}

SyncManager::SyncManager(ChainState* chain, PeerManager* peers)
    : chainState(chain), peerManager(peers), state(SyncState::IDLE), targetHeight(0), currentHeight(0) {}

void SyncManager::startSync() {
    std::lock_guard<std::mutex> lock(syncMutex);
    state = SyncState::HEADERS_SYNC;
    syncStartTime = std::chrono::steady_clock::now();
    updateSyncPeer();
}

void SyncManager::stopSync() {
    std::lock_guard<std::mutex> lock(syncMutex);
    state = SyncState::IDLE;
    syncPeer.clear();
}

bool SyncManager::isSyncing() const {
    return state != SyncState::IDLE && state != SyncState::SYNCED;
}

bool SyncManager::isSynced() const {
    return state == SyncState::SYNCED;
}

SyncState SyncManager::getState() const {
    return state;
}

void SyncManager::requestHeaders(const std::string& peerId) {
    // Send getheaders message to peer
    // ...
}

void SyncManager::requestBlocks(const std::string& peerId, const std::vector<std::string>& hashes) {
    // Send getdata message for blocks
    // ...
}

void SyncManager::handleHeaders(const std::string& peerId, const std::vector<std::string>& headers) {
    // Process received headers
    // ...
}

void SyncManager::handleBlock(const std::string& peerId, const Block& block) {
    // Process received block
    // ...
}

double SyncManager::getSyncProgress() const {
    if (targetHeight == 0) return 0.0;
    return static_cast<double>(currentHeight) / targetHeight;
}

std::string SyncManager::getSyncPeer() const {
    return syncPeer;
}

uint32_t SyncManager::getTargetHeight() const {
    return targetHeight;
}

std::chrono::seconds SyncManager::getSyncDuration() const {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(now - syncStartTime);
}

std::string SyncManager::selectSyncPeer() {
    // Select a random ready peer for sync
    auto readyPeers = peerManager->getReadyPeers();
    if (readyPeers.empty()) return "";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, readyPeers.size() - 1);
    return readyPeers[dis(gen)]->getId();
}

void SyncManager::updateSyncPeer() {
    syncPeer = selectSyncPeer();
}

P2PNetwork::P2PNetwork(const NetworkConfig& cfg, ChainState* chain, Mempool* mp)
    : config(cfg), chainState(chain), mempool(mp) {
    peerManager = std::make_unique<PeerManager>(cfg.maxConnections, cfg.maxInbound, cfg.maxOutbound);
    syncManager = std::make_unique<SyncManager>(chain, peerManager.get());
}

P2PNetwork::~P2PNetwork() {
    stop();
    cleanupNetwork();
}

bool P2PNetwork::start() {
    running = true;
    listening = config.listen;
    // Start network, sync, and message threads (stubbed)
    // ...
    return true;
}

void P2PNetwork::stop() {
    running = false;
    listening = false;
    // Stop threads and cleanup
    // ...
}

void P2PNetwork::updateConfig(const NetworkConfig& newConfig) {
    config = newConfig;
}

bool P2PNetwork::connectToPeer(const std::string& address) {
    NetworkAddress addr = stringToAddress(address);
    if (!isValidPeerAddress(addr)) return false;
    auto peer = peerManager->addPeer(addr, false);
    return peer != nullptr;
}

void P2PNetwork::disconnectPeer(const std::string& peerId) {
    peerManager->disconnectPeer(peerId);
}

void P2PNetwork::banPeer(const std::string& peerId, const std::string& reason) {
    peerManager->banPeer(peerId, reason);
}

void P2PNetwork::broadcastTransaction(const Transaction& tx) {
    auto inv = std::vector<InventoryVector>{ InventoryVector(InventoryType::TX, tx.txid) };
    relayInventory(inv);
}

void P2PNetwork::broadcastBlock(const Block& block) {
    auto inv = std::vector<InventoryVector>{ InventoryVector(InventoryType::BLOCK, block.hash) };
    relayInventory(inv);
}

void P2PNetwork::relayInventory(const std::vector<InventoryVector>& inventory, const std::string& excludePeer) {
    peerManager->broadcastInventory(inventory, excludePeer);
}

P2PNetwork::NetworkInfo P2PNetwork::getNetworkInfo() const {
    NetworkInfo info;
    info.running = running.load();
    info.listening = listening.load();
    info.port = config.port;
    info.connections = peerManager->getConnectedCount();
    info.inbound = peerManager->getInboundCount();
    info.outbound = peerManager->getOutboundCount();
    info.syncState = syncManager->getState();
    info.syncProgress = syncManager->getSyncProgress();
    info.bestPeer = syncManager->getSyncPeer();
    auto stats = peerManager->getStats();
    info.bytesReceived = stats.totalBytesReceived;
    info.bytesSent = stats.totalBytesSent;
    info.messagesReceived = stats.totalMessagesReceived;
    info.messagesSent = stats.totalMessagesSent;
    return info;
}

void P2PNetwork::addAddress(const NetworkAddress& address) {
    std::lock_guard<std::mutex> lock(addressMutex);
    knownAddresses.push_back(address);
}

std::vector<NetworkAddress> P2PNetwork::getKnownAddresses() const {
    std::lock_guard<std::mutex> lock(addressMutex);
    return knownAddresses;
}

void P2PNetwork::shareAddresses(const std::string& peerId) {
    // Send addr message to peer
    // ...
}

void P2PNetwork::onPeerConnected(const std::string& peerId) {
    std::cout << "Peer connected: " << peerId << std::endl;
}

void P2PNetwork::onPeerDisconnected(const std::string& peerId) {
    std::cout << "Peer disconnected: " << peerId << std::endl;
}

void P2PNetwork::onPeerHandshakeComplete(const std::string& peerId) {
    std::cout << "Peer handshake complete: " << peerId << std::endl;
}

void P2PNetwork::onPeerBanned(const std::string& peerId, const std::string& reason) {
    std::cout << "Peer banned: " << peerId << " Reason: " << reason << std::endl;
}

void P2PNetwork::onMessageReceived(const std::string& peerId, std::shared_ptr<P2PMessage> message) {
    std::cout << "Message received from " << peerId << " Type: " << messageTypeToString(message->header.command) << std::endl;
}

void P2PNetwork::onInvalidMessage(const std::string& peerId, const std::string& reason) {
    std::cout << "Invalid message from " << peerId << " Reason: " << reason << std::endl;
}

void P2PNetwork::onInventoryReceived(const std::string& peerId, const std::vector<InventoryVector>& inventory) {
    std::cout << "Inventory received from " << peerId << " Count: " << inventory.size() << std::endl;
}

void P2PNetwork::onTransactionReceived(const std::string& peerId, const Transaction& tx) {
    std::cout << "Transaction received from " << peerId << " TXID: " << tx.txid << std::endl;
}

void P2PNetwork::onBlockReceived(const std::string& peerId, const Block& block) {
    std::cout << "Block received from " << peerId << " Hash: " << block.hash << std::endl;
}

void P2PNetwork::onHeadersReceived(const std::string& peerId, const std::vector<std::string>& headers) {
    std::cout << "Headers received from " << peerId << " Count: " << headers.size() << std::endl;
}

void P2PNetwork::onSyncStarted(const std::string& peerId) {
    std::cout << "Sync started with peer: " << peerId << std::endl;
}

void P2PNetwork::onSyncCompleted(const std::string& peerId) {
    std::cout << "Sync completed with peer: " << peerId << std::endl;
}

void P2PNetwork::printNetworkStatus() const {
    auto info = getNetworkInfo();
    std::cout << "\n=== P2P Network Status ===" << std::endl;
    std::cout << "Running: " << (info.running ? "Yes" : "No") << std::endl;
    std::cout << "Listening: " << (info.listening ? "Yes" : "No") << std::endl;
    std::cout << "Port: " << info.port << std::endl;
    std::cout << "Connections: " << info.connections << " (Inbound: " << info.inbound << ", Outbound: " << info.outbound << ")" << std::endl;
    std::cout << "Sync state: " << static_cast<int>(info.syncState) << " Progress: " << std::fixed << std::setprecision(2) << info.syncProgress * 100 << "%" << std::endl;
    std::cout << "Best peer: " << info.bestPeer << std::endl;
    std::cout << "Bytes sent: " << info.bytesSent << " Bytes received: " << info.bytesReceived << std::endl;
    std::cout << "Messages sent: " << info.messagesSent << " Messages received: " << info.messagesReceived << std::endl;
}

void P2PNetwork::printPeerList() const {
    peerManager->printPeerStatus();
}

std::string P2PNetwork::getNetworkStatusString() const {
    auto info = getNetworkInfo();
    std::stringstream ss;
    ss << "P2P Network: " << (info.running ? "Running" : "Stopped") << ", Listening: " << (info.listening ? "Yes" : "No")
       << ", Connections: " << info.connections << ", Sync: " << static_cast<int>(info.syncState) << " (" << std::fixed << std::setprecision(2) << info.syncProgress * 100 << "%)";
    return ss.str();
}

void P2PNetwork::simulateConnect(const std::string& address) {
    connectToPeer(address);
}

void P2PNetwork::simulateMessage(const std::string& peerId, std::shared_ptr<P2PMessage> message) {
    onMessageReceived(peerId, message);
}

void P2PNetwork::simulateHandshake(const std::string& peerId) {
    onPeerHandshakeComplete(peerId);
}

NetworkAddress P2PNetwork::stringToAddress(const std::string& addressStr) {
    std::string ip;
    uint16_t port = config.port;
    parseAddress(addressStr, ip, port);
    return NetworkAddress(ip, port);
}

bool P2PNetwork::isValidPeerAddress(const NetworkAddress& address) {
    return isValidIP(address.ip) && address.port > 0;
}

void P2PNetwork::cleanupNetwork() {
    // Cleanup resources, threads, etc.
    // ...
}

// P2PNetworkFactory implementation
std::unique_ptr<P2PNetwork> P2PNetworkFactory::createMainnet(ChainState* chainState, Mempool* mempool) {
    auto config = getMainnetConfig();
    return std::make_unique<P2PNetwork>(config, chainState, mempool);
}

std::unique_ptr<P2PNetwork> P2PNetworkFactory::createTestnet(ChainState* chainState, Mempool* mempool) {
    auto config = getTestnetConfig();
    return std::make_unique<P2PNetwork>(config, chainState, mempool);
}

std::unique_ptr<P2PNetwork> P2PNetworkFactory::createRegtest(ChainState* chainState, Mempool* mempool) {
    auto config = getRegtestConfig();
    return std::make_unique<P2PNetwork>(config, chainState, mempool);
}

std::unique_ptr<P2PNetwork> P2PNetworkFactory::createSimulation(ChainState* chainState, Mempool* mempool) {
    auto config = getSimulationConfig();
    return std::make_unique<P2PNetwork>(config, chainState, mempool);
}

NetworkConfig P2PNetworkFactory::getMainnetConfig() {
    NetworkConfig cfg;
    cfg.port = 8333;
    cfg.userAgent = "/Pragma:1.0.0/";
    cfg.protocolVersion = 70015;
    cfg.services = 1;
    cfg.listen = true;
    cfg.maxConnections = 125;
    cfg.maxInbound = 100;
    cfg.maxOutbound = 25;
    cfg.connectTimeout = std::chrono::seconds(30);
    cfg.handshakeTimeout = std::chrono::seconds(60);
    cfg.pingInterval = std::chrono::seconds(60);
    return cfg;
}

NetworkConfig P2PNetworkFactory::getTestnetConfig() {
    NetworkConfig cfg = getMainnetConfig();
    cfg.port = 18333;
    cfg.userAgent = "/Pragma:1.0.0-testnet/";
    return cfg;
}

NetworkConfig P2PNetworkFactory::getRegtestConfig() {
    NetworkConfig cfg = getMainnetConfig();
    cfg.port = 18444;
    cfg.userAgent = "/Pragma:1.0.0-regtest/";
    return cfg;
}

NetworkConfig P2PNetworkFactory::getSimulationConfig() {
    NetworkConfig cfg = getMainnetConfig();
    cfg.port = 18445;
    cfg.userAgent = "/Pragma:1.0.0-sim/";
    cfg.listen = false;
    return cfg;
}

} // namespace pragma
