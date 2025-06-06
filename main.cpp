#include "sync/sync_engine.hpp"
#include<iostream>
int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Insufficient arguments\n";
        return 1;
    }

    std::string sourcePath = argv[1],destPath=argv[2];
    size_t blockSize = 4; // for easy testing

    SyncEngine syncFiles(sourcePath,destPath,blockSize);
    syncFiles.syncFile();

    return 0;
}
