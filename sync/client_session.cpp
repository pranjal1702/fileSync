#include "client_session.hpp"
#include "../common/data_transfer.hpp"
#include "../source/source_manager.hpp"
#include "../destination/destination_manager.hpp"
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
void ClientSession::runTransaction(const std::string request,const std::string& localPath,const std::string&remotePath) {
    if (!connected_) {
        std::cerr << "[Session " << sessionId_ << "] Not connected.\n";
        return;
    }

    if(request=="push"){
        pushTransaction(localPath,remotePath);
    }else if(request=="pull"){
        pullTransaction(localPath,remotePath);
    }else{
        std::cerr << "[Session " << sessionId_ << "] Invalid request.\n";
        return;
    }
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

bool ClientSession::pushTransaction(const std::string& loaclPath,const std::string& remotePath){
    std::string command = "PUSH\n";
    if (send(socketFD_, command.c_str(), command.size(), 0) != (ssize_t)command.size()) {
        std::cerr << "[Session " << sessionId_ << "] Failed to send command\n";
        return false;
    }
    DataTransfer dataPipe; // used for data transfer

    // sending the remotePath
    
    if(dataPipe.sendFilePath(socketFD_,remotePath)){
        std::cout<<"Push request send to the remote machine\n";
    }else{
        std::cerr << "[Session " << sessionId_ << "] Failed to send remote file path\n";
        return false;
    }

    // need to recieve the blockHashes from the server
    std::vector<BlockInfo> blocks;  // this is the blockHashes
    if(dataPipe.receiveBlockHashes(socketFD_,blocks)){
        std::cout<<"Hashes of "<<blocks.size()<<" blocks recieved";
    }else{
        std::cerr << "[Session " << sessionId_ << "] Failed to recieve block hashes\n";
        return false;
    }

    // now using this need to generate the delta and send it to the server
    SourceManager source(loaclPath,blocks,8);  // blockSize 8 as of now

    std::cout<<"Generating the delta instructions\n";
    Result<std::vector<DeltaInstruction>> deltaResult=source.getDelta(); 
    // this delta is need to be send over the network

    if(dataPipe.serializeAndSendDeltaInstructions(socketFD_,deltaResult.data)){
        std::cout<<"Delta instructions send successfully\n";
    }else{
        std::cerr << "[Session " << sessionId_ << "] Failed to send delta\n";
        return false;
    }

    std::cout<<"Content Pushed to the remote file\n";
    
    return true;
}

bool ClientSession::pullTransaction(const std::string& loaclPath,const std::string& remotePath){
    std::string command = "PULL\n";
    if (send(socketFD_, command.c_str(), command.size(), 0) != (ssize_t)command.size()) {
        std::cerr << "[Session " << sessionId_ << "] Failed to send command\n";
        return false;
    }
    DataTransfer dataPipe; // used for data transfer

    // sending the remotePath
    
    if(dataPipe.sendFilePath(socketFD_,remotePath)){
        std::cout<<"Push request send to the remote machine\n";
    }else{
        std::cerr << "[Session " << sessionId_ << "] Failed to send remote file path\n";
        return false;
    }

    DestinationManager destination(loaclPath,8);  // using block size = 8
    // now this local machine will generate the hashes
    std::cout<<"Generating the block hashes...\n";
    Result<std::vector<BlockInfo>> blockHashesResult= destination.getFileBlockHashes();

    // sending this block hashes
    std::cout<<"Sending block hashes...\n";
    if(dataPipe.serializeAndSendBlockHashes(socketFD_,blockHashesResult.data)){
        std::cout<<"Block Hashes sent successfully\n";
    }else{
        std::cerr << "[Session " << sessionId_ << "] Failed to send block hashes\n";
        return false;
    }

    // recieve the delta instructions
    std::vector<DeltaInstruction> deltas;
    if(dataPipe.receiveDelta(socketFD_,deltas)){
        std::cout<<"Delta Instructions recieved successfully\n";
    }else{
        std::cerr<<"[Session " << sessionId_ << "] Failed to get delta instructions\n";
        return false;
    }

    // now apply this delta
    destination.applyDelta(deltas);

    return true;

}