#include "algo_zero.h"
#include <cstring>

namespace zero {


Bytes compress(const Words& in) {
    const size_t N = in.size();
    Bytes out;
    out.reserve(4 + (N / 8) + 1 + N * 8);

    uint32_t n32 = static_cast<uint32_t>(N);
    const uint8_t* n32_bytes = reinterpret_cast<uint8_t*>(&n32);
    out.insert(out.end(), n32_bytes, n32_bytes + 4);

    const size_t bitmap_bytes = (N + 7) / 8;
    const size_t bitmap_offset = out.size();
    out.resize(out.size() + bitmap_bytes, 0x00);

    for (size_t i = 0; i < N; ++i) {
        if (in[i] != 0) {
            out[bitmap_offset + i / 8] |= static_cast<uint8_t>(1 << (i % 8));

            uint64_t w = in[i];
            const uint8_t* w_bytes = reinterpret_cast<uint8_t*>(&w);
            out.insert(out.end(), w_bytes, w_bytes + 8);
        }
    }

    return out;
}


Bytes decompress_raw(const Bytes& in);

Words decompress(const Bytes& in) {
    uint32_t N;
    std::memcpy(&N, in.data(), 4);

    const size_t bitmap_bytes  = (N + 7) / 8;
    const size_t bitmap_offset = 4;
    const size_t payload_offset = bitmap_offset + bitmap_bytes;

    Words out(N, 0);
    size_t payload_pos = payload_offset;

    for (size_t i = 0; i < N; ++i) {
        const bool non_zero = (in[bitmap_offset + i / 8] >> (i % 8)) & 1;
        if (non_zero) {
            std::memcpy(&out[i], in.data() + payload_pos, 8);
            payload_pos += 8;
        }
    }

    return out;}
}
/*
 * Implements Zero-Value Compression (ZVC). Compression writes a 4-byte word
 * count header, a packed bitmap (one bit per input word, 1 = non-zero), and
 * then only the non-zero words as a dense payload. Decompression reads the
 * header, walks the bitmap, and reconstructs zero words from the bitmap and
 * non-zero words from the payload. Effective for sparse data such as
 * post-ReLU neural network activations.
 */
