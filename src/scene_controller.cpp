#include <iostream>
#include <cuda_runtime.h>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>
#include "geometry/hittable.h"
#include "geometry/bvh.h"
#include "materials/material.h"
#include "scene/camera.h"
#include "scene/xml_scene_parser.h"
#include "image/texture_loader.h"

namespace {

void throw_on_cuda_error(cudaError_t error, const std::string& action) {
    if (error != cudaSuccess) {
        throw std::runtime_error(action + ": " + cudaGetErrorString(error));
    }
}

}

void cleanup_scene(bvh_node*& d_nodes, hittable*& d_objects, material**& d_materials, int num_materials,
                   DeviceEnvironmentMap& env_map) {
    cleanup_bvh(d_nodes, d_objects);

    if (d_materials) {
        std::vector<material*> host_materials(num_materials);
        cudaMemcpy(host_materials.data(), d_materials, num_materials * sizeof(material*), cudaMemcpyDeviceToHost);
        for (material* d_material : host_materials) {
            if (d_material) {
                cudaFree(d_material);
            }
        }
        cudaFree(d_materials);
        d_materials = nullptr;
    }

    if (env_map.pixels) {
        cudaFree(env_map.pixels);
    }
    env_map = {};
}

void create_scene_from_xml(
    bvh_node*& d_nodes,
    hittable*& d_objects,
    int& num_nodes,
    int& num_objects,
    material**& d_materials,
    int& num_materials,
    const std::string& xml_filename,
    camera& scene_camera,
    int& image_width,
    int& image_height,
    int& samples_per_pixel,
    int& max_depth,
    Film::FilmSettings& film_settings,
    DeviceEnvironmentMap& env_map
) {
    try {
        SceneData scene_data = XMLSceneParser::load_scene(xml_filename);
        TextureData env_texture;
        const bool has_environment = scene_data.environment_settings.enabled;
        if (has_environment) {
            env_texture = load_texture(scene_data.environment_settings.texture_path);
        }

        image_width = scene_data.image_width;
        image_height = scene_data.image_height;
        samples_per_pixel = scene_data.samples_per_pixel;
        max_depth = scene_data.max_depth;
        film_settings = scene_data.film_settings;
        num_objects = static_cast<int>(scene_data.objects.size());
        env_map = {};

        // Gather all unique material pointers
        std::vector<material*> unique_materials;
        for (const auto& pair : scene_data.materials) {
            unique_materials.push_back(pair.second);
        }
        num_materials = static_cast<int>(unique_materials.size());

        // Allocate device array of material pointers
        material** h_materials_dev_ptrs = num_materials > 0 ? new material*[num_materials] : nullptr;
        for (int i = 0; i < num_materials; ++i) {
            // Allocate device memory for each material
            material* d_mat = nullptr;
            size_t mat_size = 0;
            switch (unique_materials[i]->type) {
                case LAMBERTIAN: mat_size = sizeof(lambertian); break;
                case METAL: mat_size = sizeof(metal); break;
                case DIELECTRIC: mat_size = sizeof(dielectric); break;
                case EMISSIVE: mat_size = sizeof(emissive); break;
                case FRESNEL: mat_size = sizeof(fresnel); break;
                default: mat_size = sizeof(material); break;
            }
            cudaMalloc(&d_mat, mat_size);
            cudaMemcpy(d_mat, unique_materials[i], mat_size, cudaMemcpyHostToDevice);
            h_materials_dev_ptrs[i] = d_mat;
        }

        std::vector<hittable> patched_objects = scene_data.objects;
        for (auto& obj : patched_objects) {
            // Find which material this object uses
            for (int i = 0; i < num_materials; ++i) {
                if (obj.mat_ptr == unique_materials[i]) {
                    obj.mat_ptr = h_materials_dev_ptrs[i];
                    break;
                }
            }
        }

        // Copy device material pointer array to device
        if (num_materials > 0) {
            cudaMalloc(&d_materials, num_materials * sizeof(material*));
            cudaMemcpy(d_materials, h_materials_dev_ptrs, num_materials * sizeof(material*), cudaMemcpyHostToDevice);
        } else {
            d_materials = nullptr;
        }

        // Build BVH from patched objects
        if (num_objects > 0) {
            build_bvh_from_objects(patched_objects, d_nodes, d_objects, num_nodes, num_objects);
        } else {
            d_nodes = nullptr;
            d_objects = nullptr;
            num_nodes = 0;
        }

        if (has_environment) {
            float* d_env_pixels = nullptr;
            throw_on_cuda_error(
                cudaMalloc(&d_env_pixels, env_texture.data.size() * sizeof(float)),
                "Failed to allocate environment texture on the GPU");
            const cudaError_t copy_error = cudaMemcpy(
                d_env_pixels, env_texture.data.data(), env_texture.data.size() * sizeof(float), cudaMemcpyHostToDevice);
            if (copy_error != cudaSuccess) {
                cudaFree(d_env_pixels);
                throw_on_cuda_error(copy_error, "Failed to upload environment texture to the GPU");
            }

            env_map.pixels = d_env_pixels;
            env_map.width = env_texture.width;
            env_map.height = env_texture.height;
            env_map.intensity = scene_data.environment_settings.intensity;
            env_map.rotation_radians = scene_data.environment_settings.rotation_degrees * static_cast<float>(M_PI / 180.0);
            env_map.enabled = d_env_pixels != nullptr;
        }

        // Cleanup host-side temp array
        delete[] h_materials_dev_ptrs;

        // Create camera from scene data
        scene_camera = camera(scene_data.camera_pos, scene_data.camera_look_at, scene_data.camera_up,
                             scene_data.fov, 16.0f/9.0f, scene_data.aperture, scene_data.focus_distance);
    } catch (const std::exception& e) {
        std::cerr << "Error loading XML scene: " << e.what() << std::endl;
        throw;
    }
}
