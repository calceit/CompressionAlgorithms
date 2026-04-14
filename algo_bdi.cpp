// algo_bdi.cpp — Base-Delta-Immediate (BDI) Compression implementation
// similar to delta but we're storing one value & then deltas compared to that one value
// say, base = 100, rest of the values are say: 103, 105, 99, ..
// it'll store deltas of the rest of values vs base
// deltas: 3, 5, -1
// since all deltas are smol, they can be put in int16 (2 bytes), so instead of storing 64 bytes,
// we store 1 byte (tag), 8 bytes (base), 8 x 2 bytes (deltas) -> 25 bytes
// it can be done parallel-ly (delta can't)
// the tag byte says if the data stored is 8 bytes of base & 16 bytes of deltas OR if the data is uncompressed
// Read algo_bdi.h for the full description of the algorithm and format.

#include "algo_bdi.h"
#include <cstring>
#include <algorithm>
#include <cstdint>

namespace bdi {


Bytes compress(const Words& in) {
    const size_t N = in.size();
    Bytes out;
    out.reserve(N * 9);

    uint32_t n32 = static_cast<uint32_t>(N);
    out.insert(out.end(), reinterpret_cast<uint8_t*>(&n32),
                          reinterpret_cast<uint8_t*>(&n32) + 4);

    for (size_t i = 0; i < N; i += BLOCK_SIZE) {
        const size_t blk = std::min(BLOCK_SIZE, N - i);

        const uint64_t base = in[i];

        bool fits = (blk == BLOCK_SIZE);
        if (fits) {
            for (size_t j = 1; j < blk; ++j) {
                int64_t d = static_cast<int64_t>(in[i + j])
                          - static_cast<int64_t>(base);
                if (d < INT16_MIN || d > INT16_MAX) {
                    fits = false;
                    break;
                }
            }
        }

        if (fits) {
            out.push_back(0x01);

            out.insert(out.end(), reinterpret_cast<const uint8_t*>(&base),
                                  reinterpret_cast<const uint8_t*>(&base) + 8);

            for (size_t j = 0; j < blk; ++j) {
                int16_t d = static_cast<int16_t>(
                    static_cast<int64_t>(in[i + j]) -
                    static_cast<int64_t>(base));
                out.insert(out.end(), reinterpret_cast<uint8_t*>(&d),
                                      reinterpret_cast<uint8_t*>(&d) + 2);
            }
        } else {
            out.push_back(0x00);
            for (size_t j = 0; j < blk; ++j) {
                out.insert(out.end(),
                    reinterpret_cast<const uint8_t*>(&in[i + j]),
                    reinterpret_cast<const uint8_t*>(&in[i + j]) + 8);
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

    while (out.size() < N) {
        const uint8_t tag = in[pos++];

        if (tag == 0x01) {
            uint64_t base;
            std::memcpy(&base, in.data() + pos, 8);
            pos += 8;

            for (size_t j = 0; j < BLOCK_SIZE; ++j) {
                int16_t d;
                std::memcpy(&d, in.data() + pos, 2);
                pos += 2;
                out.push_back(static_cast<uint64_t>(
                    static_cast<int64_t>(base) + d));
            }
        } else {
            const size_t blk = std::min((size_t)BLOCK_SIZE, N - out.size());
            for (size_t j = 0; j < blk; ++j) {
                uint64_t w;
                std::memcpy(&w, in.data() + pos, 8);
                pos += 8;
                out.push_back(w);
            }
        }
    }

    return out;
}

}

/*
 * Implements Base-Delta-Immediate (BDI) Compression. Input is processed in
 * fixed blocks of BLOCK_SIZE words. For each block the first word is the base;
 * if all deltas relative to that base fit in int16, the block is stored
 * compressed as tag 0x01 + 8-byte base + BLOCK_SIZE×2-byte int16 deltas.
 * Partial blocks and blocks with large deltas are stored raw as tag 0x00 +
 * the unmodified words. Unlike sequential delta encoding, all deltas within a
 * block are independent of each other, enabling parallel decompression.
 * Highly effective for blocks of slowly varying values (sensor streams).
 */
