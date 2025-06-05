#include "destination_manager.hpp"
#include <fstream>
#include<iostream>
#include "../common/hash_utils.hpp"

DestinationManager::DestinationManager(const std::string& destinationPath,size_t blockSize) : destPath_(destinationPath),blockSize_(blockSize) {}

std::vector<BlockInfo> DestinationManager::getFileBlockHashes() const{
    std::ifstream file(destPath_, std::ios::binary);
    std::vector<BlockInfo> blocks;

    if (!file) {
        std::cerr << "Error opening destination file: " << destPath_ << "\n";
        return blocks;
    }

    size_t offset = 0;
    std::vector<char> buffer(blockSize_);

    while (file.read(buffer.data(), blockSize_) || file.gcount() > 0) {
        size_t bytesRead = file.gcount();
        uint32_t weakHash = HashUtils::computeWeakHash(buffer.data(), bytesRead);
        std::string strongHash = ""; 
        blocks.emplace_back(offset, weakHash, strongHash);
        offset += bytesRead;
    }

    return blocks;
}