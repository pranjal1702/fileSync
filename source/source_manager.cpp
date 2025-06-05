#include "source_manager.hpp"
#include "../common/hash_utils.hpp"
#include <fstream>
#include<iostream>
#include<vector>
#include<deque>
SourceManager::SourceManager(const std::string& sourcePath,const std::vector<BlockInfo>& destBlocks,size_t blockSize) : sourcePath_(sourcePath),destBlocks_(std::move(destBlocks)),blockSize_(blockSize){}

std::vector<DeltaInstruction> SourceManager::getDelta() const{
    std::unordered_map<uint32_t, std::vector<BlockInfo>> destHashMap;
    // creating hashmap using destBlocks_
    for(auto blockInfo:destBlocks_){
        destHashMap[blockInfo.weakHash].push_back(blockInfo);
    }

    std::vector<DeltaInstruction> deltas;
    std::ifstream file(sourcePath_, std::ios::binary);
    if (!file) {
        std::cerr << "Error opening source file\n";
        return deltas;
    }

    std::vector<char> buffer(blockSize_);
    std::deque<char> window;
    std::vector<char> pendingInsert;  // to be inserted when no match has been found

    size_t offset = 0,startIndex=0;   // startIndex is for correct order in buffer due to circular shift
    file.read(buffer.data(), blockSize_);

    size_t bytesInWindow = file.gcount();
    if (bytesInWindow == 0) return deltas;

    uint32_t hash = HashUtils::computeWeakHash(buffer.data(), bytesInWindow);

    const uint64_t base = 257;
    const uint64_t mod = 1e9 + 7;
    uint64_t basePow = 1;
    for (size_t i = 1; i < blockSize_; ++i)
        basePow = (basePow * base) % mod;

    while(true){
        if(bytesInWindow<blockSize_){
            // just insert that complete window pendingInsert and that will be at the end of file
            for(size_t ind=0;ind<bytesInWindow;ind++) pendingInsert.push_back(buffer[ind]);
            break;
        }

        bool matched=false;
        auto it=destHashMap.find(hash);
        if(it!=destHashMap.end()){
            // weak hash matches something lets confirm with strong hash
            std::vector<char> snapshot(blockSize_);
            for (size_t i = 0; i < blockSize_; ++i) {
                snapshot[i] = buffer[(startIndex + i) % blockSize_];
            }
            std::string strongHashForWindow=HashUtils::computeStrongHash(snapshot.data(),blockSize_);
            for(const auto& candidate:it->second){
                if(candidate.strongHash==strongHashForWindow){
                    // exact match found
                    matched=true;
                    if (!pendingInsert.empty()) {
                        // these characters are not matched and need to be inserted
                        deltas.push_back(DeltaInstruction::makeInsert(pendingInsert));
                        pendingInsert.clear();
                    }
                    deltas.push_back(DeltaInstruction::makeCopy(candidate.offset));
                    break;
                }
            }
        }

        if(matched){
            // then need to skip the offset by window size
            offset += blockSize_;
            file.clear();
            file.seekg(offset);
            file.read(buffer.data(), blockSize_);
            bytesInWindow = file.gcount();
            if (bytesInWindow==0) break;
            startIndex=0;
            hash = HashUtils::computeWeakHash(buffer.data(), bytesInWindow);
        }else{
            // emit 1 byte insert
            pendingInsert.push_back(buffer[startIndex]);

            // Slide window
            char outByte = buffer[startIndex];
            char inByte;
            file.read(&inByte, 1);
            if (!file) break;
            offset += 1;
            buffer[startIndex] = inByte;
            startIndex = (startIndex + 1) % blockSize_;
            
            hash = (mod + hash - (static_cast<uint64_t>(outByte) * basePow) % mod) % mod;
            hash = (hash * base + static_cast<unsigned char>(inByte)) % mod;
        }
    }
    if (!pendingInsert.empty())
        deltas.push_back(DeltaInstruction::makeInsert(pendingInsert));

    return deltas;
}