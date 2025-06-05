#pragma once
#include<string>
#include<vector>
#include <unordered_map>
#include "../common/block_info.hpp"
#include "../common/delta_instruction.hpp"

class SourceManager{
public:
    SourceManager(const std::string& sourcePath,const std::vector<BlockInfo>& destBlocks,size_t blockSize);
    std::vector<DeltaInstruction> getDelta() const;

private:
    std::string sourcePath_;
    std::vector<BlockInfo> destBlocks_;
    size_t blockSize_;
};