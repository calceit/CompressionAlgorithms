#pragma once
#include "types.h"

namespace cascade {
    static constexpr size_t BLOCK_SIZE = 8;

    Bytes compress  (const Words& in);
    Words decompress(const Bytes& in);
}

/*
 * Declares the Zero+BDI cascade compress and decompress functions
 * and the BLOCK_SIZE constant (8 words = 64-byte cache line).
 * See algo_cascade.cpp for full implementation details.
 */
