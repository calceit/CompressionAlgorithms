// algo_bpc_spec.cpp — Bit-Plane Compression (Spec: DBX variant)
//
// Read algo_bpc_spec.h for the full description of the DBX pipeline.

#include "algo_bpc_spec.h"
#include <cstring>
#include <algorithm>

namespace bpc_spec {

static void delta_encode(const uint64_t* words, uint64_t* deltas, size_t n) {
    deltas[0] = words[0];
    for (size_t i = 1; i < n; ++i)
        deltas[i] = words[i] - words[i - 1];
}

static void delta_decode(const uint64_t* deltas, uint64_t* words, size_t n) {
    words[0] = deltas[0];
    for (size_t i = 1; i < n; ++i)
        words[i] = words[i - 1] + deltas[i];
}

static void transpose_to_planes(const uint64_t* words, uint64_t* planes) {
    for (int b = 0; b < 64; ++b) {
        uint64_t plane = 0;
        for (int w = 0; w < (int)BLOCK_WORDS; ++w)
            plane |= (((words[w] >> b) & 1ULL) << w);
        planes[b] = plane;
    }
}

static void transpose_from_planes(const uint64_t* planes, uint64_t* words) {
    for (int w = 0; w < (int)BLOCK_WORDS; ++w) {
        uint64_t word = 0;
        for (int b = 0; b < 64; ++b)
            word |= (((planes[b] >> w) & 1ULL) << b);
        words[w] = word;
    }
}

static void xor_encode(uint64_t* planes) {
    for (int b = 63; b >= 1; --b)
        planes[b] ^= planes[b - 1];
}

static void xor_decode(uint64_t* planes) {
    for (int b = 1; b < 64; ++b)
        planes[b] ^= planes[b - 1];
}

Bytes compress(const Words& in) {
    const size_t N = in.size();
    Bytes out;
    out.reserve(N * 9);

    uint32_t n32 = static_cast<uint32_t>(N);
    out.insert(out.end(), reinterpret_cast<uint8_t*>(&n32),
                          reinterpret_cast<uint8_t*>(&n32) + 4);

    uint64_t block[BLOCK_WORDS];
    uint64_t deltas[BLOCK_WORDS];
    uint64_t planes[64];

    for (size_t i = 0; i < N; i += BLOCK_WORDS) {
        size_t blk = std::min(BLOCK_WORDS, N - i);

        std::memcpy(block, in.data() + i, blk * 8);
        if (blk < BLOCK_WORDS) {
            uint64_t pad_val = block[blk - 1];
            for (size_t j = blk; j < BLOCK_WORDS; ++j)
                block[j] = pad_val;
        }

        delta_encode(block, deltas, BLOCK_WORDS);
        transpose_to_planes(deltas, planes);
        xor_encode(planes);

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
    uint64_t deltas[BLOCK_WORDS];
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

        xor_decode(planes);
        transpose_from_planes(planes, deltas);
        delta_decode(deltas, block, BLOCK_WORDS);

        for (size_t j = 0; j < blk; ++j)
            out.push_back(block[j]);
    }

    return out;
}

}

/*
 * Implements spec-compliant Bit-Plane Compression using the DBX pipeline from
 * Kim et al. ISCA 2016. Each 64-word block is delta-encoded (converting words
 * to consecutive differences), transposed into 64 bit-planes, then XOR-encoded
 * across adjacent planes (planes[b] ^= planes[b-1], high to low). A 64-bit
 * zero-plane mask records which planes are all-zero after DBX; only non-zero
 * planes are stored. Partial blocks are padded with the last value so that
 * padding deltas are zero and do not pollute the planes. Decompression applies
 * the inverse: un-XOR (low to high), inverse transpose, then prefix-sum
 * integration of deltas back to original words.
 */
