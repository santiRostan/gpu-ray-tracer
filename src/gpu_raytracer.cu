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

__device__ constexpr float kPi = 3.14159265358979323846f;

__device__ inline color fetch_environment_texel(const DeviceEnvironmentMap& env_map, int x, int y) {
    if (env_map.width <= 0 || env_map.height <= 0) {
        return color(0, 0, 0);
    }

    x %= env_map.width;
    if (x < 0) {
        x += env_map.width;
    }

    y = y < 0 ? 0 : (y >= env_map.height ? env_map.height - 1 : y);

    const int index = (y * env_map.width + x) * 3;
    return color(env_map.pixels[index], env_map.pixels[index + 1], env_map.pixels[index + 2]);
}

__device__ inline float clamp_unit_interval(float value) {
    return fmaxf(0.0f, fminf(1.0f, value));
}

__device__ inline int environment_texel_index_from_uv(const DeviceEnvironmentMap& env_map, float u, float v) {
    const float wrapped_u = u - floorf(u);
    const float clamped_v = clamp_unit_interval(v);
    int x = static_cast<int>(wrapped_u * env_map.width);
    int y = static_cast<int>(clamped_v * env_map.height);
    if (x >= env_map.width) {
        x = env_map.width - 1;
    }
    if (y >= env_map.height) {
        y = env_map.height - 1;
    }
    return y * env_map.width + x;
}

__device__ color sample_environment(const DeviceEnvironmentMap& env_map, const vec3& direction) {
    if (!env_map.enabled || env_map.pixels == nullptr || env_map.width <= 0 || env_map.height <= 0) {
        return color(0, 0, 0);
    }

    const vec3 unit_direction = unit_vector(direction);
    float u = atan2f(unit_direction.z(), unit_direction.x()) / (2.0f * kPi) + 0.5f;
    float v = acosf(fmaxf(-1.0f, fminf(1.0f, unit_direction.y()))) / kPi;

    u += env_map.rotation_radians / (2.0f * kPi);
    u = u - floorf(u);
    v = clamp_unit_interval(v);

    // Bilinear filtering avoids visible texel blocks in glossy reflections and refractions.
    const float x = u * env_map.width - 0.5f;
    const float y = v * env_map.height - 0.5f;

    const int x0 = static_cast<int>(floorf(x));
    const int y0 = static_cast<int>(floorf(y));
    const int x1 = x0 + 1;
    const int y1 = y0 + 1;

    const float tx = x - floorf(x);
    const float ty = y - floorf(y);

    const color c00 = fetch_environment_texel(env_map, x0, y0);
    const color c10 = fetch_environment_texel(env_map, x1, y0);
    const color c01 = fetch_environment_texel(env_map, x0, y1);
    const color c11 = fetch_environment_texel(env_map, x1, y1);

    const color top = c00 * (1.0f - tx) + c10 * tx;
    const color bottom = c01 * (1.0f - tx) + c11 * tx;
    return (top * (1.0f - ty) + bottom * ty) * env_map.intensity;
}

__device__ int sample_environment_texel(const DeviceEnvironmentMap& env_map, curandState* state) {
    if (env_map.sampling_cdf == nullptr || env_map.distribution_size <= 0) {
        return -1;
    }

    const float target = fminf(random_float(0.0f, 1.0f, state), 0.99999994f);
    int low = 0;
    int high = env_map.distribution_size;
    while (low + 1 < high) {
        const int mid = low + (high - low) / 2;
        if (env_map.sampling_cdf[mid] <= target) {
            low = mid;
        } else {
            high = mid;
        }
    }
    return low;
}

__device__ vec3 environment_direction_from_uv(const DeviceEnvironmentMap& env_map, float u, float v) {
    const float phi = 2.0f * kPi * (u - env_map.rotation_radians / (2.0f * kPi) - 0.5f);
    const float theta = kPi * clamp_unit_interval(v);
    const float sin_theta = sinf(theta);
    return vec3(cosf(phi) * sin_theta, cosf(theta), sinf(phi) * sin_theta);
}

__device__ vec3 sample_environment_direction(const DeviceEnvironmentMap& env_map, curandState* state, float& pdf) {
    pdf = 0.0f;
    const int texel_index = sample_environment_texel(env_map, state);
    if (texel_index < 0) {
        return vec3(0.0f, 1.0f, 0.0f);
    }

    const int texel_y = texel_index / env_map.width;
    const int texel_x = texel_index - texel_y * env_map.width;
    const float u = (static_cast<float>(texel_x) + random_float(0.0f, 1.0f, state)) / static_cast<float>(env_map.width);
    const float v = (static_cast<float>(texel_y) + random_float(0.0f, 1.0f, state)) / static_cast<float>(env_map.height);
    pdf = env_map.pdf_omega[texel_index];
    return environment_direction_from_uv(env_map, u, v);
}

__device__ color estimate_environment_direct(
    const hit_record& rec,
    bvh_node* d_nodes,
    hittable* d_objects,
    curandState* state,
    const DeviceEnvironmentMap& env_map
) {
    if (!env_map.enabled || env_map.pdf_omega == nullptr || rec.mat_ptr->type != LAMBERTIAN) {
        return color(0, 0, 0);
    }

    float env_pdf = 0.0f;
    const vec3 light_direction = sample_environment_direction(env_map, state, env_pdf);
    if (env_pdf <= 0.0f) {
        return color(0, 0, 0);
    }

    const vec3 unit_light_direction = unit_vector(light_direction);
    const float cosine = dot(rec.normal, unit_light_direction);
    if (cosine <= 0.0f) {
        return color(0, 0, 0);
    }

    hit_record shadow_rec;
    const ray shadow_ray(rec.p, unit_light_direction);
    if (bvh_hit(d_nodes, d_objects, shadow_ray, 0.001f, FLT_MAX, shadow_rec)) {
        return color(0, 0, 0);
    }

    const color incident = sample_environment(env_map, unit_light_direction);
    const color albedo = ((const lambertian*)rec.mat_ptr)->albedo;
    return albedo * incident * (cosine / (kPi * env_pdf));
}

__device__ color ray_color(const ray& r, bvh_node* d_nodes, hittable* d_objects, int depth, curandState* state,
                           const DeviceEnvironmentMap& env_map) {
    ray cur_ray = r;
    auto cur_attenuation = color(1.0, 1.0, 1.0);
    color radiance(0.0f, 0.0f, 0.0f);
    bool last_bounce_specular = true;
    
    for (int i = 0; i < depth; ++i) {
        // Early termination: if attenuation becomes too small, stop tracing
        float max_attenuation = fmaxf(fmaxf(cur_attenuation.x(), cur_attenuation.y()), cur_attenuation.z());
        if (max_attenuation < 0.005f) {
            break;
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
            if (last_bounce_specular || !env_map.enabled) {
                radiance += cur_attenuation * sky_color;
            }
            break;
        }

        if (rec.mat_ptr->type == LAMBERTIAN && env_map.enabled) {
            radiance += cur_attenuation * estimate_environment_direct(rec, d_nodes, d_objects, state, env_map);
        }

        ray scattered;
        color attenuation;

        // Scatter ray
        if (scatter_device(rec.mat_ptr, cur_ray, rec, attenuation, scattered, state)) {
            cur_attenuation = cur_attenuation * attenuation;
            cur_ray = scattered;
            last_bounce_specular = rec.mat_ptr->type != LAMBERTIAN;
        } else {
            if (rec.mat_ptr->type == EMISSIVE) {
                radiance += cur_attenuation * attenuation;
            }
            break;
        }
    }

    return radiance;
}

__global__ void render_kernel(color* image, int width, int height, int samples_per_pixel, int max_depth,
                             bvh_node* d_nodes, hittable* d_objects, camera cam, DeviceEnvironmentMap env_map) {
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
        color new_color = ray_color(r, d_nodes, d_objects, max_depth, &state, env_map);
        pixel_color += new_color;
    }

    const auto scale = 1.0f / static_cast<float>(samples_per_pixel);

    image[j * width + i] = pixel_color * scale;
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
