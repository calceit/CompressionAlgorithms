#pragma once
#include "types.h"

namespace snappy_s {
    Bytes compress  (const Words& in);
    Words decompress(const Bytes& in);
}

/*
 * Declares the Snappy-style compress and decompress functions.
 * See algo_snappy.cpp for full implementation details.
 */