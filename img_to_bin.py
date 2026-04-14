#!/usr/bin/env python3
# img_to_bin.py — converts any image to a raw 64-bit word binary
# for use as the image/video workload in the compression benchmark.
#
# Usage:
#   python3 img_to_bin.py <input_image> [output.bin]
#
# The image is loaded, converted to raw RGB bytes, then packed
# into 64-bit little-endian words (8 bytes per word = ~2.67 pixels).
# This matches exactly what gen_image() produces synthetically.


import sys
import struct
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    print("ERROR: Pillow not installed. Run: pip install Pillow")
    sys.exit(1)

def convert(input_path: str, output_path: str) -> None:
    img = Image.open(input_path).convert("RGB")
    w, h = img.size
    raw = img.tobytes()

    remainder = len(raw) % 8
    if remainder:
        raw += b'\x00' * (8 - remainder)

    n_words = len(raw) // 8
    with open(output_path, 'wb') as f:
        f.write(raw)

    print(f"Image:   {input_path}  ({w}x{h}, {w*h} pixels)")
    print(f"Output:  {output_path}")
    print(f"Size:    {len(raw)} bytes  ({n_words} 64-bit words)")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 img_to_bin.py <image> [output.bin]")
        sys.exit(1)
    inp = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else Path(inp).stem + ".bin"
    convert(inp, out)

