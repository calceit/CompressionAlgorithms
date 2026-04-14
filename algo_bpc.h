#pragma once
#include "types.h"

namespace bpc {
    static constexpr size_t BLOCK_WORDS = 64;

    Bytes compress  (const Words& in);
    Words decompress(const Bytes& in);
}

/*
 * Declares the BPC compress and decompress functions and the BLOCK_WORDS
 * constant (64 words, forming a square 64×64 bit matrix for transposition).
 * See algo_bpc.cpp for full implementation details.
 */