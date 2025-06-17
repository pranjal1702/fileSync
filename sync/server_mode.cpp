#include "server_mode.hpp"
#include "../common/thread_pool.hpp"
#include "../common/block_info.hpp"
#include "../destination/destination_manager.hpp"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <thread>
#include<fstream>
#include<vector>
const int MAX_CONNECTIONS = 3;

ServerMode::ServerMode(int port):port_(port),serverSocket_(-1),activeClients_(0){}

void ServerMode::startServer(){
    // will start the server and listen for the incoming request 
    setupSocket();
    acceptConnections();
}

void ServerMode::setupSocket() {
    serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket_ < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Set socket options failed");
        exit(EXIT_FAILURE);
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port_);

    if (bind(serverSocket_, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(serverSocket_, MAX_CONNECTIONS) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    std::cout << "[Server] Listening on port " << port_ << "...\n";
}

void ServerMode::acceptConnections() {
    ThreadPool pool(MAX_CONNECTIONS);

    while (true) {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock,[this](){
            return activeClients_<3;
        });
        sockaddr_in clientAddr{};
        socklen_t addrLen = sizeof(clientAddr);
        int clientSocket = accept(serverSocket_, (sockaddr*)&clientAddr, &addrLen);
        if (clientSocket < 0) {
            perror("Accept failed");
            continue;
        }

        activeClients_++;  // increment here under lock
        lock.unlock();

        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
        std::cout << "[Server] Connection accepted from " << clientIP << "\n";

        //  thread pool to handle client
        pool.submit([this, clientSocket]() {
            this->handleClient(clientSocket);

            // this client is done now give someone other chance
            {
                std::lock_guard<std::mutex> guard(mtx_);
                activeClients_--;
            }
            cv_.notify_one();
        });
    }
}

// handleClient for communication with the client 

void ServerMode::handleClient(int clientSocket){
    try{
        //get the path length from the client
        uint32_t pathLen = 0;
        if (recv(clientSocket, &pathLen, sizeof(pathLen), MSG_WAITALL) != sizeof(pathLen)) {
            throw std::runtime_error("Failed to read path length");
        }
        pathLen = ntohl(pathLen);

        // recieving the file path
        std::vector<char> buffer(pathLen);
        if (recv(clientSocket, buffer.data(), pathLen, MSG_WAITALL) != pathLen) {
            throw std::runtime_error("Failed to read file path");
        }

        std::string filePath(buffer.begin(), buffer.end());
        std::cout << "[Server] Received file path: " << filePath << "\n";

        DestinationManager destination(filePath,8); // blockSize 8 for now, robust logic needed for it later on

        std::vector<BlockInfo> blockHashes=destination.getFileBlockHashes();  // error handling not taken care of

        // sending length of the block hashes
        uint32_t blockCount = htonl(static_cast<uint32_t>(blockHashes.size()));
        if (send(clientSocket, &blockCount, sizeof(blockCount), 0) != sizeof(blockCount)) {
            throw std::runtime_error("Failed to send block count");
        }

        // 4. Send each BlockInfo
        for (const BlockInfo& b : blockHashes) {
            uint64_t offset_net = htobe64(b.offset);
            uint32_t weakHash_net = htonl(b.weakHash);
            uint32_t strongLen = htonl(static_cast<uint32_t>(b.strongHash.size()));

            send(clientSocket, &offset_net, sizeof(offset_net), 0);
            send(clientSocket, &weakHash_net, sizeof(weakHash_net), 0);
            send(clientSocket, &strongLen, sizeof(strongLen), 0);
            send(clientSocket, b.strongHash.data(), b.strongHash.size(), 0);
        }

        std::cout << "[Server] Sent " << blockHashes.size() << " blocks to client\n";
        close(clientSocket);

    }catch(const std::exception& e){
        std::cerr << "[Server] Error: " << e.what() << "\n";
        close(clientSocket);
    }

}