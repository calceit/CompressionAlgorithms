// algo_fpc.cpp — Frequent Pattern Compression implementation
// fpc looks at each 64-bit word & checks if the word matches one of a small set of patterns
// prefix   pattern                         payload
// 000      zero                            0 bytes
// 001      sign-extended 4-bit (-8 to +7)  1 byte
// 010      sign-extended byte              1 byte
// 011      sign-extended int16             2 bytes
// 100      halfword repeated 4x            2 bytes
//          (e.g. 0xABCDABCDABCD)
// 101      byte repeated                   1 byte
//          (e.g. 0x0101010101010101)
// 110      sign-extended int32             4 bytes
// 111      uncompressed (no match)         8 bytes
// so if we have something like 0x0000000000000005
// tag is 010 (sign extended byte)
// payload is 0x05 (1 byte)
// 1 byte instead of 8
// the tag for two words is stored in 1 byte for efficiency
// Read algo_fpc.h for the full description of the algorithm and format.

#include "algo_fpc.h"
#include <cstring>

namespace fpc {


static uint8_t encode_word(uint64_t w, Bytes& out) {

    if (w == 0)
        return 0;

    {
        uint8_t b = w & 0xFF;
        uint64_t rep = 0;
        for (int i = 0; i < 8; ++i)
            rep |= (static_cast<uint64_t>(b) << (i * 8));
        if (rep == w) {
            out.push_back(b);
            return 5;
        }
    }

    {
        uint16_t h = w & 0xFFFF;
        uint64_t rep = 0;
        for (int i = 0; i < 4; ++i)
            rep |= (static_cast<uint64_t>(h) << (i * 16));
        if (rep == w) {
            out.insert(out.end(), reinterpret_cast<uint8_t*>(&h),
                                  reinterpret_cast<uint8_t*>(&h) + 2);
            return 4;
        }
    }

    {
        int64_t sv = static_cast<int64_t>(w);
        if (sv >= -8 && sv <= 7) {
            uint8_t nibble = static_cast<uint8_t>(sv & 0x0F);
            out.push_back(nibble);
            return 1;
        }
    }

    {
        int64_t sv = static_cast<int64_t>(w);
        if (sv >= INT8_MIN && sv <= INT8_MAX) {
            out.push_back(static_cast<uint8_t>(static_cast<int8_t>(sv)));
            return 2;
        }
    }

    {
        int64_t sv = static_cast<int64_t>(w);
        if (sv >= INT16_MIN && sv <= INT16_MAX) {
            int16_t s16 = static_cast<int16_t>(sv);
            out.insert(out.end(), reinterpret_cast<uint8_t*>(&s16),
                                  reinterpret_cast<uint8_t*>(&s16) + 2);
            return 3;
        }
    }

    {
        int64_t sv = static_cast<int64_t>(w);
        if (sv >= INT32_MIN && sv <= INT32_MAX) {
            int32_t s32 = static_cast<int32_t>(sv);
            out.insert(out.end(), reinterpret_cast<uint8_t*>(&s32),
                                  reinterpret_cast<uint8_t*>(&s32) + 4);
            return 6;
        }
    }

    out.insert(out.end(), reinterpret_cast<const uint8_t*>(&w),
                          reinterpret_cast<const uint8_t*>(&w) + 8);
    return 7;
}


Bytes compress(const Words& in) {
    const size_t N = in.size();
    Bytes out;
    out.reserve(4 + N * 5);

    uint32_t n32 = static_cast<uint32_t>(N);
    out.insert(out.end(), reinterpret_cast<uint8_t*>(&n32),
                          reinterpret_cast<uint8_t*>(&n32) + 4);

    for (size_t i = 0; i < N; i += 2) {
        Bytes payload_a, payload_b;

        uint8_t tag_a = encode_word(in[i], payload_a);
        uint8_t tag_b = (i + 1 < N) ? encode_word(in[i + 1], payload_b) : 0;

        uint8_t tag_byte = (tag_a & 0x7) | ((tag_b & 0x7) << 3);
        out.push_back(tag_byte);

        out.insert(out.end(), payload_a.begin(), payload_a.end());
        if (i + 1 < N)
            out.insert(out.end(), payload_b.begin(), payload_b.end());
    }

    return out;
}


static uint64_t decode_word(uint8_t tag, const Bytes& in, size_t& pos) {
    switch (tag) {
        case 0:
            return 0;

        case 1: {
            uint8_t nibble = in[pos++] & 0x0F;
            int8_t v = (nibble & 0x08) ? static_cast<int8_t>(nibble | 0xF0)
                                       : static_cast<int8_t>(nibble);
            return static_cast<uint64_t>(static_cast<int64_t>(v));
        }

        case 2: {
            int8_t v;
            std::memcpy(&v, &in[pos], 1); pos++;
            return static_cast<uint64_t>(static_cast<int64_t>(v));
        }

        case 3: {
            int16_t v;
            std::memcpy(&v, &in[pos], 2); pos += 2;
            return static_cast<uint64_t>(static_cast<int64_t>(v));
        }

        case 4: {
            uint16_t h;
            std::memcpy(&h, &in[pos], 2); pos += 2;
            uint64_t r = 0;
            for (int i = 0; i < 4; ++i)
                r |= (static_cast<uint64_t>(h) << (i * 16));
            return r;
        }

        case 5: {
            uint8_t b = in[pos++];
            uint64_t r = 0;
            for (int i = 0; i < 8; ++i)
                r |= (static_cast<uint64_t>(b) << (i * 8));
            return r;
        }

        case 6: {
            int32_t v;
            std::memcpy(&v, &in[pos], 4); pos += 4;
            return static_cast<uint64_t>(static_cast<int64_t>(v));
        }

        default: {
            uint64_t v;
            std::memcpy(&v, &in[pos], 8); pos += 8;
            return v;
        }
    }
}


Words decompress(const Bytes& in) {
    uint32_t N;
    std::memcpy(&N, in.data(), 4);

    Words out;
    out.reserve(N);
    size_t pos = 4;

    for (size_t i = 0; i < N; i += 2) {
        uint8_t tag_byte = in[pos++];
        uint8_t tag_a    = tag_byte & 0x7;
        uint8_t tag_b    = (tag_byte >> 3) & 0x7;

        out.push_back(decode_word(tag_a, in, pos));
        if (i + 1 < N)
            out.push_back(decode_word(tag_b, in, pos));
    }

    return out;
}

}

/*
 * Implements Frequent Pattern Compression (FPC). Each 64-bit word is matched
 * against eight patterns in order of compactness (zero, repeated byte, repeated
 * halfword, sign-extended 4-bit/8-bit/16-bit/32-bit, uncompressed) and encoded
 * as a 3-bit tag plus a variable-length payload. Words are processed in pairs:
 * both 3-bit tags are packed into a single tag byte, followed by the two
 * payloads. Decompression reads the tag byte, splits the two tags, and
 * reconstructs each word via its tag. Well-suited to integer workloads with
 * many small or repeated values.
 */
