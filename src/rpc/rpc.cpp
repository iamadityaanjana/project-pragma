#include "rpc.h"
#include <iostream>
#include <sstream>
#include <regex>
#include <iomanip>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

namespace pragma {

// RPCServer implementation
RPCServer::RPCServer(const Config& config) : config_(config), running_(false) {
    registerDefaultMethods();
}

RPCServer::RPCServer() : config_({}), running_(false) {
    registerDefaultMethods();
}

RPCServer::~RPCServer() {
    stop();
}

bool RPCServer::start() {
    if (running_) {
        return false;
    }

    running_ = true;
    serverThread_ = std::thread(&RPCServer::serverLoop, this);
    
    std::cout << "[RPC] Server starting on " << config_.bindAddress << ":" << config_.port << std::endl;
    return true;
}

void RPCServer::stop() {
    if (!running_) {
        return;
    }

    running_ = false;
    if (serverThread_.joinable()) {
        serverThread_.join();
    }
    
    std::cout << "[RPC] Server stopped" << std::endl;
}

void RPCServer::serverLoop() {
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        std::cerr << "[RPC] Failed to create socket" << std::endl;
        return;
    }

    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(config_.port);
    inet_pton(AF_INET, config_.bindAddress.c_str(), &serverAddr.sin_addr);

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "[RPC] Failed to bind socket" << std::endl;
        close(serverSocket);
        return;
    }

    if (listen(serverSocket, config_.maxConnections) < 0) {
        std::cerr << "[RPC] Failed to listen on socket" << std::endl;
        close(serverSocket);
        return;
    }

    std::cout << "[RPC] Server listening on " << config_.bindAddress << ":" << config_.port << std::endl;

    while (running_) {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        
        int clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientLen);
        if (clientSocket < 0) {
            if (running_) {
                std::cerr << "[RPC] Failed to accept connection" << std::endl;
            }
            continue;
        }

        // Handle connection in separate thread
        std::thread clientThread(&RPCServer::handleConnection, this, clientSocket);
        clientThread.detach();
    }

    close(serverSocket);
}

void RPCServer::handleConnection(int clientSocket) {
    char buffer[4096];
    ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    
    if (bytesRead > 0) {
        buffer[bytesRead] = '\0';
        std::string request(buffer);
        
        // Parse HTTP request
        std::string httpResponse;
        std::string jsonResponse;
        
        // Simple HTTP parsing - look for JSON-RPC in POST body
        size_t bodyPos = request.find("\r\n\r\n");
        if (bodyPos != std::string::npos) {
            std::string body = request.substr(bodyPos + 4);
            
            // Basic authentication check
            bool authenticated = !config_.enableAuth;
            if (config_.enableAuth) {
                std::string authHeader = "Authorization: Basic ";
                size_t authPos = request.find(authHeader);
                if (authPos != std::string::npos) {
                    size_t authEnd = request.find("\r\n", authPos);
                    if (authEnd != std::string::npos) {
                        std::string auth = request.substr(authPos + authHeader.length(), 
                                                        authEnd - authPos - authHeader.length());
                        authenticated = authenticate(auth);
                    }
                }
            }
            
            if (authenticated) {
                jsonResponse = processRequest(body);
            } else {
                jsonResponse = createResponse("", "", "Authentication required");
            }
        } else {
            jsonResponse = createResponse("", "", "Invalid HTTP request");
        }
        
        // Create HTTP response
        httpResponse = "HTTP/1.1 200 OK\r\n";
        httpResponse += "Content-Type: application/json\r\n";
        httpResponse += "Content-Length: " + std::to_string(jsonResponse.length()) + "\r\n";
        httpResponse += "Access-Control-Allow-Origin: *\r\n";
        httpResponse += "\r\n";
        httpResponse += jsonResponse;
        
        send(clientSocket, httpResponse.c_str(), httpResponse.length(), 0);
    }
    
    close(clientSocket);
}

std::string RPCServer::processRequest(const std::string& request) {
    try {
        auto json = JSONParser::parse(request);
        
        if (json.type != JSONParser::Value::OBJECT) {
            return createResponse("", "", "Invalid JSON-RPC request");
        }
        
        std::string method = json.objectValue["method"].stringValue;
        std::string params = JSONParser::stringify(json.objectValue["params"]);
        std::string id = json.objectValue["id"].stringValue;
        
        auto methodIt = methods_.find(method);
        if (methodIt == methods_.end()) {
            return createResponse(id, "", "Method not found");
        }
        
        std::string result = methodIt->second(params);
        return createResponse(id, result);
        
    } catch (const std::exception& e) {
        return createResponse("", "", "Parse error: " + std::string(e.what()));
    }
}

std::string RPCServer::createResponse(const std::string& id, const std::string& result, const std::string& error) {
    std::string response = "{\"jsonrpc\":\"2.0\",\"id\":\"" + id + "\"";
    
    if (error.empty()) {
        response += ",\"result\":" + result + "}";
    } else {
        response += ",\"error\":{\"code\":-1,\"message\":\"" + error + "\"}}";
    }
    
    return response;
}

bool RPCServer::authenticate(const std::string& auth) {
    // Simplified base64 decode for username:password
    // In practice, use a proper base64 library
    std::string expected = config_.username + ":" + config_.password;
    return auth.find(expected) != std::string::npos; // Simplified check
}

void RPCServer::registerDefaultMethods() {
    // Register basic methods
    registerMethod("help", [this](const std::string& params) {
        return "\"Available methods: getblockchaininfo, getbestblockhash, getblockcount, getbalance, getnewaddress, sendtoaddress\"";
    });
    
    registerMethod("ping", [this](const std::string& params) {
        return "\"pong\"";
    });
}

void RPCServer::registerMethod(const std::string& method, RPCMethod handler) {
    std::lock_guard<std::mutex> lock(serverMutex_);
    methods_[method] = handler;
}

// RPCCommands implementation
RPCCommands::RPCCommands(std::shared_ptr<ChainState> chainState,
                        std::shared_ptr<Mempool> mempool,
                        std::shared_ptr<WalletManager> walletManager)
    : chainState_(chainState), mempool_(mempool), walletManager_(walletManager) {
}

std::string RPCCommands::getBlockchainInfo(const std::string& params) {
    if (!chainState_) {
        return createJSONError(-1, "ChainState not available");
    }
    
    auto stats = chainState_->getChainStats();
    
    std::stringstream ss;
    ss << "{"
       << "\"chain\":\"pragma\","
       << "\"blocks\":" << stats.height << ","
       << "\"headers\":" << stats.height << ","
       << "\"bestblockhash\":\"" << stats.bestHash.substr(0, 16) << "\","
       << "\"difficulty\":" << stats.difficulty << ","
       << "\"mediantime\":" << std::time(nullptr) << ","
       << "\"verificationprogress\":1.0,"
       << "\"chainwork\":\"" << std::hex << stats.totalWork << "\","
       << "\"size_on_disk\":" << (stats.totalBlocks * 1000) << ","
       << "\"pruned\":false"
       << "}";
    
    return ss.str();
}

std::string RPCCommands::getBestBlockHash(const std::string& params) {
    if (!chainState_) {
        return createJSONError(-1, "ChainState not available");
    }
    
    auto stats = chainState_->getChainStats();
    return "\"" + stats.bestHash.substr(0, 16) + "\"";
}

std::string RPCCommands::getBlockCount(const std::string& params) {
    if (!chainState_) {
        return createJSONError(-1, "ChainState not available");
    }
    
    auto stats = chainState_->getChainStats();
    return std::to_string(stats.height);
}

std::string RPCCommands::getBalance(const std::string& params) {
    auto wallet = getDefaultWallet();
    if (!wallet) {
        return createJSONError(-1, "Wallet not available");
    }
    
    uint64_t balance = wallet->getBalance();
    double btcBalance = static_cast<double>(balance) / 100000000.0; // Convert satoshis to BTC
    
    std::stringstream ss;
    ss << std::fixed << std::setprecision(8) << btcBalance;
    return ss.str();
}

std::string RPCCommands::getNewAddress(const std::string& params) {
    auto wallet = getDefaultWallet();
    if (!wallet) {
        return createJSONError(-1, "Wallet not available");
    }
    
    auto params_vec = splitParams(params);
    std::string label = params_vec.size() > 0 ? params_vec[0] : "";
    
    auto address = wallet->generateNewAddress(label);
    return "\"" + address.toString() + "\"";
}

std::string RPCCommands::sendToAddress(const std::string& params) {
    auto wallet = getDefaultWallet();
    if (!wallet) {
        return createJSONError(-1, "Wallet not available");
    }
    
    auto params_vec = splitParams(params);
    if (params_vec.size() < 2) {
        return createJSONError(-1, "sendtoaddress requires address and amount");
    }
    
    std::string addressStr = params_vec[0];
    double amount = std::stod(params_vec[1]);
    std::string comment = params_vec.size() > 2 ? params_vec[2] : "";
    
    Address address(addressStr);
    if (!address.isValid()) {
        return createJSONError(-1, "Invalid address");
    }
    
    uint64_t satoshis = static_cast<uint64_t>(amount * 100000000); // Convert BTC to satoshis
    std::string txid = wallet->sendToAddress(address, satoshis, comment);
    
    if (txid.empty()) {
        return createJSONError(-1, "Failed to send transaction");
    }
    
    return "\"" + txid + "\"";
}

std::string RPCCommands::listUnspent(const std::string& params) {
    auto wallet = getDefaultWallet();
    if (!wallet) {
        return createJSONError(-1, "Wallet not available");
    }
    
    auto utxos = wallet->getSpendableUTXOs();
    
    std::stringstream ss;
    ss << "[";
    
    for (size_t i = 0; i < utxos.size(); ++i) {
        const auto& utxo = utxos[i];
        double amount = static_cast<double>(utxo.amount) / 100000000.0;
        
        if (i > 0) ss << ",";
        ss << "{"
           << "\"txid\":\"" << utxo.txid << "\","
           << "\"vout\":" << utxo.vout << ","
           << "\"address\":\"" << utxo.address.toString() << "\","
           << "\"amount\":" << std::fixed << std::setprecision(8) << amount << ","
           << "\"confirmations\":" << utxo.confirmations << ","
           << "\"spendable\":" << (utxo.spendable ? "true" : "false") << ","
           << "\"solvable\":" << (utxo.solvable ? "true" : "false")
           << "}";
    }
    
    ss << "]";
    return ss.str();
}

std::string RPCCommands::getWalletInfo(const std::string& params) {
    auto wallet = getDefaultWallet();
    if (!wallet) {
        return createJSONError(-1, "Wallet not available");
    }
    
    auto info = wallet->getInfo();
    double balance = static_cast<double>(info.balance) / 100000000.0;
    double unconfirmed = static_cast<double>(info.unconfirmedBalance) / 100000000.0;
    
    std::stringstream ss;
    ss << "{"
       << "\"walletname\":\"" << info.walletName << "\","
       << "\"walletversion\":1,"
       << "\"balance\":" << std::fixed << std::setprecision(8) << balance << ","
       << "\"unconfirmed_balance\":" << std::fixed << std::setprecision(8) << unconfirmed << ","
       << "\"txcount\":" << info.transactionCount << ","
       << "\"keypoolsize\":" << info.keyCount << ","
       << "\"encrypted\":" << (info.encrypted ? "true" : "false") << ","
       << "\"unlocked_until\":0"
       << "}";
    
    return ss.str();
}

std::shared_ptr<Wallet> RPCCommands::getDefaultWallet() {
    if (!walletManager_) {
        return nullptr;
    }
    return walletManager_->getWallet();
}

std::string RPCCommands::createJSONError(int code, const std::string& message) {
    return "{\"error\":{\"code\":" + std::to_string(code) + ",\"message\":\"" + message + "\"}}";
}

std::vector<std::string> RPCCommands::splitParams(const std::string& params) {
    std::vector<std::string> result;
    
    // Simple JSON array parsing for parameters
    if (params.empty() || params == "[]") {
        return result;
    }
    
    // Remove brackets and split by comma
    std::string cleaned = params;
    if (cleaned.front() == '[') cleaned = cleaned.substr(1);
    if (cleaned.back() == ']') cleaned = cleaned.substr(0, cleaned.length() - 1);
    
    std::stringstream ss(cleaned);
    std::string item;
    
    while (std::getline(ss, item, ',')) {
        // Remove quotes and whitespace
        item.erase(std::remove(item.begin(), item.end(), '"'), item.end());
        item.erase(std::remove(item.begin(), item.end(), ' '), item.end());
        if (!item.empty()) {
            result.push_back(item);
        }
    }
    
    return result;
}

// JSONParser implementation
JSONParser::Value JSONParser::parse(const std::string& json) {
    size_t pos = 0;
    return parseValue(json, pos);
}

JSONParser::Value JSONParser::parseValue(const std::string& json, size_t& pos) {
    skipWhitespace(json, pos);
    
    if (pos >= json.length()) {
        return Value();
    }
    
    char ch = json[pos];
    switch (ch) {
        case '"':
            return parseString(json, pos);
        case '[':
            return parseArray(json, pos);
        case '{':
            return parseObject(json, pos);
        case 't':
        case 'f':
            // Boolean
            if (json.substr(pos, 4) == "true") {
                pos += 4;
                return Value(true);
            } else if (json.substr(pos, 5) == "false") {
                pos += 5;
                return Value(false);
            }
            break;
        case 'n':
            // Null
            if (json.substr(pos, 4) == "null") {
                pos += 4;
                return Value();
            }
            break;
        default:
            if (std::isdigit(ch) || ch == '-') {
                return parseNumber(json, pos);
            }
            break;
    }
    
    return Value();
}

JSONParser::Value JSONParser::parseString(const std::string& json, size_t& pos) {
    if (json[pos] != '"') {
        return Value();
    }
    
    pos++; // Skip opening quote
    size_t start = pos;
    
    while (pos < json.length() && json[pos] != '"') {
        if (json[pos] == '\\') {
            pos += 2; // Skip escaped character
        } else {
            pos++;
        }
    }
    
    if (pos >= json.length()) {
        return Value();
    }
    
    std::string str = json.substr(start, pos - start);
    pos++; // Skip closing quote
    
    return Value(unescape(str));
}

JSONParser::Value JSONParser::parseNumber(const std::string& json, size_t& pos) {
    size_t start = pos;
    
    if (json[pos] == '-') {
        pos++;
    }
    
    while (pos < json.length() && (std::isdigit(json[pos]) || json[pos] == '.')) {
        pos++;
    }
    
    std::string numStr = json.substr(start, pos - start);
    return Value(std::stod(numStr));
}

JSONParser::Value JSONParser::parseArray(const std::string& json, size_t& pos) {
    Value array;
    array.type = Value::ARRAY;
    
    pos++; // Skip opening bracket
    skipWhitespace(json, pos);
    
    if (pos < json.length() && json[pos] == ']') {
        pos++;
        return array;
    }
    
    while (pos < json.length()) {
        array.arrayValue.push_back(parseValue(json, pos));
        skipWhitespace(json, pos);
        
        if (pos >= json.length()) break;
        
        if (json[pos] == ']') {
            pos++;
            break;
        } else if (json[pos] == ',') {
            pos++;
            skipWhitespace(json, pos);
        }
    }
    
    return array;
}

JSONParser::Value JSONParser::parseObject(const std::string& json, size_t& pos) {
    Value object;
    object.type = Value::OBJECT;
    
    pos++; // Skip opening brace
    skipWhitespace(json, pos);
    
    if (pos < json.length() && json[pos] == '}') {
        pos++;
        return object;
    }
    
    while (pos < json.length()) {
        // Parse key
        auto key = parseString(json, pos);
        skipWhitespace(json, pos);
        
        if (pos >= json.length() || json[pos] != ':') break;
        pos++; // Skip colon
        
        // Parse value
        auto value = parseValue(json, pos);
        object.objectValue[key.stringValue] = value;
        
        skipWhitespace(json, pos);
        
        if (pos >= json.length()) break;
        
        if (json[pos] == '}') {
            pos++;
            break;
        } else if (json[pos] == ',') {
            pos++;
            skipWhitespace(json, pos);
        }
    }
    
    return object;
}

void JSONParser::skipWhitespace(const std::string& json, size_t& pos) {
    while (pos < json.length() && std::isspace(json[pos])) {
        pos++;
    }
}

std::string JSONParser::stringify(const Value& value) {
    switch (value.type) {
        case Value::STRING:
            return "\"" + escape(value.stringValue) + "\"";
        case Value::NUMBER:
            return std::to_string(value.numberValue);
        case Value::BOOLEAN:
            return value.boolValue ? "true" : "false";
        case Value::NULL_TYPE:
            return "null";
        case Value::ARRAY: {
            std::string result = "[";
            for (size_t i = 0; i < value.arrayValue.size(); ++i) {
                if (i > 0) result += ",";
                result += stringify(value.arrayValue[i]);
            }
            result += "]";
            return result;
        }
        case Value::OBJECT: {
            std::string result = "{";
            bool first = true;
            for (const auto& pair : value.objectValue) {
                if (!first) result += ",";
                result += "\"" + escape(pair.first) + "\":" + stringify(pair.second);
                first = false;
            }
            result += "}";
            return result;
        }
    }
    return "null";
}

std::string JSONParser::escape(const std::string& str) {
    std::string result;
    for (char ch : str) {
        switch (ch) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += ch; break;
        }
    }
    return result;
}

std::string JSONParser::unescape(const std::string& str) {
    std::string result;
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '\\' && i + 1 < str.length()) {
            switch (str[i + 1]) {
                case '"': result += '"'; i++; break;
                case '\\': result += '\\'; i++; break;
                case 'n': result += '\n'; i++; break;
                case 'r': result += '\r'; i++; break;
                case 't': result += '\t'; i++; break;
                default: result += str[i]; break;
            }
        } else {
            result += str[i];
        }
    }
    return result;
}

} // namespace pragma
