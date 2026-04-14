#!/usr/bin/env python3
# capture_activations.py
#
# Runs a forward pass through MobileNetV2 (primary) or ResNet-18 (fallback)
# on a batch of random images, captures post-ReLU activation maps from each
# layer, and saves them as flat binary files for the compression benchmark.
#
# MobileNetV2 is used by default as it is representative of edge inference
# models (depthwise separable convolutions, designed for mobile/embedded).
#
# Produces two output files:
#   ai_tensor.bin      — float32 activations (raw IEEE 754)
#   ai_tensor_int8.bin — int8 quantised activations (packed 8 per uint64 word)
#
# Why activations and not weights:
#   - Post-ReLU activations are sparse (40-90% zero after ReLU zeroes negatives)
#   - Int8 quantised activations have *exact* zeros — no near-zero ambiguity
#   - This is the data the DMA engine transfers between SRAM banks during inference
#   - Rhu et al. [cDMA, MICRO 2017] showed this compresses well with zero-value compression
#
# Usage:
#   pip install torch torchvision
#   python3 capture_activations.py [--model resnet18]

import struct
import sys
import argparse
import numpy as np

try:
    import torch
    import torch.nn as nn
    import torchvision.models as models
except ImportError:
    print("ERROR: pip install torch torchvision")
    sys.exit(1)


def quantise_to_int8(activations: np.ndarray) -> np.ndarray:
    max_val = activations.max()
    if max_val == 0:
        return np.zeros_like(activations, dtype=np.uint8)
    scale = 127.0 / max_val
    quantised = np.clip(np.round(activations * scale), 0, 127).astype(np.uint8)
    return quantised


def pack_int8_to_uint64(int8_vals: np.ndarray) -> bytes:
    remainder = len(int8_vals) % 8
    if remainder:
        int8_vals = np.concatenate([int8_vals, np.zeros(8 - remainder, dtype=np.uint8)])
    n_words = len(int8_vals) // 8
    words = struct.pack(f'<{n_words}Q',
                        *[int.from_bytes(int8_vals[i*8:(i+1)*8].tobytes(), 'little')
                          for i in range(n_words)])
    return words


def capture_activations(model_name: str = 'mobilenet_v2') -> None:
    print(f"Loading {model_name}...")

    if model_name == 'mobilenet_v2':
        model = models.mobilenet_v2(weights=None)
    else:
        model = models.resnet18(weights=None)
    model.eval()

    activations = []

    def hook_fn(module, input, output):
        arr = output.detach().float().numpy().flatten()
        arr = np.maximum(arr, 0)
        activations.append(arr)

    hooks = []
    for name, module in model.named_modules():
        if isinstance(module, nn.ReLU) or isinstance(module, nn.ReLU6):
            hooks.append(module.register_forward_hook(hook_fn))

    print(f"Running forward pass (batch=16, 224x224)...")
    dummy_input = torch.randn(16, 3, 224, 224)
    with torch.no_grad():
        _ = model(dummy_input)

    for h in hooks:
        h.remove()

    print(f"Captured {len(activations)} activation layers")
    for i, a in enumerate(activations):
        zeros = np.sum(a == 0.0)
        print(f"  Layer {i:2d}: {len(a):>10,} values,  {zeros/len(a)*100:5.1f}% zero")

    all_acts = np.concatenate(activations).astype(np.float32)
    total_zeros = np.sum(all_acts == 0.0)
    print(f"\nTotal float32 values : {len(all_acts):,}")
    print(f"Overall sparsity     : {total_zeros/len(all_acts)*100:.1f}% zero")

    raw_f32 = all_acts.tobytes()
    if len(raw_f32) % 8:
        raw_f32 += b'\x00' * (8 - len(raw_f32) % 8)
    with open("ai_tensor.bin", 'wb') as f:
        f.write(raw_f32)
    print(f"\nSaved: ai_tensor.bin  ({len(raw_f32):,} bytes, {len(raw_f32)//8:,} uint64 words)")

    int8_acts = quantise_to_int8(all_acts)
    int8_zeros = np.sum(int8_acts == 0)
    print(f"\nInt8 quantised       : {len(int8_acts):,} values")
    print(f"Int8 sparsity        : {int8_zeros/len(int8_acts)*100:.1f}% zero")
    raw_int8 = pack_int8_to_uint64(int8_acts)
    with open("ai_tensor_int8.bin", 'wb') as f:
        f.write(raw_int8)
    print(f"Saved: ai_tensor_int8.bin  ({len(raw_int8):,} bytes, {len(raw_int8)//8:,} uint64 words)")

    print(f"\nNow run: ./bench")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('--model', default='mobilenet_v2',
                        choices=['mobilenet_v2', 'resnet18'],
                        help='Model architecture (default: mobilenet_v2)')
    args = parser.parse_args()
    capture_activations(args.model)

