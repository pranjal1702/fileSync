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
        std::string strongHash = HashUtils::computeStrongHash(buffer.data(),bytesRead);
        blocks.emplace_back(offset, weakHash, strongHash);
        offset += bytesRead;
    }

    return blocks;
}

void DestinationManager::applyDelta(const std::vector<DeltaInstruction>& deltas){
    std::ifstream oldFile(destPath_, std::ios::binary);
    std::string tempFilePath = destPath_ + ".sync.tmp";
    std::ofstream tempFile(tempFilePath, std::ios::binary | std::ios::trunc);

    if (!oldFile.is_open() || !tempFile.is_open()) {
        std::cerr << "File open failed during delta apply\n";
        return;
    }

    for (const auto& delta : deltas) {
        if (delta.type == DeltaType::COPY) {
            oldFile.seekg(delta.offset, std::ios::beg);
            std::vector<char> buffer(blockSize_);
            oldFile.read(buffer.data(), blockSize_);
            tempFile.write(buffer.data(), oldFile.gcount());
        } else if (delta.type == DeltaType::INSERT) {
            tempFile.write(delta.data.data(), delta.data.size());
        }
    }

    oldFile.close();
    tempFile.close();

    // Atomic swap
    std::remove(destPath_.c_str());                // Delete old file
    std::rename(tempFilePath.c_str(), destPath_.c_str()); // Replace with new

}