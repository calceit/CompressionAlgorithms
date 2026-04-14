#pragma once
#include "types.h"

namespace fpc {
    Bytes compress  (const Words& in);
    Words decompress(const Bytes& in);
}

/*
 * Declares the Frequent Pattern Compression compress and decompress functions.
 * See algo_fpc.cpp for full implementation details.
 */