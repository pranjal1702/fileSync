#pragma once
#include<string>
#include<vector>
#include "common/block_info.hpp"
#include "common/delta_instruction.hpp"
class DestinationManager{
public:
    DestinationManager(const std::string& destinationPath,size_t blockSize);
    std::vector<BlockInfo> getFileBlockHashes() const;
    void applyDelta(const std::vector<DeltaInstruction>& delta);
private:
    std::string destPath_;
    size_t blockSize_;
};