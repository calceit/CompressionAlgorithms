// workloads.cpp — synthetic workload generator implementations
//
// Read workloads.h for a description of each generator.
// The goal of each generator is to produce data that is
// *statistically representative* of the real source — not
// to be a perfect model, but to give each algorithm a fair
// and meaningful test.

#include "workloads.h"
#include <random>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>


Words gen_int8_activations(size_t n_words) {
    std::mt19937_64 rng(55);
    std::uniform_real_distribution<float>    zero_dist(0.0f, 1.0f);
    std::uniform_int_distribution<int>       val_dist(1, 127);

    Words out(n_words);
    for (auto& w : out) {
        uint64_t word = 0;
        for (int b = 0; b < 8; ++b) {
            uint8_t val = (zero_dist(rng) < 0.65f)
                        ? 0
                        : static_cast<uint8_t>(val_dist(rng));
            word |= (static_cast<uint64_t>(val) << (b * 8));
        }
        w = word;
    }
    return out;
}


Words gen_ai_tensor(size_t n_words) {
    std::mt19937_64 rng(42);
    std::normal_distribution<float>         value_dist(0.0f, 0.1f);
    std::uniform_real_distribution<float>   zero_dist(0.0f, 1.0f);

    Words out(n_words);
    for (auto& w : out) {
        float a = value_dist(rng);
        float b = value_dist(rng);
        if (zero_dist(rng) < 0.4f) a = 0.0f;
        if (zero_dist(rng) < 0.4f) b = 0.0f;

        uint32_t ra, rb;
        std::memcpy(&ra, &a, 4);
        std::memcpy(&rb, &b, 4);
        w = (static_cast<uint64_t>(ra) << 32) | rb;
    }
    return out;
}


Words gen_image(size_t n_words) {
    std::mt19937_64 rng(7);
    Words out(n_words);

    uint8_t br = 128, bg = 100, bb = 80;
    for (size_t i = 0; i < n_words; ++i) {
        if (i % 256 == 0) {
            br = static_cast<uint8_t>(40 + rng() % 180);
            bg = static_cast<uint8_t>(40 + rng() % 180);
            bb = static_cast<uint8_t>(40 + rng() % 180);
        }
        uint8_t r = static_cast<uint8_t>(br + (rng() % 5) - 2);
        uint8_t g = static_cast<uint8_t>(bg + (rng() % 5) - 2);
        uint8_t b = static_cast<uint8_t>(bb + (rng() % 5) - 2);
        uint64_t word = 0;
        for (int byte = 0; byte < 8; ++byte) {
            uint8_t ch = (byte % 3 == 0) ? r : (byte % 3 == 1) ? g : b;
            word |= (static_cast<uint64_t>(ch) << (byte * 8));
        }
        out[i] = word;
    }
    return out;
}


Words gen_sensor(size_t n_words) {
    std::mt19937_64 rng(13);
    std::normal_distribution<double> noise(0.0, 0.5);

    Words out(n_words);
    double val = 101325.0;
    for (auto& w : out) {
        val += noise(rng);
        val = std::clamp(val, 90000.0, 110000.0);
        uint32_t fixed = static_cast<uint32_t>(static_cast<int32_t>(std::round(val * 100.0)));
        w = static_cast<uint64_t>(fixed);
    }
    return out;
}


Words gen_random(size_t n_words) {
    std::mt19937_64 rng(99);
    Words out(n_words);
    for (auto& w : out) w = rng();
    return out;
}


Words load_image_file(const std::string& filename) {
    std::ifstream f(filename, std::ios::binary | std::ios::ate);
    if (!f) {
        std::cerr << "WARNING: could not open '" << filename
                  << "' — falling back to synthetic image data.\n"
                  << "  To use a real image: python3 img_to_bin.py <image.png> "
                  << filename << "\n";
        return {};
    }
    size_t byte_count = f.tellg();
    f.seekg(0);
    size_t n_words = (byte_count + 7) / 8;
    Words out(n_words, 0);
    f.read(reinterpret_cast<char*>(out.data()), byte_count);
    std::cout << "Loaded: " << filename
              << " (" << byte_count << " bytes, " << n_words << " words)\n";
    return out;
}


void save_workload(const Words& data, const std::string& filename) {
    std::ofstream f(filename, std::ios::binary);
    if (!f) {
        std::cerr << "ERROR: could not open " << filename << " for writing\n";
        return;
    }
    f.write(reinterpret_cast<const char*>(data.data()),
            data.size() * sizeof(uint64_t));
    std::cout << "Saved " << data.size() * 8 << " bytes to " << filename << "\n";
}

