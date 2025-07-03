#include "sync_engine.hpp"
#include "../destination/destination_manager.hpp"
#include "../source/source_manager.hpp"
#include<iostream>
SyncEngine:: SyncEngine(const std::string& sourcePath, const std::string& destPath, size_t blockSize):sourcePath_(sourcePath),destPath_(destPath),blockSize_(blockSize){}

void SyncEngine::syncFile(){
    DestinationManager dest(destPath_);
    auto blocks = dest.getFileBlockHashes().data;
    SourceManager src(sourcePath_,blocks);
    auto deltaData=src.getDelta().data;
    dest.applyDelta(deltaData);
    std::cout<<"Sync Completed\n";
}