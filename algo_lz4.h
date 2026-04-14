#pragma once
#include "types.h"

namespace lz4s {
    Bytes compress  (const Words& in);
    Words decompress(const Bytes& in);
}

/*
 * Declares the LZ4-style compress and decompress functions.
 * See algo_lz4.cpp for full implementation details.
 */