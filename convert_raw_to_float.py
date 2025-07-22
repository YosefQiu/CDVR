import numpy as np
import sys
import os

def main():
    if len(sys.argv) < 2:
        print("Usage: python convert_raw_to_float.py <path_to_raw_file>")
        sys.exit(1)

    raw_path = sys.argv[1]
    if not os.path.exists(raw_path):
        print(f"File not found: {raw_path}")
        sys.exit(1)

    # 假设数据大小为 64x64x64
    width, height, depth = 64, 64, 64
    num_voxels = width * height * depth

    # 读取 uint8 数据
    with open(raw_path, 'rb') as f:
        data = np.frombuffer(f.read(), dtype=np.uint8, count=num_voxels)

    if data.size != num_voxels:
        print(f"Warning: Expected {num_voxels} voxels, but got {data.size}")

    print(f"Data:")
    print(data)

    # 转为 float32 并归一化到 [0.0, 1.0]
    data_float = data.astype(np.float32) / 255.0

    # 保存为 .float32 文件
    out_path = os.path.splitext(raw_path)[0] + '_normalized.float32'
    data_float.tofile(out_path)

    print(f"Converted (normalized) file saved to: {out_path}")

if __name__ == "__main__":
    main()

