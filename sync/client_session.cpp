#include "client_session.hpp"
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
    }

    // after each transaction close the client session
    std::cout << "[Session " << sessionId_ << "] Closing client session\n";
    connected_ = false;
    std::cout<<">>> "<<std::flush;
    ::close(socketFD_);
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

bool ClientSession::recievingStatus(DataTransfer &dataPipe) {
    // it returns true only when status is successfull
    // in all other cases it returns false and socket get closed
    // it prints message in all cases
    StatusMessage message(false, "Unknown error occurred");
    if (dataPipe.recieveStatus(socketFD_, message)) {
        std::cout << "\033[34m[Session " << sessionId_ << ": Server] " << message.msg << "\033[0m\n";
        std::cout<<">>> ";
        return message.status;
    } else {
        std::cerr << "\033[34m[Session " << sessionId_ << ": Server] Error while receiving status\033[0m\n";
        std::cout<<">>> ";
        return false;
    }
}
void printClientMessage(int sessionId, const std::string& message) {
    std::cout << "\033[36m[Session " << sessionId << ": Client] " << message << "\033[0m\n";
    std::cout<<">>> ";
}
bool ClientSession::pushTransaction(const std::string& loaclPath,const std::string& remotePath){
    std::string command = "PUSH\n";
    if (send(socketFD_, command.c_str(), command.size(), 0) != (ssize_t)command.size()) {
        printClientMessage(sessionId_,"Failed to send command");
        return false;
    }
    DataTransfer dataPipe; // used for data transfer

    if(!recievingStatus(dataPipe)) return false; // status for command part
    // sending the remotePath
    
    if(dataPipe.sendFilePath(socketFD_,remotePath)){
        printClientMessage(sessionId_,"Push request send to the remote machine");
    }else{
        printClientMessage(sessionId_,"Failed to send remote file path");
        return false;
    }

    if(!recievingStatus(dataPipe)) return false;  // status for file path

    // recieving the status for whether hash generation has been successfull on server side or not
    if(!recievingStatus(dataPipe)) return false;  
    
    // need to recieve the blockHashes from the server
    std::vector<BlockInfo> blocks;  // this is the blockHashes
    if(dataPipe.receiveBlockHashes(socketFD_,blocks)){
        printClientMessage(sessionId_,"Hashes of "+std::to_string(blocks.size())+" blocks recieved");
    }else{
        printClientMessage(sessionId_,"Failed to recieve block hashes");
        return false;
    }

    // now using this need to generate the delta and send it to the server
    SourceManager source(loaclPath,blocks); 

    printClientMessage(sessionId_,"Generating the delta instructions");
    Result<std::vector<DeltaInstruction>> deltaResult=source.getDelta(); 
    // this delta is need to be send over the network
    if(!deltaResult.success){
        printClientMessage(sessionId_,"Failed to Generate Delta: "+deltaResult.message);
        return false;
    }

    if(dataPipe.serializeAndSendDeltaInstructions(socketFD_,deltaResult.data)){
        printClientMessage(sessionId_,"Delta instructions send successfully");
    }else{
        printClientMessage(sessionId_,"Failed to send delta");
        return false;
    }

    if(!recievingStatus(dataPipe)) return false; // whether deltas are successfully recieved or not

    if(!recievingStatus(dataPipe)) return false;  // whether delta application was succesfull or not

    printClientMessage(sessionId_,"Content Pushed to the remote file");
    
    return true;
}

bool ClientSession::pullTransaction(const std::string& loaclPath,const std::string& remotePath){
    std::string command = "PULL\n";
    if (send(socketFD_, command.c_str(), command.size(), 0) != (ssize_t)command.size()) {
        printClientMessage(sessionId_,"Failed to send command");
        return false;
    }
    DataTransfer dataPipe; // used for data transfer

    if(!recievingStatus(dataPipe)) return false; // status for command part

    // sending the remotePath
    
    if(dataPipe.sendFilePath(socketFD_,remotePath)){
        printClientMessage(sessionId_,"Push request send to the remote machine");
    }else{
        printClientMessage(sessionId_,"Failed to send remote file path");
        return false;
    }

    if(!recievingStatus(dataPipe)) return false; // status for remote file path

    DestinationManager destination(loaclPath);  // using block size = 8
    // now this local machine will generate the hashes
    printClientMessage(sessionId_,"Generating the block hashes...");
    Result<std::vector<BlockInfo>> blockHashesResult= destination.getFileBlockHashes();
    if(!blockHashesResult.success){
        printClientMessage(sessionId_,"Error while generating block hashes:: "+blockHashesResult.message);
        return false;
    }
    // sending this block hashes
    printClientMessage(sessionId_,"Sending block hashes...");
    if(dataPipe.serializeAndSendBlockHashes(socketFD_,blockHashesResult.data)){
        printClientMessage(sessionId_,"Block Hashes sent successfully");
    }else{
        printClientMessage(sessionId_,"Failed to send block hashes");
        return false;
    }

    if(!recievingStatus(dataPipe)) return false; // status for whether block hashes are successfully recieved or not

    if(!recievingStatus(dataPipe)) return false; // status for whether delta calculated succesfully or not
    // recieve the delta instructions
    std::vector<DeltaInstruction> deltas;
    if(dataPipe.receiveDelta(socketFD_,deltas)){
        printClientMessage(sessionId_,"Delta Instructions recieved successfully");
    }else{
        printClientMessage(sessionId_,"Failed to get delta instructions");
        return false;
    }

    // now apply this delta
    Result<void> applyDetaRes= destination.applyDelta(deltas);
    if(applyDetaRes.success){
        printClientMessage(sessionId_,"Content pulled successfully");
    }else{
        printClientMessage(sessionId_,"Error while applying delta instructions");
    }
    return applyDetaRes.success;

}