#include "server_mode.hpp"
#include "../common/thread_pool.hpp"
#include "../common/block_info.hpp"
#include "../destination/destination_manager.hpp"
#include "../source/source_manager.hpp"
#include "../common/data_transfer.hpp"
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
    char cmd[5]; // For 4-char mode + '\n'
    ssize_t bytesRead = recv(clientSocket, cmd, sizeof(cmd), MSG_WAITALL);
    if (bytesRead != 5|| cmd[4]!='\n' ) {
        std::cerr << "[Server] Failed to read 5-byte command\n";
        close(clientSocket);
        return;
    }

    std::string mode(cmd, 4); // First 4 chars are the command
    if(mode=="PUSH"){
        pushTransaction(clientSocket);
    }else if(mode=="PULL"){
        pullTransaction(clientSocket);
    }else{
        std::cerr << "Invalid mode\n";
        close(clientSocket);
        return;
    }
}

bool ServerMode::pushTransaction(int clientSocket) {
    std::string remotePath;

    DataTransfer dataPipe;
    // 1. Receive remote file path
    if (!dataPipe.receiveFilePath(clientSocket,remotePath)){ 
        std::cerr << "Failed to receive remote path\n";
        return false;
    }

    // 2. Get existing file block hashes
    DestinationManager dest(remotePath, 8);  // Block size = 8
    Result<std::vector<BlockInfo>> blockHashesResult = dest.getFileBlockHashes();

    

    // 3. Send the block hashes to the client
    if (!dataPipe.serializeAndSendBlockHashes(clientSocket, blockHashesResult.data)) {
        std::cerr << "Failed to send block hashes\n";
        return false;
    }

    // 4. Receive delta from client
    std::vector<DeltaInstruction> delta;
    if (!dataPipe.receiveDelta(clientSocket, delta)) {
        std::cerr << "Failed to receive delta\n";
        return false;
    }

    // 5. Apply delta to remote file
    dest.applyDelta(delta);

    return true;
}

bool ServerMode::pullTransaction(int clientSocket){
    std::string remotePath;

    DataTransfer dataPipe;
    // 1. Receive remote file path
    if (!dataPipe.receiveFilePath(clientSocket,remotePath)){ 
        std::cerr << "Failed to receive remote path\n";
        return false;
    }

    // recieve the block hashes
    std::vector<BlockInfo> blockHashes;
    if(dataPipe.receiveBlockHashes(clientSocket,blockHashes)){
        std::cout<<"Block Hashes recieved successfully\n";
    }else{
        std::cerr<<"Error while recieving block hashes\n";
        return false;
    }

    // generate deltas
    SourceManager source(remotePath,blockHashes,8);  // 8 is taken as block size
    Result<std::vector<DeltaInstruction>> deltaResult=source.getDelta();
    // send this deltas
    if(dataPipe.serializeAndSendDeltaInstructions(clientSocket,deltaResult.data)){
        std::cout<<"Deltas sent successfully\n";
    }else{
        std::cerr<<"Error while sending deltas\n";
        return false;
    }

    return true;

}