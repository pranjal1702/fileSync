#pragma once
#include "block_info.hpp"
#include "delta_instruction.hpp"
#include<vector>
class DataTransfer{
public:
    bool serializeAndSendBlockHashes(const int socket, const std::vector<BlockInfo>& blockHashes);
    bool receiveBlockHashes(const int socketFD, std::vector<BlockInfo>& blocks);
    bool serializeAndSendDeltaInstructions(int socket, const std::vector<DeltaInstruction>& delta);
    bool receiveDelta(int socket, std::vector<DeltaInstruction>& delta);
    bool sendFilePath(int socketFD, const std::string& filePath);
    bool receiveFilePath(int socketFD, std::string& filePath);
private:
    bool sendAll(int socket, const void* buffer, size_t length);
    bool recvAll(int socketFD,void* buffer, size_t length);
};