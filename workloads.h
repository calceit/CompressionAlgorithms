#pragma once
#include "types.h"
#include <cstddef>

Words gen_int8_activations(size_t n_words);

Words gen_ai_tensor(size_t n_words);

Words gen_sensor(size_t n_words);

Words gen_image(size_t n_words);

Words gen_random(size_t n_words);

Words load_image_file(const std::string& filename);

void save_workload(const Words& data, const std::string& filename);

/*
 * Declares workload generators for the edge-inference benchmark.
 * gen_int8_activations is the primary workload — packed int8 post-ReLU
 * activations with ~65% sparsity, matching quantised edge model behaviour.
 * gen_ai_tensor provides float32 activations for comparison. gen_sensor
 * covers IoT edge use cases. gen_image and gen_random provide secondary
 * and baseline workloads respectively.
 */
