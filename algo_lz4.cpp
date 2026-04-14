#include "algo_lz4.h"
#include <cstring>
#include <vector>
#include <algorithm>

namespace lz4s {

static constexpr int     HASH_BITS  = 16;
static constexpr int     HASH_SIZE  = 1 << HASH_BITS;
static constexpr int     MIN_MATCH  = 4;
static constexpr int     MAX_OFFSET = 65535;
static constexpr size_t  LAST_LITS  = 5;

static uint32_t hash4(const uint8_t* p) {
    uint32_t v;
    std::memcpy(&v, p, 4);
    return (v * 2654435761u) >> (32 - HASH_BITS);
}

static void emit_sequence(Bytes& out,
                           const uint8_t* src,
                           size_t lit_start, size_t lit_len,
                           uint16_t offset, size_t match_len) {
    size_t ll = lit_len;
    size_t ml = match_len - MIN_MATCH;

    uint8_t token = static_cast<uint8_t>(
        (std::min(ll, (size_t)15) << 4) |
         std::min(ml, (size_t)15));
    out.push_back(token);

    if (ll >= 15) {
        ll -= 15;
        while (ll >= 255) { out.push_back(255); ll -= 255; }
        out.push_back(static_cast<uint8_t>(ll));
    }

    out.insert(out.end(), src + lit_start, src + lit_start + lit_len);

    out.push_back(offset & 0xFF);
    out.push_back((offset >> 8) & 0xFF);

    if (ml >= 15) {
        ml -= 15;
        while (ml >= 255) { out.push_back(255); ml -= 255; }
        out.push_back(static_cast<uint8_t>(ml));
    }
}

Bytes compress(const Words& in) {
    const uint8_t* src  = reinterpret_cast<const uint8_t*>(in.data());
    const size_t   slen = in.size() * sizeof(uint64_t);

    Bytes out;
    out.reserve(slen);

    uint32_t orig32 = static_cast<uint32_t>(slen);
    out.insert(out.end(), reinterpret_cast<uint8_t*>(&orig32),
                          reinterpret_cast<uint8_t*>(&orig32) + 4);

    std::vector<int> table(HASH_SIZE, -1);

    size_t ip     = 0;
    size_t anchor = 0;

    const size_t limit = (slen >= LAST_LITS + MIN_MATCH)
                       ? slen - LAST_LITS - MIN_MATCH + 1
                       : 0;

    while (ip < limit) {
        uint32_t h         = hash4(src + ip);
        int      match_pos = table[h];
        table[h]           = static_cast<int>(ip);

        if (match_pos >= 0 &&
            ip - match_pos <= MAX_OFFSET &&
            std::memcmp(src + ip, src + match_pos, MIN_MATCH) == 0)
        {
            size_t mlen = MIN_MATCH;
            while (ip + mlen < slen - LAST_LITS &&
                   src[ip + mlen] == src[match_pos + mlen] &&
                   mlen < 65535) {
                ++mlen;
            }

            emit_sequence(out, src,
                          anchor, ip - anchor,
                          static_cast<uint16_t>(ip - match_pos),
                          mlen);

            ip     += mlen;
            anchor  = ip;
            continue;
        }

        ++ip;
    }

    size_t final_lits = slen - anchor;
    size_t ll = final_lits;

    uint8_t token = static_cast<uint8_t>(std::min(ll, (size_t)15) << 4);
    out.push_back(token);
    if (ll >= 15) {
        ll -= 15;
        while (ll >= 255) { out.push_back(255); ll -= 255; }
        out.push_back(static_cast<uint8_t>(ll));
    }
    out.insert(out.end(), src + anchor, src + slen);

    return out;
}

Words decompress(const Bytes& in) {
    uint32_t orig_bytes;
    std::memcpy(&orig_bytes, in.data(), 4);

    const size_t n_words = (orig_bytes + 7) / 8;
    Words out_words(n_words, 0);
    uint8_t* dst = reinterpret_cast<uint8_t*>(out_words.data());

    size_t ip = 4;
    size_t op = 0;

    const uint8_t* src = in.data();

    while (op < orig_bytes) {
        uint8_t token = src[ip++];

        size_t lit_len = (token >> 4) & 0xF;
        if (lit_len == 15) {
            uint8_t b;
            do { b = src[ip++]; lit_len += b; } while (b == 255);
        }

        std::memcpy(dst + op, src + ip, lit_len);
        op += lit_len;
        ip += lit_len;

        if (op >= orig_bytes) break;

        uint16_t offset;
        std::memcpy(&offset, src + ip, 2); ip += 2;

        size_t match_len = (token & 0xF) + MIN_MATCH;
        if ((token & 0xF) == 15) {
            uint8_t b;
            do { b = src[ip++]; match_len += b; } while (b == 255);
        }

        size_t match_start = op - offset;
        for (size_t i = 0; i < match_len; ++i)
            dst[op++] = dst[match_start + i];
    }

    return out_words;
}

}

/*
 * Implements an LZ4-style block compressor. Operates on the raw byte
 * representation of the input word array. Uses a 64K-entry hash table to find
 * back-references; each compressed sequence consists of a token byte encoding
 * literal and match lengths, optional overflow bytes, the literal run, and a
 * 2-byte little-endian match offset. A 4-byte original-size header is prepended
 * so the decompressor knows when to stop. The last 5 bytes of input are always
 * emitted as literals per the LZ4 spec. Effective on data with repeated byte
 * patterns such as image and sensor streams.
 */
