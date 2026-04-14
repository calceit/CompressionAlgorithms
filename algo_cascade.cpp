#include "algo_cascade.h"
#include <cstring>
#include <algorithm>
#include <limits>

namespace cascade {

Bytes compress(const Words& in) {
    const size_t N = in.size();
    Bytes out;
    out.reserve(N * 9);

    uint32_t n32 = static_cast<uint32_t>(N);
    out.insert(out.end(), reinterpret_cast<uint8_t*>(&n32),
                          reinterpret_cast<uint8_t*>(&n32) + 4);

    for (size_t i = 0; i < N; i += BLOCK_SIZE) {
        const size_t blk = std::min(BLOCK_SIZE, N - i);

        bool all_zero = true;
        for (size_t j = 0; j < blk; ++j)
            if (in[i + j] != 0) { all_zero = false; break; }

        if (all_zero) {
            out.push_back(0x00);
            continue;
        }

        const uint64_t base = in[i];
        bool bdi_fits = (blk == BLOCK_SIZE);
        if (bdi_fits) {
            for (size_t j = 1; j < blk; ++j) {
                int64_t d = static_cast<int64_t>(in[i + j])
                          - static_cast<int64_t>(base);
                if (d < INT16_MIN || d > INT16_MAX) { bdi_fits = false; break; }
            }
        }

        if (bdi_fits) {
            out.push_back(0x01);
            out.insert(out.end(), reinterpret_cast<const uint8_t*>(&base),
                                  reinterpret_cast<const uint8_t*>(&base) + 8);
            for (size_t j = 0; j < blk; ++j) {
                int16_t d = static_cast<int16_t>(
                    static_cast<int64_t>(in[i + j]) - static_cast<int64_t>(base));
                out.insert(out.end(), reinterpret_cast<uint8_t*>(&d),
                                      reinterpret_cast<uint8_t*>(&d) + 2);
            }
            continue;
        }

        out.push_back(0x02);
        for (size_t j = 0; j < blk; ++j)
            out.insert(out.end(),
                reinterpret_cast<const uint8_t*>(&in[i + j]),
                reinterpret_cast<const uint8_t*>(&in[i + j]) + 8);
    }

    return out;
}

Words decompress(const Bytes& in) {
    uint32_t N;
    std::memcpy(&N, in.data(), 4);

    Words out;
    out.reserve(N);
    size_t pos = 4;

    while (out.size() < N) {
        const size_t remaining = N - out.size();
        const uint8_t tag = in[pos++];

        if (tag == 0x00) {
            const size_t blk = std::min(BLOCK_SIZE, remaining);
            for (size_t j = 0; j < blk; ++j) out.push_back(0);

        } else if (tag == 0x01) {
            uint64_t base;
            std::memcpy(&base, in.data() + pos, 8); pos += 8;
            for (size_t j = 0; j < BLOCK_SIZE; ++j) {
                int16_t d;
                std::memcpy(&d, in.data() + pos, 2); pos += 2;
                out.push_back(static_cast<uint64_t>(
                    static_cast<int64_t>(base) + d));
            }

        } else {
            const size_t blk = std::min(BLOCK_SIZE, remaining);
            for (size_t j = 0; j < blk; ++j) {
                uint64_t w;
                std::memcpy(&w, in.data() + pos, 8); pos += 8;
                out.push_back(w);
            }
        }
    }

    return out;
}

}

/*
 * Implements a Zero-Value + BDI cascade operating on 64-byte blocks
 * (8 × uint64 words, matching a cache line). For each block, compression
 * first checks for all-zero (tag 0x00, 1 byte total); then attempts BDI
 * with int16 deltas (tag 0x01, 25 bytes total); then falls back to raw
 * storage (tag 0x02, 65 bytes total). Decompression selects the correct
 * path from the single tag byte — the hardware equivalent is a one-hot
 * mux. This is the primary proposed DMA compression algorithm for edge
 * NPU activation transfer.
 */
