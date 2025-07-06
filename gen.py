import numpy as np
import noise
import sys

def generate_perlin_noise_3d(shape, scale=0.1, seed=0):
    """Generate 3D Perlin noise array"""
    np.random.seed(seed)
    offset = np.random.randint(0, 10000)

    x_dim, y_dim, z_dim = shape
    data = np.zeros((x_dim, y_dim, z_dim), dtype=np.float32)

    for x in range(x_dim):
        for y in range(y_dim):
            for z in range(z_dim):
                nx, ny, nz = x * scale, y * scale, z * scale
                val = noise.pnoise3(nx + offset, ny + offset, nz + offset,
                                    octaves=4, persistence=0.5, lacunarity=2.0,
                                    repeatx=1024, repeaty=1024, repeatz=1024,
                                    base=0)
                data[x, y, z] = val
    return data

def main():
    if len(sys.argv) != 5:
        print("Usage: python gen.py x y z output_path")
        return

    x, y, z = map(int, sys.argv[1:4])
    path = sys.argv[4]

    data = generate_perlin_noise_3d((x, y, z))
    data.astype(np.float32).tofile(path)
    print(f"[Done] Saved 3D Perlin noise ({x}x{y}x{z}) to {path}")

if __name__ == "__main__":
    main()

