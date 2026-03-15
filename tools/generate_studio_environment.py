#!/usr/bin/env python3
"""Generate a synthetic lat-long HDR environment for local testing."""

from __future__ import annotations

import math
import os
import sys
from pathlib import Path


def clamp(value: float, low: float, high: float) -> float:
    return max(low, min(high, value))


def lerp(a: tuple[float, float, float], b: tuple[float, float, float], t: float) -> tuple[float, float, float]:
    return (
        a[0] + (b[0] - a[0]) * t,
        a[1] + (b[1] - a[1]) * t,
        a[2] + (b[2] - a[2]) * t,
    )


def smoothstep(edge0: float, edge1: float, x: float) -> float:
    if edge0 == edge1:
        return 0.0
    t = clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def normalize(x: float, y: float, z: float) -> tuple[float, float, float]:
    length = math.sqrt(x * x + y * y + z * z)
    return (x / length, y / length, z / length)


def add_light(
    direction: tuple[float, float, float],
    light_dir: tuple[float, float, float],
    sigma: float,
    intensity: float,
    tint: tuple[float, float, float],
) -> tuple[float, float, float]:
    dot_value = clamp(
        direction[0] * light_dir[0] + direction[1] * light_dir[1] + direction[2] * light_dir[2],
        -1.0,
        1.0,
    )
    angle = math.acos(dot_value)
    falloff = math.exp(-((angle / sigma) ** 2))
    return (tint[0] * intensity * falloff, tint[1] * intensity * falloff, tint[2] * intensity * falloff)


def float_to_rgbe(r: float, g: float, b: float) -> bytes:
    peak = max(r, g, b)
    if peak < 1e-32:
        return bytes((0, 0, 0, 0))

    mantissa, exponent = math.frexp(peak)
    scale = mantissa * 256.0 / peak
    return bytes(
        (
            clamp(int(r * scale), 0, 255),
            clamp(int(g * scale), 0, 255),
            clamp(int(b * scale), 0, 255),
            clamp(exponent + 128, 0, 255),
        )
    )


def build_environment(width: int, height: int) -> bytes:
    key_dir = normalize(0.45, 0.88, -0.15)
    fill_dir = normalize(-0.55, 0.72, 0.35)
    rim_dir = normalize(0.0, 0.98, 0.2)

    sky_zenith = (0.55, 0.72, 1.05)
    sky_horizon = (1.15, 1.0, 0.88)
    ground_near = (0.22, 0.24, 0.27)
    ground_far = (0.05, 0.06, 0.08)

    pixels = bytearray()

    for y in range(height):
        v = (y + 0.5) / height
        phi = math.pi * v
        sin_phi = math.sin(phi)
        cos_phi = math.cos(phi)

        for x in range(width):
            u = (x + 0.5) / width
            theta = 2.0 * math.pi * (u - 0.5)

            direction = (
                math.cos(theta) * sin_phi,
                cos_phi,
                math.sin(theta) * sin_phi,
            )

            if direction[1] >= 0.0:
                sky_t = smoothstep(0.0, 1.0, direction[1])
                color = list(lerp(sky_horizon, sky_zenith, sky_t))
            else:
                ground_t = smoothstep(0.0, 1.0, -direction[1])
                color = list(lerp(ground_near, ground_far, ground_t))

            azimuth_mix = 0.5 + 0.5 * math.sin(theta)
            color[0] *= 0.92 + 0.14 * (1.0 - azimuth_mix)
            color[2] *= 0.92 + 0.16 * azimuth_mix

            for contribution in (
                add_light(direction, rim_dir, 0.16, 12.0, (1.0, 0.98, 0.93)),
                add_light(direction, key_dir, 0.22, 7.0, (1.15, 1.0, 0.82)),
                add_light(direction, fill_dir, 0.28, 4.0, (0.75, 0.9, 1.2)),
            ):
                color[0] += contribution[0]
                color[1] += contribution[1]
                color[2] += contribution[2]

            pixels.extend(float_to_rgbe(color[0], color[1], color[2]))

    return bytes(pixels)


def main() -> int:
    repo_root = Path(__file__).resolve().parents[1]
    default_output = repo_root / "scenes" / "environments" / "studio_softboxes.hdr"

    output_path = Path(sys.argv[1]) if len(sys.argv) > 1 else default_output
    width = int(sys.argv[2]) if len(sys.argv) > 2 else 1024
    height = int(sys.argv[3]) if len(sys.argv) > 3 else 512

    output_path.parent.mkdir(parents=True, exist_ok=True)
    payload = build_environment(width, height)

    with output_path.open("wb") as hdr_file:
        hdr_file.write(b"#?RADIANCE\n")
        hdr_file.write(b"FORMAT=32-bit_rle_rgbe\n\n")
        hdr_file.write(f"-Y {height} +X {width}\n".encode("ascii"))
        hdr_file.write(payload)

    size_kb = os.path.getsize(output_path) / 1024.0
    print(f"Wrote {output_path} ({width}x{height}, {size_kb:.1f} KiB)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
