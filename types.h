#pragma once
#include <cstdint>
#include <string>
#include <vector>

using Words = std::vector<uint64_t>;
using Bytes = std::vector<uint8_t>;

struct BenchResult {
    std::string algo;
    std::string workload;
    size_t original_bytes;
    size_t compressed_bytes;
    double ratio;
    double compress_mbps;
    double decompress_mbps;
    bool roundtrip_ok;
};

/*
 * Defines the three shared types used across the benchmark: Words
 * (vector of uint64_t) as the unit of uncompressed data, Bytes
 * (vector of uint8_t) as the compressed byte stream, and BenchResult
 * which holds all metrics from a single algorithm/workload run —
 * original size, compressed size, ratio, throughput, and round-trip
 * correctness flag.
 */
