#include <cuda_runtime.h>
#include <curand_kernel.h>
#include <cfloat>
#include "core/vec3.h"
#include "geometry/ray.h"
#include "geometry/hittable.h"
#include "geometry/bvh.h"
#include "materials/material.h"
#include "materials/material_device.h"
#include "scene/camera.h"
#include "core/cuda_random.h"
#include "core/environment_map.h"

// Main ray tracing logic - executed by GPU

__device__ color sample_environment(const DeviceEnvironmentMap& env_map, const vec3& direction) {
    if (!env_map.enabled || env_map.pixels == nullptr || env_map.width <= 0 || env_map.height <= 0) {
        return color(0, 0, 0);
    }

    const vec3 unit_direction = unit_vector(direction);
    float u = atan2f(unit_direction.z(), unit_direction.x()) / (2.0f * static_cast<float>(M_PI)) + 0.5f;
    float v = acosf(fmaxf(-1.0f, fminf(1.0f, unit_direction.y()))) / static_cast<float>(M_PI);

    u += env_map.rotation_radians / (2.0f * static_cast<float>(M_PI));
    u = u - floorf(u);
    v = fmaxf(0.0f, fminf(1.0f, v));

    int x = static_cast<int>(u * env_map.width);
    int y = static_cast<int>(v * env_map.height);
    x = x < 0 ? 0 : (x >= env_map.width ? env_map.width - 1 : x);
    y = y < 0 ? 0 : (y >= env_map.height ? env_map.height - 1 : y);

    const int index = (y * env_map.width + x) * 3;
    return color(env_map.pixels[index], env_map.pixels[index + 1], env_map.pixels[index + 2]) * env_map.intensity;
}

__device__ color ray_color(const ray& r, bvh_node* d_nodes, hittable* d_objects, int depth, curandState* state,
                           const DeviceEnvironmentMap& env_map) {
    ray cur_ray = r;
    auto cur_attenuation = color(1.0, 1.0, 1.0);
    
    for (int i = 0; i < depth; ++i) {
        // Early termination: if attenuation becomes too small, stop tracing
        float max_attenuation = fmaxf(fmaxf(cur_attenuation.x(), cur_attenuation.y()), cur_attenuation.z());
        if (max_attenuation < 0.005f) {
            return {0, 0, 0};
        }
        
        hit_record rec;
        bool hit_anything = false;

        // Use BVH for efficient intersection testing
        hit_anything = bvh_hit(d_nodes, d_objects, cur_ray, 0.001, FLT_MAX, rec);

        // If no hit, return background gradient color
        if (!hit_anything){
            color sky_color;
            if (env_map.enabled) {
                sky_color = sample_environment(env_map, cur_ray.direction());
            } else {
                const vec3 unit_direction = unit_vector(cur_ray.direction());
                const auto t = 0.5f*(unit_direction.y() + 1.0f);
                sky_color = (1.0f-t)*color(1.0f, 1.0f, 1.0f) + t*color(0.5f, 0.7f, 1.0f);
            }
            return cur_attenuation * sky_color;
        }

        ray scattered;
        color attenuation;

        // Scatter ray
        if (scatter_device(rec.mat_ptr, cur_ray, rec, attenuation, scattered, state)) {
            cur_attenuation = cur_attenuation * attenuation;
            cur_ray = scattered;
        } else {
            return rec.mat_ptr->type == EMISSIVE ? cur_attenuation * attenuation : color(0, 0, 0);
        }
    }

    return {0, 0, 0};
}

__global__ void render_kernel(color* image, int width, int height, int samples_per_pixel, int max_depth,
                             bvh_node* d_nodes, hittable* d_objects, camera cam, DeviceEnvironmentMap env_map) {
    const int i = blockIdx.x * blockDim.x + threadIdx.x;
    const int j = blockIdx.y * blockDim.y + threadIdx.y;
    if (i >= width || j >= height) return;

    curandState state;
    curand_init(clock64(), i * height + j, 0, &state);
    color pixel_color(0, 0, 0);

    color min_color(1.0f, 1.0f, 1.0f);  // Initialize to maximum possible value
    color max_color(0.0f, 0.0f, 0.0f);  // Initialize to minimum possible value
    int samples_threshold = samples_per_pixel / 10;
    // TODO: Test with different variance thresholds, totally random rn.
    float variance_threshold = 4.5f;

    int actual_samples = samples_per_pixel;
    for (int s = 0; s < samples_per_pixel; ++s) {
        const auto u = (i + random_float(0.0f, 1.0f, &state)) / (width-1);
        const auto v = (j + random_float(0.0f, 1.0f, &state)) / (height-1);
        ray r = cam.get_ray(u, v, &state);
        color new_color = ray_color(r, d_nodes, d_objects, max_depth, &state, env_map);
        pixel_color += new_color;

        min_color = min_per_component(min_color, new_color);
        max_color = max_per_component(max_color, new_color);

        // Early exit if variance is low (adaptive sampling)
        if (s > samples_threshold && (max_color - min_color).length() < variance_threshold) {
            actual_samples = s + 1;  // +1 because we're using 0-based indexing
            break;
        }
    }

    const auto scale = 1.0f / static_cast<float>(actual_samples);

    // Clamp values to prevent overflow
    const auto r = fmin(scale * pixel_color.x(), 1.0f);
    const auto g = fmin(scale * pixel_color.y(), 1.0f);
    const auto b = fmin(scale * pixel_color.z(), 1.0f);

    image[j * width + i] = color(r, g, b);
}

// Wrapper function for the kernel with optimized configuration
extern "C" void launch_render_kernel(color* d_image, int width, int height, int samples_per_pixel, int max_depth,
                                    bvh_node* d_nodes, hittable* d_objects, camera cam,
                                    DeviceEnvironmentMap env_map) {
    // Optimized block size for better occupancy (32x8 = 256 threads per block)
    dim3 block_size(32, 8);
    
    // Calculate grid size to cover the entire image
    dim3 grid_size((width + block_size.x - 1) / block_size.x,
                   (height + block_size.y - 1) / block_size.y);
    
    // Launch kernel with optimized configuration
    render_kernel<<<grid_size, block_size>>>(d_image, width, height, samples_per_pixel, max_depth, 
                                            d_nodes, d_objects, cam, env_map);
    
    // Check for errors
    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
        printf("CUDA Error: %s\n", cudaGetErrorString(err));
    }
    
    // Synchronize to ensure kernel completion
    cudaDeviceSynchronize();
}
