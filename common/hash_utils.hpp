#pragma once
#include <cstdint>
#include <string>

class HashUtils {
public:

    static uint32_t computeWeakHash(const char* data, size_t len) {
        const uint64_t base = 257;
        const uint64_t mod = 1000000007;

        uint64_t hash = 0;
        for (size_t i = 0; i < len; ++i) {
            hash = (hash * base + static_cast<unsigned char>(data[i])) % mod;
        }
        return static_cast<uint32_t>(hash);
    }

};
