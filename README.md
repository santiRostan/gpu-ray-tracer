# GPU Ray Tracer with CUDA

<p align="center">
  <img src="renders/cornell_box_hd.png" alt="Cornell Box Render" width="600"/>
</p>
<p align="center">Render of a Cornell Box scene produced by this ray tracer.</p>

This is a simple, compact and learning-oriented GPU ray tracer implemented in C++ and CUDA. It renders scenes defined in XML using Monte Carlo path tracing and writes PNG images. Examples include a Cornell Box, simple spheres, and a BVH stress test.

Quick highlights
- Path tracing on the GPU (CUDA kernels)
- Simple material types: diffuse, metal, dielectric (glass), emissive
- XML scene files in the `scenes/` folder for easy editing
- BVH acceleration and PNG output via stb_image_write

Prerequisites
- NVIDIA GPU and CUDA toolkit (roughly CUDA 10+)
- CMake and a C++17-capable compiler

Quickstart (Windows PowerShell)
```powershell
\build.bat
.\build\gpu_raytracer.exe
```

Quickstart (Linux/macOS)
```bash
./build.sh
./bin/gpu_raytracer
```

Usage
- Pick or edit an XML scene in the `scenes/` directory (files like `cornell_box.xml`, `minimal.xml`).
- Run the built executable and choose a scene when prompted.
- The renderer writes PNG images to the `renders/` folder (or the program's output path).

What to expect in the repo
- `include/` — headers for vectors, geometry, materials, and the XML scene loader
- `src/` — the C++ and CUDA source files (CPU/BVH/renderer and CUDA kernels)
- `scenes/` — example scene XML files
- `renders/` — example outputs

If you want a quick edit to a scene: open a file under `scenes/`, tweak camera, materials or objects, and run the program again.

## Features

### Current
- **Path Tracing:** Physically-based rendering using Monte Carlo path tracing.
- **Material System:** Supports Lambertian (diffuse), Metal, Dielectric (glass), Emissive, and pure Fresnel materials.
- **Camera System:** Configurable camera with depth of field, aperture, and focus distance.
- **GPU Acceleration:** CUDA kernels for massively parallel ray tracing.
- **PNG Output:** High-quality image output using stb_image_write.
- **XML Scene Loading:** All scenes are defined in XML for easy editing and extension.
- **BVH Acceleration Structure:** Hierarchical bounding volume structure for O(log n) ray-object intersection testing.

### Planned / Future
- **Astigmatism Simulation:** Planned feature to simulate optical astigmatism, allowing for realistic rendering of vision defects.
- **Advanced Materials:** Subsurface scattering, anisotropic materials, and more.

## Third-Party Libraries
This project uses the following open-source libraries:

- [TinyXML2](https://github.com/leethomason/tinyxml2) — XML parsing (zlib license)
- [stb_image_write](https://github.com/nothings/stb) — PNG image output (public domain or MIT license)
- [stb_image](https://github.com/nothings/stb) — PNG image loading (public domain or MIT license)

