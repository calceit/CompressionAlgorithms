#include "algo_delta.h"
#include <cstring>

namespace delta {

Bytes compress(const Words& in) {
    const size_t N = in.size();
    Bytes out;
    out.reserve(4 + 8 + N * 9);

    uint32_t n32 = static_cast<uint32_t>(N);
    out.insert(out.end(), reinterpret_cast<uint8_t*>(&n32),
                          reinterpret_cast<uint8_t*>(&n32) + 4);

    if (N == 0) return out;

    uint64_t prev = in[0];
    out.insert(out.end(), reinterpret_cast<uint8_t*>(&prev),
                          reinterpret_cast<uint8_t*>(&prev) + 8);

    for (size_t i = 1; i < N; ++i) {
        int64_t delta = static_cast<int64_t>(in[i])
                      - static_cast<int64_t>(prev);

        if (delta >= INT16_MIN && delta <= INT16_MAX) {
            uint8_t tag = 0x01;
            int16_t d16 = static_cast<int16_t>(delta);
            out.push_back(tag);
            out.insert(out.end(), reinterpret_cast<uint8_t*>(&d16),
                                  reinterpret_cast<uint8_t*>(&d16) + 2);
        } else {
            uint8_t tag = 0x00;
            out.push_back(tag);
            out.insert(out.end(), reinterpret_cast<const uint8_t*>(&in[i]),
                                  reinterpret_cast<const uint8_t*>(&in[i]) + 8);
        }

        prev = in[i];
    }

    return out;
}

Words decompress(const Bytes& in) {
    uint32_t N;
    std::memcpy(&N, in.data(), 4);

    Words out(N);
    if (N == 0) return out;

    size_t pos = 4;

    std::memcpy(&out[0], in.data() + pos, 8);
    pos += 8;

    for (size_t i = 1; i < N; ++i) {
        uint8_t tag = in[pos++];

        if (tag == 0x01) {
            int16_t d16;
            std::memcpy(&d16, in.data() + pos, 2);
            pos += 2;
            out[i] = static_cast<uint64_t>(
                         static_cast<int64_t>(out[i - 1]) + d16);
        } else {
            std::memcpy(&out[i], in.data() + pos, 8);
            pos += 8;
        }
    }

    return out;
}

}

/*
 * Implements sequential Delta Encoding. The first word is stored raw; each
 * subsequent word is encoded as a tagged record: tag 0x01 followed by a 2-byte
 * int16 delta if the difference from the previous word fits in 16 bits, or
 * tag 0x00 followed by the full 8-byte raw word otherwise. Decompression
 * reconstructs each word by adding the delta to the running previous value.
 * Highly effective for slowly varying streams (e.g. sensor data) and
 * ineffective for random data.
 */
