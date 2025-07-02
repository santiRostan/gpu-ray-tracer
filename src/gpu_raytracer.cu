#include <cuda_runtime.h>
#include <curand_kernel.h>
#include <cfloat>
#include "core/vec3.h"
#include "geometry/ray.h"
#include "geometry/sphere.h"
#include "materials/material.h"
#include "geometry/quad.h"
#include "materials/material_device.h"
#include "scene/camera.h"
#include "core/cuda_random.h"

// Main ray tracing logic - executed by GPU

__device__ color ray_color(const ray& r, quad* d_quads, int num_quads, sphere* d_spheres, int num_spheres, int depth, curandState* state) {
    ray cur_ray = r;
    auto cur_attenuation = color(1.0, 1.0, 1.0);
    
    for (int i = 0; i < depth; ++i) {
        hit_record rec;
        bool hit_anything = false;
        float closest_intersection = FLT_MAX;

        // Check intersection with quads
        for (int q = 0; q < num_quads; ++q) {
            hit_record temp_rec;
            if (d_quads[q].hit(cur_ray, 0.001, closest_intersection, temp_rec)) {
                hit_anything = true;
                closest_intersection = temp_rec.t;
                rec = temp_rec;
            }
        }

        // Check intersection with spheres
        for (int s = 0; s < num_spheres; ++s) {
            hit_record temp_rec;
            if (d_spheres[s].hit(cur_ray, 0.001, closest_intersection, temp_rec)) {
                hit_anything = true;
                closest_intersection = temp_rec.t;
                rec = temp_rec;
            }
        }

        if (!hit_anything){
            // If no hit, return background gradient color
            const vec3 unit_direction = unit_vector(cur_ray.direction());
            const auto t = 0.5f*(unit_direction.y() + 1.0f);
            const color sky_color = (1.0f-t)*color(1.0f, 1.0f, 1.0f) + t*color(0.5f, 0.7f, 1.0f);
            return cur_attenuation * sky_color;
        }

        ray scattered;
        color attenuation;

        // Use the scatter dispatcher
        if (scatter_device(rec.mat_ptr, cur_ray, rec, attenuation, scattered, state)) {
            cur_attenuation = cur_attenuation * attenuation;
            cur_ray = scattered;
        } else {
            if (rec.mat_ptr->type == EMISSIVE) {
                return cur_attenuation * attenuation;
            }
            return {0, 0, 0};
        }
    }

    return {0, 0, 0};
}

__global__ void render_kernel(color* image, int width, int height, int samples_per_pixel, 
                             quad* d_quads, int num_quads, sphere* d_spheres, int num_spheres, camera cam) {
    const int i = blockIdx.x * blockDim.x + threadIdx.x;
    const int j = blockIdx.y * blockDim.y + threadIdx.y;
    if (i >= width || j >= height) return;

    curandState state;
    curand_init(clock64(), i * height + j, 0, &state);
    color pixel_color(0, 0, 0);
    for (int s = 0; s < samples_per_pixel; ++s) {
        const auto u = (i + random_float(0.0f, 1.0f, &state)) / (width-1);
        const auto v = (j + random_float(0.0f, 1.0f, &state)) / (height-1);
        ray r = cam.get_ray(u, v, &state);
        pixel_color += ray_color(r, d_quads, num_quads, d_spheres, num_spheres, 50, &state);
    }
    const auto scale = 1.0f / samples_per_pixel;

    // Clamp values to prevent overflow
    auto r = fmin(scale * pixel_color.x(), 1.0f);
    auto g = fmin(scale * pixel_color.y(), 1.0f);
    auto b = fmin(scale * pixel_color.z(), 1.0f);
    r = sqrt(r);
    g = sqrt(g);
    b = sqrt(b);

    image[j * width + i] = color(r, g, b);
}

// Wrapper function for the kernel
extern "C" void launch_render_kernel(color* d_image, int width, int height, int samples_per_pixel, 
                                    quad* d_quads, int num_quads, sphere* d_spheres, int num_spheres, camera cam) {
    dim3 block_size(16, 16);
    dim3 grid_size((width + block_size.x - 1) / block_size.x,
                   (height + block_size.y - 1) / block_size.y);
    render_kernel<<<grid_size, block_size>>>(d_image, width, height, samples_per_pixel, d_quads, num_quads, d_spheres, num_spheres, cam);
} 