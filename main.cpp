#include "destination/destination_manager.hpp"
#include "source/source_manager.hpp"
#include "common/delta_instruction.hpp"
#include<iostream>
int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Insufficient arguments\n";
        return 1;
    }

    std::string sourcePath = argv[1],destPath=argv[2];
    size_t blockSize = 4; // for easy testing

    DestinationManager dest(destPath, blockSize);
    auto blocks = dest.getFileBlockHashes();
    SourceManager src(sourcePath,blocks,blockSize);
    auto deltaData=src.getDelta();

    for (const auto& block : blocks) {
        std::cout << "Offset: " << block.offset
                  << "  WeakHash: " << block.weakHash << '\n';
    }

    for(const auto& d:deltaData){
        std::cout << "Type: ";
        if(d.type==DeltaType::INSERT) {
            std::cout<<"Insert ";
            for(auto x:d.data) std::cout<<x;
            std::cout<<'\n';
        }
        else {
            std::cout<<"copy Offset: "<<d.offset<<'\n';
        }
    }

    dest.applyDelta(deltaData);

    return 0;
}
