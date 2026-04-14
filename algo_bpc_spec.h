#pragma once
#include "types.h"

namespace bpc_spec {
    static constexpr size_t BLOCK_WORDS = 64;

    Bytes compress  (const Words& in);
    Words decompress(const Bytes& in);
}

/*
 * Declares the spec-compliant BPC compress and decompress functions using
 * the full DBX (Delta + BitPlane + XOR) transformation from Kim et al. 2016.
 * BLOCK_WORDS = 64 to form a square 64×64 bit matrix for the BitPlane step.
 * See algo_bpc_spec.cpp for the full implementation.
 */
