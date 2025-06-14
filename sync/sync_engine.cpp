#include "sync_engine.hpp"
#include "../destination/destination_manager.hpp"
#include "../source/source_manager.hpp"
#include<iostream>
SyncEngine:: SyncEngine(const std::string& sourcePath, const std::string& destPath, size_t blockSize):sourcePath_(sourcePath),destPath_(destPath),blockSize_(blockSize){}

void SyncEngine::syncFile(){
    DestinationManager dest(destPath_, blockSize_);
    auto blocks = dest.getFileBlockHashes();
    SourceManager src(sourcePath_,blocks,blockSize_);
    auto deltaData=src.getDelta();
    dest.applyDelta(deltaData);
    std::cout<<"Sync Completed\n";
}