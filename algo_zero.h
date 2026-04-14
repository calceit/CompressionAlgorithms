#pragma once
#include "types.h"

namespace zero {
    Bytes compress  (const Words& in);
    Words decompress(const Bytes& in);
}

/*
 * Declares the Zero-Value Compression compress and decompress functions.
 * See algo_zero.cpp for full implementation details.
 */