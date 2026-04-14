#include "algo_bpc.h"
#include <cstring>
#include <algorithm>

namespace bpc {

static void transpose_to_planes(const uint64_t* words, uint64_t* planes) {
    for (int b = 0; b < 64; ++b) {
        uint64_t plane = 0;
        for (int w = 0; w < (int)BLOCK_WORDS; ++w) {
            uint64_t bit = (words[w] >> b) & 1ULL;
            plane |= (bit << w);
        }
        planes[b] = plane;
    }
}

static void transpose_from_planes(const uint64_t* planes, uint64_t* words) {
    for (int w = 0; w < (int)BLOCK_WORDS; ++w) {
        uint64_t word = 0;
        for (int b = 0; b < 64; ++b) {
            uint64_t bit = (planes[b] >> w) & 1ULL;
            word |= (bit << b);
        }
        words[w] = word;
    }
}

Bytes compress(const Words& in) {
    const size_t N = in.size();
    Bytes out;
    out.reserve(N * 9);

    uint32_t n32 = static_cast<uint32_t>(N);
    out.insert(out.end(), reinterpret_cast<uint8_t*>(&n32),
                          reinterpret_cast<uint8_t*>(&n32) + 4);

    uint64_t block[BLOCK_WORDS];
    uint64_t planes[64];

    for (size_t i = 0; i < N; i += BLOCK_WORDS) {
        size_t blk = std::min(BLOCK_WORDS, N - i);

        std::memcpy(block, in.data() + i, blk * 8);
        if (blk < BLOCK_WORDS)
            std::memset(block + blk, 0, (BLOCK_WORDS - blk) * 8);

        transpose_to_planes(block, planes);

        uint64_t zero_mask = 0;
        for (int b = 0; b < 64; ++b)
            if (planes[b] == 0)
                zero_mask |= (1ULL << b);

        out.insert(out.end(), reinterpret_cast<uint8_t*>(&zero_mask),
                              reinterpret_cast<uint8_t*>(&zero_mask) + 8);

        for (int b = 0; b < 64; ++b) {
            if (!(zero_mask & (1ULL << b))) {
                out.insert(out.end(),
                    reinterpret_cast<uint8_t*>(&planes[b]),
                    reinterpret_cast<uint8_t*>(&planes[b]) + 8);
            }
        }
    }

    return out;
}

Words decompress(const Bytes& in) {
    uint32_t N;
    std::memcpy(&N, in.data(), 4);

    Words out;
    out.reserve(N);
    size_t pos = 4;

    uint64_t planes[64];
    uint64_t block[BLOCK_WORDS];

    while (out.size() < N) {
        size_t blk = std::min(BLOCK_WORDS, N - out.size());

        uint64_t zero_mask;
        std::memcpy(&zero_mask, in.data() + pos, 8);
        pos += 8;

        for (int b = 0; b < 64; ++b) {
            if (zero_mask & (1ULL << b)) {
                planes[b] = 0;
            } else {
                std::memcpy(&planes[b], in.data() + pos, 8);
                pos += 8;
            }
        }

        transpose_from_planes(planes, block);

        for (size_t j = 0; j < blk; ++j)
            out.push_back(block[j]);
    }

    return out;
}

}

/*
 * Implements Bit-Plane Compression (BPC). Input words are processed in blocks
 * of BLOCK_WORDS. Each block is transposed into 64 bit-planes where plane[b]
 * holds bit b from every word in the block. A 64-bit zero-plane mask records
 * which planes are entirely zero; only non-zero planes are stored. The
 * compressed block is the 8-byte zero-mask followed by the packed non-zero
 * planes. Decompression reads the mask, reconstructs all 64 planes (zeroing
 * masked ones), then transposes back to words. Effective when many bit
 * positions are uniformly zero across a block, as seen in small-integer data.
 */
