#!/usr/bin/env python3

import sys
import struct
import os

def convert_image(input_path: str, output_path: str) -> None:
    try:
        from PIL import Image
    except ImportError:
        print("ERROR: pip install Pillow"); sys.exit(1)

    img = Image.open(input_path).convert("RGB")
    w, h = img.size
    raw = img.tobytes()

    if len(raw) % 8:
        raw += b'\x00' * (8 - len(raw) % 8)

    with open(output_path, 'wb') as f:
        f.write(raw)

    print(f"Image:   {input_path}  ({w}×{h} = {w*h} pixels)")
    print(f"Output:  {output_path}  ({len(raw)} bytes, {len(raw)//8} words)")

def convert_sensor(input_path: str, output_path: str) -> None:
    import csv

    SENSOR_COLS = [2, 3, 6, 7, 8, 9, 10, 11, 12, 13]

    columns = {col: [] for col in SENSOR_COLS}
    with open(input_path, 'r', encoding='utf-8-sig') as f:
        reader = csv.reader(f, delimiter=';')
        next(reader)
        for row in reader:
            if len(row) < 15:
                continue
            for col in SENSOR_COLS:
                try:
                    v = float(row[col].replace(',', '.').strip())
                    if v != -200.0:
                        columns[col].append(v)
                except (ValueError, IndexError):
                    continue

    values = []
    for col in SENSOR_COLS:
        values.extend(columns[col])

    if not values:
        print("ERROR: no values parsed — check file path and format")
        sys.exit(1)

    words = []
    for v in values:
        word = int(round(v * 100)) & 0xFFFFFFFF
        words.append(word)

    raw = struct.pack(f'<{len(words)}Q', *words)
    with open(output_path, 'wb') as f:
        f.write(raw)

    total_readings = sum(len(columns[c]) for c in SENSOR_COLS)
    print(f"Sensor:  {input_path}")
    print(f"         {len(SENSOR_COLS)} columns × ~{len(columns[2])} rows = {total_readings} readings")
    print(f"         stored as fixed-point int32 ×100, one per uint64 word")
    print(f"Output:  {output_path}  ({len(raw)} bytes)")

def convert_tensor(input_path: str, output_path: str) -> None:
    try:
        import torch
        state = torch.load(input_path, map_location='cpu', weights_only=True)
        import numpy as np
        arrays = []
        if isinstance(state, dict):
            for k, v in state.items():
                if hasattr(v, 'numpy'):
                    arr = v.float().numpy().flatten()
                    arrays.append(arr)
        if arrays:
            flat = np.concatenate(arrays).astype(np.float32)
            raw = flat.tobytes()
            if len(raw) % 8:
                raw += b'\x00' * (8 - len(raw) % 8)
            with open(output_path, 'wb') as f:
                f.write(raw)
            print(f"Tensor:  {input_path}  ({len(flat)} float32 values)")
            print(f"Output:  {output_path}  ({len(raw)} bytes, {len(raw)//8} words)")
            return
    except Exception:
        pass

    with open(input_path, 'rb') as f:
        raw = f.read()
    if len(raw) % 8:
        raw += b'\x00' * (8 - len(raw) % 8)
    with open(output_path, 'wb') as f:
        f.write(raw)
    print(f"Tensor:  {input_path}  (raw binary, {len(raw)} bytes)")
    print(f"Output:  {output_path}  ({len(raw)//8} words)")

USAGE = """
Usage:
  python3 dataset_to_bin.py image   <input.png/.jpg>        <output.bin>
  python3 dataset_to_bin.py sensor  <AirQualityUCI.csv>     <output.bin>
  python3 dataset_to_bin.py tensor  <pytorch_model.bin>     <output.bin>

Downloads:
  Image:  https://r0k.us/graphics/kodak/kodak/kodim23.png
  Sensor: https://archive.ics.uci.edu/ml/machine-learning-databases/00360/AirQualityUCI.zip
  Tensor: huggingface_hub.hf_hub_download('distilbert-base-uncased', 'pytorch_model.bin')
"""

if __name__ == "__main__":
    if len(sys.argv) != 4:
        print(USAGE); sys.exit(1)

    mode, inp, out = sys.argv[1], sys.argv[2], sys.argv[3]

    if mode == "image":
        convert_image(inp, out)
    elif mode == "sensor":
        convert_sensor(inp, out)
    elif mode == "tensor":
        convert_tensor(inp, out)
    else:
        print(f"Unknown mode: {mode}"); print(USAGE); sys.exit(1)

"""
Converts three real-world datasets to raw little-endian uint64 .bin files for
the compression benchmark. The 'image' mode loads a PNG/JPEG via Pillow and
writes raw RGB bytes padded to 8-byte alignment. The 'sensor' mode reads the
UCI Air Quality CSV (semicolon-delimited, European decimal commas), extracts
ten sensor columns, stores each column as a contiguous block of fixed-point
int32×100 values (one per uint64 word) to preserve intra-column temporal
locality. The 'tensor' mode loads a PyTorch checkpoint and concatenates all
float32 weight tensors, falling back to raw binary copy if PyTorch is
unavailable. All outputs match the format expected by the benchmark harness.
"""
