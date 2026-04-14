#pragma once
#include "types.h"

namespace bdi {
    static constexpr size_t BLOCK_SIZE = 8;

    Bytes compress  (const Words& in);
    Words decompress(const Bytes& in);
}

/*
 * Declares the BDI compress and decompress functions and the BLOCK_SIZE
 * constant (8 words, matching a 64-byte cache line). See algo_bdi.cpp
 * for full implementation details.
 */