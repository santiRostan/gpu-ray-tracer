# GPU Ray Tracer with CUDA

<p align="center">
  <img src="renders/cornell_box_hd.png" alt="Cornell Box Render" width="600"/>
</p>
<p align="center">Render of a Cornell Box scene produced by this ray tracer.</p>

This is a simple, compact and learning-oriented GPU ray tracer implemented in C++ and CUDA. It renders scenes defined in XML using Monte Carlo path tracing and writes PNG images. This branch also adds environment-map lighting and a TOML-based scene authoring workflow for quickly iterating on showcase scenes.

Quick highlights
- Path tracing on the GPU (CUDA kernels)
- Environment-map lighting with HDR/LDR texture loading
- Simple material types: diffuse, metal, dielectric (glass), emissive
- XML scene files in the `scenes/` folder for direct editing
- TOML scene specs plus lint/build/render helpers in `tools/`
- CLI support for `--scene` and `--list-scenes`
- BVH acceleration and PNG output via stb_image_write

Prerequisites
- NVIDIA GPU and CUDA toolkit (roughly CUDA 10+)
- CMake and a C++17-capable compiler

Quickstart (Windows PowerShell)
```powershell
.\build.bat
.\build\bin\Release\gpu_raytracer.exe --list-scenes
```

Quickstart (Linux/macOS)
```bash
./build.sh
./build/bin/gpu_raytracer --list-scenes
```

Usage
- Pick or edit an XML scene in the `scenes/` directory (files like `cornell_box.xml`, `minimal.xml`).
- Run the built executable and choose a scene when prompted, or pass `--scene <path-to-xml>` to render a specific file directly.
- Use `--list-scenes` to print the currently discoverable scene XML files.
- The renderer writes PNG images next to the source scene XML by default.
- For easier authoring, you can build XML from TOML specs in `scene_specs/` with the helper scripts in `tools/`.

What to expect in the repo
- `include/` - headers for vectors, geometry, materials, image loading, and the XML scene loader
- `src/` - the C++ and CUDA source files (CPU/BVH/renderer and CUDA kernels)
- `scenes/` - example scene XML files, generated XML from TOML specs, and environment assets
- `scene_specs/` - higher-level TOML scene specs and templates
- `tools/` - scene builder, linter, render helper, and HDR generator scripts

If you want a quick edit to a scene: open a file under `scenes/`, tweak camera, materials or objects, and run the program again.

## Scene Authoring Workflow

Writing raw XML is still supported, but new scenes are much easier to iterate on through the TOML-based builder workflow:

```powershell
python tools/scene_lint.py scene_specs/studio_pedestals.toml
python tools/scene_builder.py scene_specs/studio_pedestals.toml
python tools/scene_builder.py scene_specs/studio_pedestals.toml --draft
python tools/scene_render.py scene_specs/studio_pedestals.toml --draft
python tools/scene_render.py scene_specs/studio_pedestals.toml
```

That workflow lets you use higher-level placements such as:
- `placement = "on_floor"` with `at = [x, z]`
- `placement = "on_top_of"` with `target = "some_item_id"`

It also supports explicit room surfaces through generic quads:
- `kind = "floor_quad"` for the simple horizontal-floor helper
- `kind = "quad"` with `corner`, `u_edge`, and `v_edge` for walls, ceilings, windows, and openings

It also supports camera auto-framing so you do not have to hand-tune every camera position:
- `frame = "all"` to frame the whole arrangement
- `frame_target = "item_id"` to frame a specific hero object
- `view_direction`, `target_offset`, and `distance_scale` to steer composition

The linter catches common scene mistakes before you render:
- overlapping solids
- floating absolute placements
- objects clipping below the floor plane
- camera look-at points that drift away from the main subject

The builder writes standard XML scenes into `scenes/`, so the renderer itself does not need to change.
The render helper writes XML and then launches the renderer directly with `--scene`, so iteration no longer depends on the interactive menu.

Current examples:
- Spec: `scene_specs/studio_pedestals.toml`
- Generated scene: `scenes/studio_pedestals.xml`
- Draft scene: `scenes/studio_pedestals_draft.xml`
- Quad room example: `scene_specs/quad_room_showcase.toml`
- Warm interior showcase: `scene_specs/cozy_vignette_warm.toml`
- Synthetic HDR generator: `tools/generate_studio_environment.py`
- Templates: `scene_specs/templates/`

## Features

### Current
- **Path Tracing:** Physically-based rendering using Monte Carlo path tracing.
- **Environment Lighting:** Lat-long environment maps with HDR/LDR loading, bilinear sampling, rotation controls, and direct-environment importance sampling for diffuse hits.
- **Material System:** Supports Lambertian (diffuse), Metal, Dielectric (glass), Emissive, and pure Fresnel materials.
- **Camera System:** Configurable camera with depth of field, aperture, and focus distance.
- **GPU Acceleration:** CUDA kernels for massively parallel ray tracing.
- **PNG Output:** High-quality image output using stb_image_write.
- **XML + TOML Scene Loading:** All scenes are rendered from XML, with optional TOML helpers for higher-level authoring and linting.
- **BVH Acceleration Structure:** Hierarchical bounding volume structure for O(log n) ray-object intersection testing.

### Planned / Future
- **Astigmatism Simulation:** Planned feature to simulate optical astigmatism, allowing for realistic rendering of vision defects.
- **Advanced Materials:** Subsurface scattering, anisotropic materials, and more.

## Third-Party Libraries
This project uses the following open-source libraries:

- [TinyXML2](https://github.com/leethomason/tinyxml2) - XML parsing (zlib license)
- [stb_image_write](https://github.com/nothings/stb) - PNG image output (public domain or MIT license)
- [stb_image](https://github.com/nothings/stb) - PNG image loading (public domain or MIT license)
