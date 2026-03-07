#!/usr/bin/env python3
"""Generate synthetic YUV test sequences for MV-HEVC testing."""

import struct
import os
import sys

def gen_yuv_420_8bit(filename, width, height, nframes):
    """Generate 8-bit 4:2:0 YUV file with gradient pattern."""
    with open(filename, 'wb') as f:
        for frame in range(nframes):
            # Y plane
            for y in range(height):
                for x in range(width):
                    val = ((x + y + frame * 7) * 3) & 0xFF
                    f.write(struct.pack('B', val))
            # U plane (half res)
            for y in range(height // 2):
                for x in range(width // 2):
                    val = ((x * 2 + frame * 3) * 5 + 128) & 0xFF
                    f.write(struct.pack('B', val))
            # V plane (half res)
            for y in range(height // 2):
                for x in range(width // 2):
                    val = ((y * 2 + frame * 5) * 7 + 128) & 0xFF
                    f.write(struct.pack('B', val))
    print(f"  Generated {filename}: {width}x{height}, {nframes} frames, 8-bit 4:2:0")

def gen_yuv_420_10bit(filename, width, height, nframes):
    """Generate 10-bit 4:2:0 YUV file (16-bit LE samples)."""
    with open(filename, 'wb') as f:
        for frame in range(nframes):
            # Y plane
            for y in range(height):
                for x in range(width):
                    val = ((x + y + frame * 7) * 3) & 0x3FF
                    f.write(struct.pack('<H', val))
            # U plane
            for y in range(height // 2):
                for x in range(width // 2):
                    val = ((x * 2 + frame * 3) * 5 + 512) & 0x3FF
                    f.write(struct.pack('<H', val))
            # V plane
            for y in range(height // 2):
                for x in range(width // 2):
                    val = ((y * 2 + frame * 5) * 7 + 512) & 0x3FF
                    f.write(struct.pack('<H', val))
    print(f"  Generated {filename}: {width}x{height}, {nframes} frames, 10-bit 4:2:0")

def gen_yuv_444_10bit(filename, width, height, nframes):
    """Generate 10-bit 4:4:4 YUV file (16-bit LE samples)."""
    with open(filename, 'wb') as f:
        for frame in range(nframes):
            # Y plane (full res)
            for y in range(height):
                for x in range(width):
                    val = ((x + y + frame * 7) * 3) & 0x3FF
                    f.write(struct.pack('<H', val))
            # U plane (full res for 4:4:4)
            for y in range(height):
                for x in range(width):
                    val = ((x + frame * 3) * 5 + 512) & 0x3FF
                    f.write(struct.pack('<H', val))
            # V plane (full res for 4:4:4)
            for y in range(height):
                for x in range(width):
                    val = ((y + frame * 5) * 7 + 512) & 0x3FF
                    f.write(struct.pack('<H', val))
    print(f"  Generated {filename}: {width}x{height}, {nframes} frames, 10-bit 4:4:4")

def gen_yuv_444_12bit(filename, width, height, nframes):
    """Generate 12-bit 4:4:4 YUV file (16-bit LE samples)."""
    with open(filename, 'wb') as f:
        for frame in range(nframes):
            # Y plane (full res)
            for y in range(height):
                for x in range(width):
                    val = ((x + y + frame * 7) * 13) & 0xFFF
                    f.write(struct.pack('<H', val))
            # U plane (full res for 4:4:4)
            for y in range(height):
                for x in range(width):
                    val = ((x + frame * 3) * 17 + 2048) & 0xFFF
                    f.write(struct.pack('<H', val))
            # V plane (full res for 4:4:4)
            for y in range(height):
                for x in range(width):
                    val = ((y + frame * 5) * 19 + 2048) & 0xFFF
                    f.write(struct.pack('<H', val))
    print(f"  Generated {filename}: {width}x{height}, {nframes} frames, 12-bit 4:4:4")

if __name__ == '__main__':
    testdir = os.path.dirname(os.path.abspath(__file__))
    W, H, N = 128, 128, 9

    print("Generating synthetic YUV test sequences...")

    # Test 1: Standard 2-view 4:2:0 8-bit
    gen_yuv_420_8bit(os.path.join(testdir, 'test_420_8bit_v0.yuv'), W, H, N)
    gen_yuv_420_8bit(os.path.join(testdir, 'test_420_8bit_v1.yuv'), W, H, N)

    # Test 2: Mixed bitdepth (4:2:0 8-bit + 4:2:0 10-bit)
    # view0 reuses test_420_8bit_v0.yuv
    gen_yuv_420_10bit(os.path.join(testdir, 'test_420_10bit_v1.yuv'), W, H, N)

    # Test 3: Mixed chroma (4:2:0 8-bit + 4:4:4 10-bit)
    # view0 reuses test_420_8bit_v0.yuv
    gen_yuv_444_10bit(os.path.join(testdir, 'test_444_10bit_v1.yuv'), W, H, N)

    # Test 4: Uniform 4:4:4 12-bit
    gen_yuv_444_12bit(os.path.join(testdir, 'test_444_12bit_v0.yuv'), W, H, N)
    gen_yuv_444_12bit(os.path.join(testdir, 'test_444_12bit_v1.yuv'), W, H, N)

    # Test 5: 3-layer mixed format (L2 = 64x64 4:4:4 10-bit, independent)
    gen_yuv_444_10bit(os.path.join(testdir, 'test_444_10bit_64x64_v2.yuv'), 64, 64, N)

    # Test 6: 3-layer simulcast (L1 = 64x64 4:2:0 8-bit, independent)
    gen_yuv_420_8bit(os.path.join(testdir, 'test_420_8bit_64x64_v1.yuv'), 64, 64, N)

    print("Done.")
