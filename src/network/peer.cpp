#include "peer.h"
#include "../core/transaction.h"
#include "../core/block.h"
#include "../primitives/utils.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <random>

namespace pragma {

// Peer implementation
Peer::Peer(const std::string& peerId, const NetworkAddress& addr)
    : id(peerId), address(addr), state(PeerState::CONNECTING), version(0), services(0),
      startHeight(0), nonce(0), relay(true), versionSent(false), versionReceived(false),
      verackSent(false), verackReceived(false), messageRate(0) {
    
    stats.connectionTime = Utils::getCurrentTimestamp();
    lastMessageTime = std::chrono::steady_clock::now();
}

void Peer::setState(PeerState newState) {
    std::lock_guard<std::mutex> lock(peerMutex);
    state = newState;
    updateLastSeen();
}

bool Peer::isConnected() const {
    std::lock_guard<std::mutex> lock(peerMutex);
    return state != PeerState::DISCONNECTED && state != PeerState::BANNED;
}

bool Peer::isHandshakeComplete() const {
    std::lock_guard<std::mutex> lock(peerMutex);
    return state == PeerState::HANDSHAKE_COMPLETE || state == PeerState::SYNCING || state == PeerState::READY;
}

bool Peer::isReady() const {
    std::lock_guard<std::mutex> lock(peerMutex);
    return state == PeerState::READY;
}

bool Peer::isBanned() const {
    std::lock_guard<std::mutex> lock(peerMutex);
    return state == PeerState::BANNED;
}

void Peer::setVersionInfo(const VersionMessage& versionMsg) {
    std::lock_guard<std::mutex> lock(peerMutex);
    version = versionMsg.version;
    services = versionMsg.services;
    startHeight = versionMsg.startHeight;
    userAgent = versionMsg.userAgent;
    nonce = versionMsg.nonce;
    relay = versionMsg.relay;
}

void Peer::queueOutboundMessage(std::shared_ptr<P2PMessage> message) {
    std::lock_guard<std::mutex> lock(peerMutex);
    outboundQueue.push(message);
}

std::shared_ptr<P2PMessage> Peer::getNextOutboundMessage() {
    std::lock_guard<std::mutex> lock(peerMutex);
    if (outboundQueue.empty()) {
        return nullptr;
    }
    
    auto message = outboundQueue.front();
    outboundQueue.pop();
    return message;
}

void Peer::queueInboundMessage(std::shared_ptr<P2PMessage> message) {
    std::lock_guard<std::mutex> lock(peerMutex);
    inboundQueue.push(message);
}

std::shared_ptr<P2PMessage> Peer::getNextInboundMessage() {
    std::lock_guard<std::mutex> lock(peerMutex);
    if (inboundQueue.empty()) {
        return nullptr;
    }
    
    auto message = inboundQueue.front();
    inboundQueue.pop();
    return message;
}

bool Peer::hasOutboundMessages() const {
    std::lock_guard<std::mutex> lock(peerMutex);
    return !outboundQueue.empty();
}

bool Peer::hasInboundMessages() const {
    std::lock_guard<std::mutex> lock(peerMutex);
    return !inboundQueue.empty();
}

size_t Peer::getOutboundQueueSize() const {
    std::lock_guard<std::mutex> lock(peerMutex);
    return outboundQueue.size();
}

size_t Peer::getInboundQueueSize() const {
    std::lock_guard<std::mutex> lock(peerMutex);
    return inboundQueue.size();
}

void Peer::updateStats(uint64_t bytesReceived, uint64_t bytesSent) {
    std::lock_guard<std::mutex> lock(peerMutex);
    stats.bytesReceived += bytesReceived;
    stats.bytesSent += bytesSent;
    updateLastSeen();
}

void Peer::incrementMessageCount(bool inbound) {
    std::lock_guard<std::mutex> lock(peerMutex);
    if (inbound) {
        stats.messagesReceived++;
    } else {
        stats.messagesSent++;
    }
    updateLastSeen();
}

void Peer::incrementInvalidMessages() {
    std::lock_guard<std::mutex> lock(peerMutex);
    stats.invalidMessages++;
    increaseBanScore(10); // Invalid messages increase ban score
}

void Peer::updateLastSeen() {
    stats.lastSeen = Utils::getCurrentTimestamp();
}

void Peer::increaseBanScore(uint32_t points) {
    stats.banScore += points;
}

void Peer::resetBanScore() {
    stats.banScore = 0;
}

bool Peer::isRateLimited() const {
    auto now = std::chrono::steady_clock::now();
    auto timeSinceLastMessage = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastMessageTime).count();
    
    // Allow up to 100 messages per second
    return timeSinceLastMessage < 10;
}

void Peer::updateMessageRate() {
    lastMessageTime = std::chrono::steady_clock::now();
}

void Peer::addPendingPing(uint64_t pingNonce) {
    std::lock_guard<std::mutex> lock(peerMutex);
    pendingPings[pingNonce] = std::chrono::steady_clock::now();
}

bool Peer::handlePong(uint64_t pongNonce) {
    std::lock_guard<std::mutex> lock(peerMutex);
    auto it = pendingPings.find(pongNonce);
    if (it != pendingPings.end()) {
        auto now = std::chrono::steady_clock::now();
        auto latencyMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second).count();
        stats.latency = static_cast<double>(latencyMs);
        pendingPings.erase(it);
        return true;
    }
    return false;
}

bool Peer::hasInventory(const std::string& hash) const {
    std::lock_guard<std::mutex> lock(peerMutex);
    return knownInventory.find(hash) != knownInventory.end();
}

void Peer::addKnownInventory(const std::string& hash) {
    std::lock_guard<std::mutex> lock(peerMutex);
    knownInventory.insert(hash);
    
    // Cleanup old inventory if too large
    if (knownInventory.size() > 10000) {
        clearOldInventory(8000);
    }
}

void Peer::addRequestedInventory(const std::string& hash) {
    std::lock_guard<std::mutex> lock(peerMutex);
    requestedInventory.insert(hash);
}

void Peer::removeRequestedInventory(const std::string& hash) {
    std::lock_guard<std::mutex> lock(peerMutex);
    requestedInventory.erase(hash);
}

bool Peer::isInventoryRequested(const std::string& hash) const {
    std::lock_guard<std::mutex> lock(peerMutex);
    return requestedInventory.find(hash) != requestedInventory.end();
}

size_t Peer::getKnownInventorySize() const {
    std::lock_guard<std::mutex> lock(peerMutex);
    return knownInventory.size();
}

size_t Peer::getRequestedInventorySize() const {
    std::lock_guard<std::mutex> lock(peerMutex);
    return requestedInventory.size();
}

void Peer::clearOldInventory(size_t maxSize) {
    // Simple implementation: clear half when over limit
    // In production, would use LRU or time-based cleanup
    if (knownInventory.size() > maxSize) {
        auto it = knownInventory.begin();
        size_t toRemove = knownInventory.size() - maxSize;
        for (size_t i = 0; i < toRemove && it != knownInventory.end(); ++i) {
            it = knownInventory.erase(it);
        }
    }
}

std::string Peer::toString() const {
    std::lock_guard<std::mutex> lock(peerMutex);
    std::stringstream ss;
    ss << "Peer[" << id << "] " << address.toString() 
       << " State: " << static_cast<int>(state)
       << " Version: " << version
       << " Height: " << startHeight
       << " Latency: " << std::fixed << std::setprecision(1) << stats.latency << "ms";
    return ss.str();
}

double Peer::getConnectionDuration() const {
    uint64_t now = Utils::getCurrentTimestamp();
    return static_cast<double>(now - stats.connectionTime);
}

bool Peer::shouldBan() const {
    std::lock_guard<std::mutex> lock(peerMutex);
    return stats.banScore >= 100; // Ban threshold
}

// PeerManager implementation
PeerManager::PeerManager(size_t maxPeers, size_t maxInbound, size_t maxOutbound)
    : maxPeers(maxPeers), maxInboundPeers(maxInbound), maxOutboundPeers(maxOutbound),
      banThreshold(100), banDuration(std::chrono::hours(24)), maxMessageRate(100) {
}

std::string PeerManager::generatePeerId(const NetworkAddress& address) {
    return address.toString() + "_" + std::to_string(Utils::getCurrentTimestamp());
}

std::shared_ptr<Peer> PeerManager::addPeer(const NetworkAddress& address, bool inbound) {
    std::lock_guard<std::mutex> lock(managerMutex);
    
    // Check if peer already exists
    auto addressStr = address.toString();
    auto it = addressToPeerId.find(addressStr);
    if (it != addressToPeerId.end()) {
        return getPeer(it->second);
    }
    
    // Check connection limits
    if (inbound && !canAcceptInbound()) {
        return nullptr;
    }
    
    if (!inbound && !canMakeOutbound()) {
        return nullptr;
    }
    
    // Check if banned
    if (isBanned(address)) {
        return nullptr;
    }
    
    // Create new peer
    std::string peerId = generatePeerId(address);
    auto peer = std::make_shared<Peer>(peerId, address);
    
    peers[peerId] = peer;
    addressToPeerId[addressStr] = peerId;
    
    if (inbound) {
        inboundConnections++;
    } else {
        outboundConnections++;
    }
    connectedPeers++;
    
    return peer;
}

bool PeerManager::removePeer(const std::string& peerId) {
    std::lock_guard<std::mutex> lock(managerMutex);
    
    auto it = peers.find(peerId);
    if (it == peers.end()) {
        return false;
    }
    
    auto peer = it->second;
    auto addressStr = peer->getAddress().toString();
    
    peers.erase(it);
    addressToPeerId.erase(addressStr);
    
    if (peer->isConnected()) {
        connectedPeers--;
        // Note: We don't track inbound/outbound separately for removal
        // In a full implementation, this would be tracked per peer
    }
    
    return true;
}

void PeerManager::disconnectPeer(const std::string& peerId) {
    auto peer = getPeer(peerId);
    if (peer) {
        peer->setState(PeerState::DISCONNECTING);
    }
}

void PeerManager::banPeer(const std::string& peerId, const std::string& reason) {
    std::lock_guard<std::mutex> lock(managerMutex);
    
    auto peer = getPeer(peerId);
    if (peer) {
        peer->setState(PeerState::BANNED);
        bannedPeers[peer->getAddress().toString()] = std::chrono::steady_clock::now();
    }
}

bool PeerManager::isBanned(const NetworkAddress& address) const {
    std::lock_guard<std::mutex> lock(managerMutex);
    auto it = bannedPeers.find(address.toString());
    if (it == bannedPeers.end()) {
        return false;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto banTime = it->second;
    return (now - banTime) < banDuration;
}

std::shared_ptr<Peer> PeerManager::getPeer(const std::string& peerId) {
    std::lock_guard<std::mutex> lock(managerMutex);
    auto it = peers.find(peerId);
    return (it != peers.end()) ? it->second : nullptr;
}

std::shared_ptr<Peer> PeerManager::getPeerByAddress(const NetworkAddress& address) {
    std::lock_guard<std::mutex> lock(managerMutex);
    auto it = addressToPeerId.find(address.toString());
    if (it != addressToPeerId.end()) {
        return getPeer(it->second);
    }
    return nullptr;
}

std::vector<std::shared_ptr<Peer>> PeerManager::getAllPeers() const {
    std::lock_guard<std::mutex> lock(managerMutex);
    std::vector<std::shared_ptr<Peer>> result;
    result.reserve(peers.size());
    
    for (const auto& pair : peers) {
        result.push_back(pair.second);
    }
    
    return result;
}

std::vector<std::shared_ptr<Peer>> PeerManager::getReadyPeers() const {
    std::lock_guard<std::mutex> lock(managerMutex);
    std::vector<std::shared_ptr<Peer>> result;
    
    for (const auto& pair : peers) {
        if (pair.second->isReady()) {
            result.push_back(pair.second);
        }
    }
    
    return result;
}

std::vector<std::shared_ptr<Peer>> PeerManager::getConnectedPeers() const {
    std::lock_guard<std::mutex> lock(managerMutex);
    std::vector<std::shared_ptr<Peer>> result;
    
    for (const auto& pair : peers) {
        if (pair.second->isConnected()) {
            result.push_back(pair.second);
        }
    }
    
    return result;
}

bool PeerManager::canAcceptInbound() const {
    return inboundConnections.load() < maxInboundPeers && connectedPeers.load() < maxPeers;
}

bool PeerManager::canMakeOutbound() const {
    return outboundConnections.load() < maxOutboundPeers && connectedPeers.load() < maxPeers;
}

bool PeerManager::hasRoom() const {
    return connectedPeers.load() < maxPeers;
}

void PeerManager::broadcastMessage(std::shared_ptr<P2PMessage> message, const std::string& excludePeer) {
    auto peers = getConnectedPeers();
    for (auto& peer : peers) {
        if (peer->getId() != excludePeer) {
            peer->queueOutboundMessage(message);
        }
    }
}

void PeerManager::broadcastToReady(std::shared_ptr<P2PMessage> message, const std::string& excludePeer) {
    auto peers = getReadyPeers();
    for (auto& peer : peers) {
        if (peer->getId() != excludePeer) {
            peer->queueOutboundMessage(message);
        }
    }
}

void PeerManager::sendToRandomPeers(std::shared_ptr<P2PMessage> message, size_t count) {
    auto peers = getReadyPeers();
    if (peers.empty()) return;
    
    // Shuffle and take first 'count' peers
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(peers.begin(), peers.end(), g);
    size_t sendCount = std::min(count, peers.size());
    
    for (size_t i = 0; i < sendCount; ++i) {
        peers[i]->queueOutboundMessage(message);
    }
}

void PeerManager::broadcastInventory(const std::vector<InventoryVector>& inventory, const std::string& excludePeer) {
    auto message = P2PMessage::createInv(inventory);
    broadcastToReady(message, excludePeer);
}

std::vector<std::shared_ptr<Peer>> PeerManager::getPeersWithoutInventory(const std::string& hash) {
    std::vector<std::shared_ptr<Peer>> result;
    auto peers = getReadyPeers();
    
    for (auto& peer : peers) {
        if (!peer->hasInventory(hash)) {
            result.push_back(peer);
        }
    }
    
    return result;
}

PeerManager::ManagerStats PeerManager::getStats() const {
    std::lock_guard<std::mutex> lock(managerMutex);
    ManagerStats stats;
    
    stats.totalPeers = peers.size();
    stats.connectedPeers = 0;
    stats.readyPeers = 0;
    stats.bannedPeers = bannedPeers.size();
    stats.totalBytesReceived = 0;
    stats.totalBytesSent = 0;
    stats.totalMessagesReceived = 0;
    stats.totalMessagesSent = 0;
    
    double totalLatency = 0.0;
    size_t latencyCount = 0;
    
    for (const auto& pair : peers) {
        auto peer = pair.second;
        if (peer->isConnected()) {
            stats.connectedPeers++;
        }
        if (peer->isReady()) {
            stats.readyPeers++;
        }
        
        auto peerStats = peer->getStats();
        stats.totalBytesReceived += peerStats.bytesReceived;
        stats.totalBytesSent += peerStats.bytesSent;
        stats.totalMessagesReceived += peerStats.messagesReceived;
        stats.totalMessagesSent += peerStats.messagesSent;
        
        if (peerStats.latency > 0) {
            totalLatency += peerStats.latency;
            latencyCount++;
        }
    }
    
    stats.averageLatency = latencyCount > 0 ? totalLatency / latencyCount : 0.0;
    
    return stats;
}

void PeerManager::cleanupBannedPeers() {
    std::lock_guard<std::mutex> lock(managerMutex);
    auto now = std::chrono::steady_clock::now();
    
    auto it = bannedPeers.begin();
    while (it != bannedPeers.end()) {
        if ((now - it->second) >= banDuration) {
            it = bannedPeers.erase(it);
        } else {
            ++it;
        }
    }
}

void PeerManager::cleanupPeers() {
    cleanupBannedPeers();
    
    // Remove disconnected peers
    std::vector<std::string> toRemove;
    {
        std::lock_guard<std::mutex> lock(managerMutex);
        for (const auto& pair : peers) {
            if (pair.second->getState() == PeerState::DISCONNECTED) {
                toRemove.push_back(pair.first);
            }
        }
    }
    
    for (const auto& peerId : toRemove) {
        removePeer(peerId);
    }
}

void PeerManager::updatePeerStates() {
    auto peers = getAllPeers();
    for (auto& peer : peers) {
        // Check for ban conditions
        if (peer->shouldBan() && !peer->isBanned()) {
            banPeer(peer->getId(), "Ban score exceeded");
        }
        
        // Update handshake state
        if (peer->getState() == PeerState::HANDSHAKING && peer->isHandshakeReady()) {
            peer->setState(PeerState::HANDSHAKE_COMPLETE);
        }
    }
}

void PeerManager::printPeerStatus() const {
    auto stats = getStats();
    std::cout << "\n=== Peer Manager Status ===" << std::endl;
    std::cout << "Total peers: " << stats.totalPeers << std::endl;
    std::cout << "Connected peers: " << stats.connectedPeers << std::endl;
    std::cout << "Ready peers: " << stats.readyPeers << std::endl;
    std::cout << "Banned peers: " << stats.bannedPeers << std::endl;
    std::cout << "Average latency: " << std::fixed << std::setprecision(1) << stats.averageLatency << "ms" << std::endl;
    std::cout << "Total bytes sent: " << stats.totalBytesSent << std::endl;
    std::cout << "Total bytes received: " << stats.totalBytesReceived << std::endl;
}

std::vector<std::string> PeerManager::getPeerIds() const {
    std::lock_guard<std::mutex> lock(managerMutex);
    std::vector<std::string> ids;
    ids.reserve(peers.size());
    
    for (const auto& pair : peers) {
        ids.push_back(pair.first);
    }
    
    return ids;
}

std::string PeerManager::getManagerStatus() const {
    auto stats = getStats();
    std::stringstream ss;
    ss << "Peers: " << stats.connectedPeers << "/" << maxPeers 
       << " (Ready: " << stats.readyPeers << ", Banned: " << stats.bannedPeers << ")";
    return ss.str();
}

} // namespace pragma
