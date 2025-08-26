#include "rpc.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <termios.h>
#include <unistd.h>

namespace pragma {

// CLI implementation
CLI::CLI(std::shared_ptr<WalletManager> walletManager) 
    : walletManager_(walletManager), running_(true), isInteractive_(isatty(STDIN_FILENO)) {
}

void CLI::run() {
    std::cout << "=== Pragma Blockchain Wallet CLI ===" << std::endl;
    std::cout << "Type 'help' for available commands" << std::endl;
    std::cout << std::endl;
    
    std::string command;
    while (running_) {
        std::cout << "pragma> ";
        std::getline(std::cin, command);
        
        if (std::cin.eof()) {
            break;
        }
        
        processCommand(command);
    }
}

void CLI::processCommand(const std::string& command) {
    auto args = parseCommand(command);
    if (args.empty()) {
        return;
    }
    
    std::string cmd = args[0];
    args.erase(args.begin());
    
    try {
        if (cmd == "help" || cmd == "h") {
            showHelp();
        } else if (cmd == "createwallet") {
            createWallet(args);
        } else if (cmd == "loadwallet") {
            loadWallet(args);
        } else if (cmd == "getinfo") {
            getInfo();
        } else if (cmd == "getbalance") {
            getBalance();
        } else if (cmd == "getnewaddress") {
            getNewAddress(args);
        } else if (cmd == "listaddresses") {
            listAddresses();
        } else if (cmd == "sendtoaddress") {
            sendToAddress(args);
        } else if (cmd == "listtransactions") {
            listTransactions(args);
        } else if (cmd == "importprivkey") {
            importPrivateKey(args);
        } else if (cmd == "dumpprivkey") {
            exportPrivateKey(args);
        } else if (cmd == "listunspent") {
            listUnspent();
        } else if (cmd == "encryptwallet") {
            encryptWallet(args);
        } else if (cmd == "walletpassphrase") {
            unlockWallet(args);
        } else if (cmd == "walletlock") {
            lockWallet();
        } else if (cmd == "backupwallet") {
            backupWallet(args);
        } else if (cmd == "exit" || cmd == "quit" || cmd == "q") {
            exit();
        } else {
            printError("Unknown command: " + cmd + ". Type 'help' for available commands.");
        }
    } catch (const std::exception& e) {
        printError("Error: " + std::string(e.what()));
    }
}

void CLI::showHelp() {
    std::cout << std::endl;
    std::cout << "=== Available Commands ===" << std::endl;
    std::cout << std::endl;
    std::cout << "Wallet Management:" << std::endl;
    std::cout << "  createwallet <name> [passphrase]  - Create a new wallet" << std::endl;
    std::cout << "  loadwallet <name> [passphrase]    - Load an existing wallet" << std::endl;
    std::cout << "  getinfo                           - Show wallet information" << std::endl;
    std::cout << "  encryptwallet <passphrase>        - Encrypt the wallet" << std::endl;
    std::cout << "  walletpassphrase <passphrase>     - Unlock encrypted wallet" << std::endl;
    std::cout << "  walletlock                        - Lock the wallet" << std::endl;
    std::cout << "  backupwallet <filename>           - Backup wallet to file" << std::endl;
    std::cout << std::endl;
    std::cout << "Address Management:" << std::endl;
    std::cout << "  getnewaddress [label]             - Generate new address" << std::endl;
    std::cout << "  listaddresses                     - List all addresses" << std::endl;
    std::cout << "  importprivkey <key> [label]       - Import private key" << std::endl;
    std::cout << "  dumpprivkey <address>             - Export private key" << std::endl;
    std::cout << std::endl;
    std::cout << "Transaction Management:" << std::endl;
    std::cout << "  getbalance                        - Show wallet balance" << std::endl;
    std::cout << "  sendtoaddress <addr> <amount>     - Send coins to address" << std::endl;
    std::cout << "  listtransactions [count]          - List transactions" << std::endl;
    std::cout << "  listunspent                       - List unspent outputs" << std::endl;
    std::cout << std::endl;
    std::cout << "General:" << std::endl;
    std::cout << "  help                              - Show this help" << std::endl;
    std::cout << "  exit                              - Exit the CLI" << std::endl;
    std::cout << std::endl;
}

void CLI::createWallet(const std::vector<std::string>& args) {
    if (args.empty()) {
        printError("Usage: createwallet <name> [passphrase]");
        return;
    }
    
    std::string name = args[0];
    std::string passphrase = args.size() > 1 ? args[1] : "";
    
    // For command line usage, skip password prompting entirely
    // Use empty passphrase if not provided
    
    auto wallet = walletManager_->createWallet(name, passphrase);
    if (wallet) {
        currentWallet_ = name;
        printSuccess("Wallet '" + name + "' created successfully");
        
        // Generate first address
        auto address = wallet->generateNewAddress("default");
        std::cout << "First address: " << address.toString() << std::endl;
    } else {
        printError("Failed to create wallet. Wallet may already exist.");
    }
}

void CLI::loadWallet(const std::vector<std::string>& args) {
    if (args.empty()) {
        printError("Usage: loadwallet <name> [passphrase]");
        return;
    }
    
    std::string name = args[0];
    std::string passphrase = args.size() > 1 ? args[1] : "";
    
    auto wallet = walletManager_->loadWallet(name, passphrase);
    if (wallet) {
        currentWallet_ = name;
        printSuccess("Wallet '" + name + "' loaded successfully");
    } else {
        printError("Failed to load wallet. Check name and passphrase.");
    }
}

void CLI::getInfo() {
    auto wallet = getCurrentWallet();
    if (!wallet) {
        printError("No wallet loaded. Use 'createwallet' or 'loadwallet' first.");
        return;
    }
    
    auto info = wallet->getInfo();
    
    std::cout << std::endl;
    std::cout << "=== Wallet Information ===" << std::endl;
    std::cout << "Wallet Name: " << info.walletName << std::endl;
    std::cout << "Balance: " << formatAmount(info.balance) << " BTC" << std::endl;
    std::cout << "Unconfirmed: " << formatAmount(info.unconfirmedBalance) << " BTC" << std::endl;
    std::cout << "Immature: " << formatAmount(info.immatureBalance) << " BTC" << std::endl;
    std::cout << "Transactions: " << info.transactionCount << std::endl;
    std::cout << "Addresses: " << info.addressCount << std::endl;
    std::cout << "Keys: " << info.keyCount << std::endl;
    std::cout << "Encrypted: " << (info.encrypted ? "Yes" : "No") << std::endl;
    if (info.encrypted) {
        std::cout << "Locked: " << (wallet->isLocked() ? "Yes" : "No") << std::endl;
    }
    std::cout << std::endl;
}

void CLI::getBalance() {
    auto wallet = getCurrentWallet();
    if (!wallet) {
        printError("No wallet loaded.");
        return;
    }
    
    uint64_t balance = wallet->getBalance();
    uint64_t unconfirmed = wallet->getUnconfirmedBalance();
    
    std::cout << "Balance: " << formatAmount(balance) << " BTC" << std::endl;
    if (unconfirmed > 0) {
        std::cout << "Unconfirmed: " << formatAmount(unconfirmed) << " BTC" << std::endl;
    }
}

void CLI::getNewAddress(const std::vector<std::string>& args) {
    auto wallet = getCurrentWallet();
    if (!wallet) {
        printError("No wallet loaded.");
        return;
    }
    
    std::string label = args.empty() ? "" : args[0];
    auto address = wallet->generateNewAddress(label);
    
    std::cout << "New address: " << address.toString() << std::endl;
    if (!label.empty()) {
        std::cout << "Label: " << label << std::endl;
    }
}

void CLI::listAddresses() {
    auto wallet = getCurrentWallet();
    if (!wallet) {
        printError("No wallet loaded.");
        return;
    }
    
    auto addresses = wallet->getAddresses();
    auto keys = wallet->getAllKeys();
    
    if (addresses.empty()) {
        std::cout << "No addresses found." << std::endl;
        return;
    }
    
    std::cout << std::endl;
    std::cout << "=== Addresses ===" << std::endl;
    for (const auto& address : addresses) {
        std::cout << address.toString();
        
        // Find label
        for (const auto& key : keys) {
            if (key.address.toString() == address.toString()) {
                if (!key.label.empty()) {
                    std::cout << " (" << key.label << ")";
                }
                break;
            }
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;
}

void CLI::sendToAddress(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        printError("Usage: sendtoaddress <address> <amount>");
        return;
    }
    
    auto wallet = getCurrentWallet();
    if (!wallet) {
        printError("No wallet loaded.");
        return;
    }
    
    if (wallet->isLocked()) {
        printError("Wallet is locked. Use 'walletpassphrase' to unlock.");
        return;
    }
    
    std::string addressStr = args[0];
    double amount = std::stod(args[1]);
    
    Address address(addressStr);
    if (!address.isValid()) {
        printError("Invalid address format.");
        return;
    }
    
    uint64_t satoshis = static_cast<uint64_t>(amount * 100000000);
    if (satoshis == 0) {
        printError("Amount must be greater than 0.");
        return;
    }
    
    if (satoshis > wallet->getBalance()) {
        printError("Insufficient balance.");
        return;
    }
    
    std::string txid = wallet->sendToAddress(address, satoshis);
    if (txid.empty()) {
        printError("Failed to send transaction.");
    } else {
        printSuccess("Transaction sent!");
        std::cout << "TXID: " << txid << std::endl;
        std::cout << "Amount: " << formatAmount(satoshis) << " BTC" << std::endl;
        std::cout << "To: " << addressStr << std::endl;
    }
}

void CLI::listTransactions(const std::vector<std::string>& args) {
    auto wallet = getCurrentWallet();
    if (!wallet) {
        printError("No wallet loaded.");
        return;
    }
    
    int32_t count = args.empty() ? 10 : std::stoi(args[0]);
    auto transactions = wallet->getTransactions(count);
    
    if (transactions.empty()) {
        std::cout << "No transactions found." << std::endl;
        return;
    }
    
    std::cout << std::endl;
    std::cout << "=== Recent Transactions ===" << std::endl;
    for (const auto& tx : transactions) {
        std::cout << std::setw(20) << std::left << tx.txid;
        std::cout << std::setw(15) << std::right << formatAmount(std::abs(tx.amount));
        std::cout << " BTC ";
        std::cout << (tx.amount > 0 ? "[RECEIVED]" : "[SENT]");
        
        if (tx.confirmations > 0) {
            std::cout << " (" << tx.confirmations << " confirmations)";
        } else {
            std::cout << " (unconfirmed)";
        }
        
        if (!tx.comment.empty()) {
            std::cout << " - " << tx.comment;
        }
        
        std::cout << std::endl;
    }
    std::cout << std::endl;
}

void CLI::importPrivateKey(const std::vector<std::string>& args) {
    if (args.empty()) {
        printError("Usage: importprivkey <privatekey> [label]");
        return;
    }
    
    auto wallet = getCurrentWallet();
    if (!wallet) {
        printError("No wallet loaded.");
        return;
    }
    
    if (wallet->isLocked()) {
        printError("Wallet is locked. Use 'walletpassphrase' to unlock.");
        return;
    }
    
    std::string keyStr = args[0];
    std::string label = args.size() > 1 ? args[1] : "";
    
    auto privateKey = PrivateKey::fromWIF(keyStr);
    if (!privateKey.isValid()) {
        printError("Invalid private key format.");
        return;
    }
    
    if (wallet->importPrivateKey(privateKey, label)) {
        auto address = Address::fromPrivateKey(privateKey);
        printSuccess("Private key imported successfully");
        std::cout << "Address: " << address.toString() << std::endl;
        if (!label.empty()) {
            std::cout << "Label: " << label << std::endl;
        }
    } else {
        printError("Failed to import private key.");
    }
}

void CLI::exportPrivateKey(const std::vector<std::string>& args) {
    if (args.empty()) {
        printError("Usage: dumpprivkey <address>");
        return;
    }
    
    auto wallet = getCurrentWallet();
    if (!wallet) {
        printError("No wallet loaded.");
        return;
    }
    
    if (wallet->isLocked()) {
        printError("Wallet is locked. Use 'walletpassphrase' to unlock.");
        return;
    }
    
    Address address(args[0]);
    if (!address.isValid()) {
        printError("Invalid address format.");
        return;
    }
    
    if (!wallet->hasPrivateKey(address)) {
        printError("Address not found in wallet.");
        return;
    }
    
    auto privateKey = wallet->getPrivateKey(address);
    std::cout << "Private key: " << privateKey.toWIF() << std::endl;
    std::cout << "⚠️  WARNING: Keep this private key secure!" << std::endl;
}

void CLI::listUnspent() {
    auto wallet = getCurrentWallet();
    if (!wallet) {
        printError("No wallet loaded.");
        return;
    }
    
    auto utxos = wallet->getSpendableUTXOs();
    
    if (utxos.empty()) {
        std::cout << "No unspent outputs found." << std::endl;
        return;
    }
    
    std::cout << std::endl;
    std::cout << "=== Unspent Outputs ===" << std::endl;
    std::cout << std::setw(20) << std::left << "TXID"
              << std::setw(6) << "VOUT"
              << std::setw(15) << std::right << "AMOUNT"
              << std::setw(8) << "CONF"
              << "  ADDRESS" << std::endl;
    std::cout << std::string(80, '-') << std::endl;
    
    for (const auto& utxo : utxos) {
        std::cout << std::setw(20) << std::left << utxo.txid.substr(0, 16) + "..."
                  << std::setw(6) << utxo.vout
                  << std::setw(15) << std::right << formatAmount(utxo.amount)
                  << std::setw(8) << utxo.confirmations
                  << "  " << utxo.address.toString() << std::endl;
    }
    std::cout << std::endl;
}

void CLI::encryptWallet(const std::vector<std::string>& args) {
    if (args.empty()) {
        printError("Usage: encryptwallet <passphrase>");
        return;
    }
    
    auto wallet = getCurrentWallet();
    if (!wallet) {
        printError("No wallet loaded.");
        return;
    }
    
    std::string passphrase = args[0];
    
    if (wallet->encryptWallet(passphrase)) {
        printSuccess("Wallet encrypted successfully");
        std::cout << "⚠️  IMPORTANT: Write down your passphrase!" << std::endl;
        std::cout << "⚠️  If you lose it, you will lose access to your funds!" << std::endl;
    } else {
        printError("Failed to encrypt wallet. May already be encrypted.");
    }
}

void CLI::unlockWallet(const std::vector<std::string>& args) {
    if (args.empty()) {
        printError("Usage: walletpassphrase <passphrase> [timeout_seconds]");
        return;
    }
    
    auto wallet = getCurrentWallet();
    if (!wallet) {
        printError("No wallet loaded.");
        return;
    }
    
    std::string passphrase = args[0];
    uint32_t timeout = args.size() > 1 ? std::stoul(args[1]) : 0;
    
    if (wallet->unlock(passphrase, timeout)) {
        printSuccess("Wallet unlocked successfully");
        if (timeout > 0) {
            std::cout << "Wallet will lock automatically in " << timeout << " seconds" << std::endl;
        }
    } else {
        printError("Incorrect passphrase.");
    }
}

void CLI::lockWallet() {
    auto wallet = getCurrentWallet();
    if (!wallet) {
        printError("No wallet loaded.");
        return;
    }
    
    wallet->lock();
    printSuccess("Wallet locked successfully");
}

void CLI::backupWallet(const std::vector<std::string>& args) {
    if (args.empty()) {
        printError("Usage: backupwallet <filename>");
        return;
    }
    
    auto wallet = getCurrentWallet();
    if (!wallet) {
        printError("No wallet loaded.");
        return;
    }
    
    std::string filename = args[0];
    
    if (wallet->backup(filename)) {
        printSuccess("Wallet backed up to: " + filename);
    } else {
        printError("Failed to backup wallet.");
    }
}

void CLI::exit() {
    printSuccess("Goodbye!");
    running_ = false;
}

std::vector<std::string> CLI::parseCommand(const std::string& command) {
    std::vector<std::string> args;
    std::stringstream ss(command);
    std::string arg;
    
    while (ss >> arg) {
        args.push_back(arg);
    }
    
    return args;
}

std::shared_ptr<Wallet> CLI::getCurrentWallet() {
    if (currentWallet_.empty()) {
        return nullptr;
    }
    return walletManager_->getWallet(currentWallet_);
}

void CLI::printError(const std::string& error) {
    std::cout << "❌ " << error << std::endl;
}

void CLI::printSuccess(const std::string& message) {
    std::cout << "✅ " << message << std::endl;
}

std::string CLI::formatAmount(uint64_t amount) {
    double btc = static_cast<double>(amount) / 100000000.0;
    std::stringstream ss;
    ss << std::fixed << std::setprecision(8) << btc;
    return ss.str();
}

std::string CLI::getPassword(const std::string& prompt) {
    std::cout << prompt;
    
    // Turn off echo
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    
    std::string password;
    std::getline(std::cin, password);
    
    // Restore echo
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    std::cout << std::endl;
    
    return password;
}

} // namespace pragma
