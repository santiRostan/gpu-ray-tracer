# GPU Ray Tracer Concepts

This document explains the key concepts and algorithms used in this GPU ray tracer implementation, featuring a XML scene loading system and advanced camera features.

## Ray Tracing Fundamentals

### What is Ray Tracing?
Ray tracing is a rendering technique that simulates the way light travels in the real world. It works by:
1. Casting rays from the camera through each pixel
2. Following those rays as they bounce off objects
3. Calculating the final color based on material properties and lighting

### Path Tracing
Path tracing is a specific type of ray tracing that uses Monte Carlo integration to solve the rendering equation. It:
- Traces multiple rays per pixel for anti-aliasing
- Uses random sampling to simulate global illumination
- Converges to the correct solution as sample count increases

## Monte Carlo Integration

### Why Monte Carlo?
The rendering equation is an integral that's difficult to solve analytically. Monte Carlo integration:
- Approximates integrals using random sampling
- Provides unbiased estimates
- Works well with high-dimensional problems

### Implementation
```cpp
// For each pixel, take multiple samples
for (int s = 0; s < samples_per_pixel; ++s) {
    // Generate random ray direction
    auto u = (i + random_float()) / (width-1);
    auto v = (j + random_float()) / (height-1);
    ray r = cam.get_ray(u, v);
    
    // Accumulate color
    pixel_color += ray_color(r, world, max_depth);
}

// Average the samples
auto scale = 1.0f / samples_per_pixel;
final_color = scale * pixel_color;
```

### Scene Loading Process
1. **XML Parsing**: TinyXML2 library parses scene definition
2. **Material Creation**: Materials are created and stored with unique IDs
3. **Object Creation**: Objects reference materials by ID
4. **GPU Allocation**: Scene data is allocated and copied to GPU memory
5. **Rendering**: CUDA kernel processes the scene

## Advanced Camera System

### Depth of Field Implementation
The camera system implements realistic depth of field effects:

#### Aperture and Focus
```cpp
class camera {
    point3 origin;
    point3 lower_left_corner;
    vec3 horizontal;
    vec3 vertical;
    vec3 u, v, w;
    float lens_radius;
    float focus_distance;
    
    ray get_ray(float s, float t) const {
        vec3 rd = lens_radius * random_in_unit_disk();
        vec3 offset = u * rd.x() + v * rd.y();
        
        return ray(origin + offset,
                   lower_left_corner + s*horizontal + t*vertical - origin - offset);
    }
};
```

#### Bokeh Effect
- **Lens Radius**: Controls the amount of blur (larger = more blur)
- **Focus Distance**: Objects at this distance appear sharp
- **Random Disk Sampling**: Creates realistic lens aperture simulation

## CUDA Implementation

### Kernel Structure
```cpp
__global__ void render_kernel_quads_and_spheres(color* image, int width, int height, 
                                               int samples_per_pixel, quad* quads, 
                                               int num_quads, sphere* spheres, 
                                               int num_spheres, camera cam) {
    // Each thread handles one pixel
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    int j = blockIdx.y * blockDim.y + threadIdx.y;
    
    if (i >= width || j >= height) return;
    
    // Render pixel (i, j) with both quads and spheres
    // ...
}
```

### Memory Management
- **Host Memory**: Scene objects, materials, XML parsing
- **Device Memory**: Object arrays, material pointers, image buffer
- **Memory Transfers**: Host to device for scene data, device to host for result
- **Unified Allocation**: Contiguous arrays for efficient memory access

### Random Number Generation
- Each thread has its own random state
- Uses CUDA's curand library
- Ensures reproducible results with proper seeding

## Scene Representation

### Primitive Types

#### Sphere Primitive
```cpp
struct sphere {
    point3 center;
    float radius;
    material* mat_ptr;
};
```
- **Intersection**: Quadratic equation solution
- **Normal**: Direction from center to intersection point
- **Usage**: Spherical objects, ground planes (large radius)

#### Quad Primitive
```cpp
struct quad {
    point3 Q;           // Corner point
    vec3 u, v;          // Edge vectors
    material* mat_ptr;
    vec3 normal;        // Precomputed normal
    float D;            // Plane equation constant
    float w_area;       // Reciprocal area
};
```
- **Intersection**: Ray-plane intersection with bounds checking
- **Normal**: Precomputed for efficiency
- **Usage**: Walls, floors, ceilings, area lights

#### Box Primitive
- The `<box>` element allows you to define a rectangular prism by center, size, rotation (y-axis), and material.
- The parser generates the 6 quads for the box.

### Material System
The material system uses type tags instead of virtual functions for CUDA compatibility:

```cpp
enum material_type { LAMBERTIAN, METAL, DIELECTRIC, EMISSIVE };

struct material {
    material_type type;
    // Union or additional fields for material-specific data
};

struct lambertian {
    material_type type = LAMBERTIAN;
    color albedo;
};

struct metal {
    material_type type = METAL;
    color albedo;
    float fuzz;
};

struct dielectric {
    material_type type = DIELECTRIC;
    float ir;  // Index of refraction
};

struct emissive {
    material_type type = EMISSIVE;
    color emit;
};
```

## Performance Considerations

### Parallelization
- Each pixel is processed by a separate GPU thread
- No dependencies between pixels
- Excellent for GPU parallelization

### Memory Access Patterns
- Coalesced memory access for image buffer
- Object data stored in contiguous arrays
- Efficient memory layout for multiple primitive types

### Optimization Techniques
- Early ray termination
- Unified scene system reduces kernel overhead
- Efficient material type checking with tags
- Multiple primitive support in single kernel

## XML Integration Architecture

### TinyXML2 Library
- **Lightweight**: Header-only XML parser
- **Easy Integration**: Simple CMake integration
- **Robust**: Handles malformed XML gracefully
- **Cross-platform**: Works on Windows, Linux, macOS

### Scene Controller
```cpp
class SceneController {
    bool load_scene_from_xml(const char* filename, ...);
    bool parse_camera(tinyxml2::XMLElement* camera_elem, camera& cam);
    bool parse_materials(tinyxml2::XMLElement* materials_elem, ...);
    bool parse_objects(tinyxml2::XMLElement* objects_elem, ...);
};
```

### Error Handling
- **File Validation**: Checks for file existence and readability
- **XML Structure**: Validates required elements and attributes
- **Material References**: Ensures object materials exist
- **Parameter Validation**: Checks for valid numeric values

## Pure XML Architecture Benefits

### Code Simplification
- **Single Scene Loading Function**: All scenes use `create_scene_from_xml()`
- **No Hardcoded Logic**: Scene creation entirely data-driven
- **Consistent Interface**: Same loading process for all scenes
- **Easier Maintenance**: Scene changes don't require code modifications

### Development Workflow
- **Rapid Prototyping**: New scenes created in minutes
- **Version Control**: Scene changes tracked separately from code
- **Collaboration**: Artists can modify scenes without touching code
- **Testing**: Easy to create test scenes for validation

### Extensibility
- **New Primitives**: Add new object types to XML parser
- **New Materials**: Extend material system with XML support
- **Scene Composition**: Combine multiple XML files
- **Procedural Generation**: Generate XML from scripts
