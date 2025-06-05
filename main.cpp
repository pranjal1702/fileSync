#include "destination/destination_manager.hpp"
#include<iostream>
int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <destination_file>\n";
        return 1;
    }

    std::string destPath = argv[1];
    size_t blockSize = 4; // for easy testing

    DestinationManager dest(destPath, blockSize);
    auto blocks = dest.getFileBlockHashes();

    for (const auto& block : blocks) {
        std::cout << "Offset: " << block.offset
                  << "  WeakHash: " << block.weakHash << '\n';
    }

    return 0;
}
