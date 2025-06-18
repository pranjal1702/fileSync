#include "client_session.hpp"
#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <netinet/in.h>

ClientSession::ClientSession(const std::string& ip, int port, int id)
    : ip_(ip), port_(port), sessionId_(id), socketFD_(-1), connected_(false) {}

ClientSession::~ClientSession() {
    close();
}

// connecting to the specified ip and port
bool ClientSession::connectToServer() {
    socketFD_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFD_ < 0) {
        perror("[Session] Socket creation failed");
        return false;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port_);
    inet_pton(AF_INET, ip_.c_str(), &serverAddr.sin_addr);

    if (connect(socketFD_, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("[Session] Connection failed");
        ::close(socketFD_);
        socketFD_ = -1;
        return false;
    }

    connected_ = true;
    std::cout << "[Session " << sessionId_ << "] Connected to " << ip_ << ":" << port_ << "\n";
    return true;
}

// communication with the server
void ClientSession::runTransaction(const std::string& filepath) {
    if (!connected_) {
        std::cerr << "[Session " << sessionId_ << "] Not connected.\n";
        return;
    }

    if (!sendFilePath(filepath)) {
        std::cerr << "[Session " << sessionId_ << "] Failed to send file path.\n";
        return;
    }

    std::vector<BlockInfo> blocks;
    if (!receiveBlockHashes(blocks)) {
        std::cerr << "[Session " << sessionId_ << "] Failed to receive block hashes.\n";
        return;
    }

    std::cout << "[Session " << sessionId_ << "] Received " << blocks.size() << " block hashes.\n";
    // TODO: Use blocks as needed
}
// sending the file path
bool ClientSession::sendFilePath(const std::string& path) {
    uint32_t len = htonl(static_cast<uint32_t>(path.size()));
    if (send(socketFD_, &len, sizeof(len), 0) != sizeof(len)) return false;
    if (send(socketFD_, path.c_str(), path.size(), 0) != static_cast<ssize_t>(path.size())) return false;
    return true;
}

// getting the block hashes from the server
bool ClientSession::receiveBlockHashes(std::vector<BlockInfo>& blocks) {
    uint32_t blockCountNet;
    if (recv(socketFD_, &blockCountNet, sizeof(blockCountNet), MSG_WAITALL) != sizeof(blockCountNet)) return false;

    uint32_t blockCount = ntohl(blockCountNet);
    blocks.reserve(blockCount);

    for (uint32_t i = 0; i < blockCount; ++i) {
        uint64_t offsetNet;
        uint32_t weakHashNet, strongLenNet;

        if (recv(socketFD_, &offsetNet, sizeof(offsetNet), MSG_WAITALL) != sizeof(offsetNet)) return false;
        if (recv(socketFD_, &weakHashNet, sizeof(weakHashNet), MSG_WAITALL) != sizeof(weakHashNet)) return false;
        if (recv(socketFD_, &strongLenNet, sizeof(strongLenNet), MSG_WAITALL) != sizeof(strongLenNet)) return false;

        uint64_t offset = be64toh(offsetNet);
        uint32_t weakHash = ntohl(weakHashNet);
        uint32_t strongLen = ntohl(strongLenNet);

        std::vector<char> hashBuf(strongLen);
        if (recv(socketFD_, hashBuf.data(), strongLen, MSG_WAITALL) != strongLen) return false;

        blocks.emplace_back(offset, weakHash, std::string(hashBuf.begin(), hashBuf.end()));
    }

    return true;
}


void ClientSession::close() {
    if (connected_) {
        ::close(socketFD_);
        connected_ = false;
        std::cout << "[Session " << sessionId_ << "] Connection closed.\n";
    }
}

bool ClientSession::isConnected() const {
    return connected_;
}

std::string ClientSession::getInfo() const {
    return "[Session " + std::to_string(sessionId_) + "] " + ip_ + ":" + std::to_string(port_) +
           (connected_ ? " [CONNECTED]" : " [DISCONNECTED]");
}