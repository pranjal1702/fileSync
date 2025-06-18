#pragma once
#include "../common/block_info.hpp"
#include<string>
#include<vector>
class ClientSession {
public:
    ClientSession(const std::string& ip, int port, int id);
    ~ClientSession();
    
    bool connectToServer();
    void close();
    bool isConnected() const;

    void runTransaction(const std::string& filepath); // one interaction
    std::string getInfo() const;

private:
    std::string ip_;
    int port_;
    int sessionId_;
    int socketFD_;
    bool connected_;

    bool sendFilePath(const std::string& path);
    bool receiveBlockHashes(std::vector<BlockInfo>& blocks);
};
