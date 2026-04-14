#pragma once
#include "types.h"

namespace delta {
    Bytes compress  (const Words& in);
    Words decompress(const Bytes& in);
}

/*
 * Declares the Delta Encoding compress and decompress functions.
 * See algo_delta.cpp for full implementation details.
 */