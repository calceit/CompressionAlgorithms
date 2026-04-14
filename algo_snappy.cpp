#include "algo_snappy.h"
#include <cstring>
#include <vector>
#include <algorithm>

namespace snappy_s {

static constexpr int HASH_BITS  = 12;
static constexpr int HASH_SIZE  = 1 << HASH_BITS;
static constexpr int MIN_MATCH  = 4;
static constexpr int MAX_MATCH  = 64;
static constexpr int MAX_OFFSET = 65535;

static uint32_t hash4(const uint8_t* p) {
    uint32_t v;
    std::memcpy(&v, p, 4);
    return (v * 0x1E35A7BD) >> (32 - HASH_BITS);
}

static void emit_literal(Bytes& out, const uint8_t* src,
                          size_t start, size_t len) {
    if (len == 0) return;

    const size_t MAX_LIT = 65536;
    while (len > 0) {
        size_t chunk = std::min(len, MAX_LIT);
        size_t n = chunk - 1;

        if (n < 60) {
            out.push_back(static_cast<uint8_t>(n << 2 | 0x00));
        } else if (n < 256) {
            out.push_back(60 << 2 | 0x00);
            out.push_back(static_cast<uint8_t>(n));
        } else {
            out.push_back(61 << 2 | 0x00);
            out.push_back(static_cast<uint8_t>(n & 0xFF));
            out.push_back(static_cast<uint8_t>(n >> 8));
        }
        out.insert(out.end(), src + start, src + start + chunk);
        start += chunk;
        len   -= chunk;
    }
}

static void emit_copy(Bytes& out, size_t offset, size_t match_len) {
    while (match_len > 0) {
        size_t chunk = std::min(match_len, (size_t)MAX_MATCH);

        uint8_t tag = static_cast<uint8_t>(((chunk - 1) << 2) | 0x02);
        out.push_back(tag);
        out.push_back(static_cast<uint8_t>(offset & 0xFF));
        out.push_back(static_cast<uint8_t>((offset >> 8) & 0xFF));

        match_len -= chunk;
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

    const size_t limit = slen >= 4 ? slen - 4 : 0;

    while (ip < limit) {
        uint32_t h         = hash4(src + ip);
        int      match_pos = table[h];
        table[h]           = static_cast<int>(ip);

        if (match_pos >= 0 &&
            ip - match_pos <= MAX_OFFSET &&
            std::memcmp(src + ip, src + match_pos, MIN_MATCH) == 0) {

            size_t mlen = MIN_MATCH;
            while (ip + mlen < slen &&
                   src[ip + mlen] == src[match_pos + mlen] &&
                   mlen < MAX_MATCH) {
                ++mlen;
            }

            emit_literal(out, src, anchor, ip - anchor);

            emit_copy(out, ip - match_pos, mlen);

            ip    += mlen;
            anchor = ip;
            continue;
        }
        ++ip;
    }

    emit_literal(out, src, anchor, slen - anchor);

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
    const size_t   src_len = in.size();

    while (ip < src_len && op < orig_bytes) {
        if (ip >= src_len) break;
        uint8_t tag = src[ip++];
        uint8_t type = tag & 0x03;

        if (type == 0x00) {
            size_t len;
            uint8_t n = tag >> 2;
            if (n < 60) {
                len = n + 1;
            } else if (n == 60) {
                if (ip >= src_len) break;
                len = src[ip++] + 1;
            } else if (n == 61) {
                if (ip + 1 >= src_len) break;
                len = src[ip] | (src[ip+1] << 8);
                ip += 2; len += 1;
            } else {
                break;
            }
            len = std::min(len, orig_bytes - op);
            if (ip + len > src_len) len = src_len - ip;
            std::memcpy(dst + op, src + ip, len);
            op += len; ip += len;

        } else if (type == 0x01) {
            if (ip >= src_len) break;
            size_t mlen   = ((tag >> 2) & 0x07) + 4;
            size_t offset = ((tag & 0xE0) << 3) | src[ip++];
            if (offset == 0 || offset > op) break;
            size_t start  = op - offset;
            mlen = std::min(mlen, orig_bytes - op);
            for (size_t i = 0; i < mlen; ++i)
                dst[op++] = dst[start + i];

        } else if (type == 0x02) {
            if (ip + 1 >= src_len) break;
            size_t mlen   = (tag >> 2) + 1;
            size_t offset = src[ip] | (src[ip+1] << 8); ip += 2;
            if (offset == 0 || offset > op) break;
            size_t start  = op - offset;
            mlen = std::min(mlen, orig_bytes - op);
            for (size_t i = 0; i < mlen; ++i)
                dst[op++] = dst[start + i];

        } else {
            if (ip + 3 >= src_len) break;
            size_t mlen   = (tag >> 2) + 1;
            size_t offset = src[ip] | (src[ip+1]<<8) |
                            ((size_t)src[ip+2]<<16) | ((size_t)src[ip+3]<<24);
            ip += 4;
            if (offset == 0 || offset > op) break;
            size_t start  = op - offset;
            mlen = std::min(mlen, orig_bytes - op);
            for (size_t i = 0; i < mlen; ++i)
                dst[op++] = dst[start + i];
        }
    }

    return out_words;
}

}

/*
 * Implements a Snappy-style block compressor operating on the raw byte
 * representation of the input word array. Uses a 4K-entry hash table to find
 * back-references. Literals are emitted with a tag byte encoding the length
 * (with 1- or 2-byte extensions for longer runs). Copies always use the 2-byte
 * offset format for correctness, capped at MAX_MATCH (64) bytes per copy
 * element. A 4-byte original-size header is prepended. Decompression handles
 * all three Snappy copy types (1-, 2-, and 4-byte offsets). Effective on data
 * with repeated byte patterns; faster to compress than LZ4 due to the smaller
 * hash table and simpler match extension.
 */
