#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <functional>
#include <chrono>
#include <cstring>
#include <limits>

#include "types.h"
#include "workloads.h"
#include "algo_zero.h"
#include "algo_delta.h"
#include "algo_bdi.h"
#include "algo_fpc.h"
#include "algo_lz4.h"
#include "algo_bpc.h"
#include "algo_bpc_spec.h"
#include "algo_snappy.h"
#include "algo_cascade.h"

static constexpr int N_RUNS = 3;

using CompressFn   = std::function<Bytes(const Words&)>;
using DecompressFn = std::function<Words(const Bytes&)>;

struct Algorithm {
    std::string  name;
    CompressFn   compress;
    DecompressFn decompress;
};

template<typename Fn>
double timed_min(Fn&& fn) {
    double best = std::numeric_limits<double>::max();
    for (int i = 0; i < N_RUNS; ++i) {
        auto t0 = std::chrono::high_resolution_clock::now();
        fn();
        auto t1 = std::chrono::high_resolution_clock::now();
        best = std::min(best, std::chrono::duration<double>(t1 - t0).count());
    }
    return best;
}

BenchResult run_bench(const Algorithm& algo,
                      const std::string& workload_name,
                      const Words& data) {
    size_t orig_bytes = data.size() * sizeof(uint64_t);

    Bytes compressed;
    double comp_secs = timed_min([&]{ compressed = algo.compress(data); });

    Words recovered;
    double decomp_secs = timed_min([&]{ recovered = algo.decompress(compressed); });

    bool ok = (recovered == data);

    double ratio       = static_cast<double>(orig_bytes) /
                         static_cast<double>(compressed.size());
    double comp_mbps   = (orig_bytes / 1e6) / comp_secs;
    double decomp_mbps = (orig_bytes / 1e6) / decomp_secs;

    return { algo.name, workload_name,
             orig_bytes, compressed.size(),
             ratio, comp_mbps, decomp_mbps, ok };
}

void print_results(const std::vector<BenchResult>& results) {
    const int W_ALGO = 12, W_WL = 18, W_NUM = 12, W_RATIO = 10,
              W_TP   = 16, W_OK  = 6;
    int total_w = W_ALGO + W_WL + W_NUM*2 + W_RATIO + W_TP*2 + W_OK;

    auto sep = [&]{ std::cout << std::string(total_w, '-') << "\n"; };

    std::cout << "\n";
    sep();
    std::cout << std::left
              << std::setw(W_ALGO) << "Algorithm"
              << std::setw(W_WL)   << "Workload"
              << std::setw(W_NUM)  << "Orig(B)"
              << std::setw(W_NUM)  << "Comp(B)"
              << std::setw(W_RATIO)<< "Ratio"
              << std::setw(W_TP)   << "Comp(MB/s)"
              << std::setw(W_TP)   << "Decomp(MB/s)"
              << std::setw(W_OK)   << "OK?"
              << "\n";
    sep();

    std::string last_wl;
    for (const auto& r : results) {
        if (r.workload != last_wl) {
            std::cout << "\n  Workload: " << r.workload << "\n";
            last_wl = r.workload;
        }
        std::cout << std::left  << std::fixed
                  << std::setw(W_ALGO) << r.algo
                  << std::setw(W_WL)   << r.workload
                  << std::setw(W_NUM)  << r.original_bytes
                  << std::setw(W_NUM)  << r.compressed_bytes
                  << std::setw(W_RATIO)<< std::setprecision(2) << r.ratio
                  << std::setw(W_TP)   << std::setprecision(1) << r.compress_mbps
                  << std::setw(W_TP)   << std::setprecision(1) << r.decompress_mbps
                  << std::setw(W_OK)   << (r.roundtrip_ok ? "YES" : "NO!")
                  << "\n";
    }
    sep();
    std::cout << "Ratio > 1.0 = space saved.  Ratio < 1.0 = data expanded (bad).\n";
    std::cout << "Throughput = software proxy only; does not reflect hardware latency.\n\n";
}

Words load_optional(const std::string& filename, const std::string& label,
                    const Words& fallback) {
    Words data = load_image_file(filename);
    if (data.empty()) {
        std::cout << "  [" << label << "] " << filename
                  << " not found — using synthetic fallback\n";
        return fallback;
    }
    std::cout << "  [" << label << "] " << filename
              << " — " << data.size() * 8 << " bytes\n";
    return data;
}

int main() {

    std::cout << "Edge NPU DMA Compression Benchmark\n";
    std::cout << "Timing: min of " << N_RUNS << " runs per measurement\n\n";
    std::cout << "Loading datasets...\n";

    Words int8_data = load_optional(
        "ai_tensor_int8.bin", "int8 Activations",
        gen_int8_activations(65536));

    Words f32_data = load_optional(
        "ai_tensor.bin", "float32 Tensor",
        gen_ai_tensor(65536));

    Words sensor_data = load_optional(
        "sensor.bin", "Sensor Stream",
        gen_sensor(65536));

    Words image_data = load_optional(
        "image.bin", "Photo",
        gen_image(65536));

    Words random_data = gen_random(65536);
    std::cout << "  [Random] synthetic — " << random_data.size() * 8 << " bytes\n";

    struct Workload { std::string name; Words data; };
    std::vector<Workload> workloads = {
        { "int8 Activations", int8_data   },
        { "float32 Tensor",   f32_data    },
        { "Photo",            image_data  },
        { "Sensor Stream",    sensor_data },
        { "Random",           random_data },
    };

    std::vector<Algorithm> algorithms = {
        { "Cascade",    cascade::compress,      cascade::decompress  },
        { "Zero-Value", zero::compress,         zero::decompress     },
        { "BDI",        bdi::compress,          bdi::decompress      },
        { "FPC",        fpc::compress,          fpc::decompress      },
        { "Delta",      delta::compress,        delta::decompress    },
        { "BPC",        bpc::compress,          bpc::decompress      },
        { "BPC-Spec",   bpc_spec::compress,     bpc_spec::decompress },
        { "LZ4",        lz4s::compress,         lz4s::decompress     },
        { "Snappy",     snappy_s::compress,     snappy_s::decompress },
    };

    std::cout << "\nRunning benchmarks (" << workloads.size()
              << " workloads x " << algorithms.size()
              << " algorithms x " << N_RUNS << " runs)...\n";

    std::vector<BenchResult> results;
    for (const auto& wl : workloads)
        for (const auto& algo : algorithms)
            results.push_back(run_bench(algo, wl.name, wl.data));

    print_results(results);

    return 0;
}

/*
 * Edge-inference DMA compression benchmark harness. Loads int8 activation,
 * float32 tensor, and sensor datasets from .bin files if available, falling
 * back to synthetic generators otherwise — no hard exits. Runs all algorithms
 * against all workloads with N_RUNS repetitions, reporting the minimum time
 * to reduce OS scheduling noise. The Cascade algorithm (Zero+BDI) is listed
 * first as the primary hardware design candidate. Throughput numbers are
 * software proxies and do not reflect hardware decompression latency.
 */
