#include <iostream>
#include <cuda_runtime.h>
#include <vector>
#include "geometry/hittable.h"
#include "geometry/bvh.h"
#include "materials/material.h"
#include "scene/camera.h"
#include "scene/xml_scene_parser.h"

void cleanup_scene(bvh_node*& d_nodes, hittable*& d_objects, material**& d_materials, int num_materials) {
    cleanup_bvh(d_nodes, d_objects);
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
    Film::FilmSettings& film_settings
) {
    try {
        SceneData scene_data = XMLSceneParser::load_scene(xml_filename);
        image_width = scene_data.image_width;
        image_height = scene_data.image_height;
        samples_per_pixel = scene_data.samples_per_pixel;
        max_depth = scene_data.max_depth;
        film_settings = scene_data.film_settings;
        num_objects = static_cast<int>(scene_data.objects.size());

        // Gather all unique material pointers
        std::vector<material*> unique_materials;
        std::vector<material*> host_material_ptrs;
        for (const auto& pair : scene_data.materials) {
            unique_materials.push_back(pair.second);
        }
        num_materials = static_cast<int>(unique_materials.size());

        // Allocate device array of material pointers
        material** h_materials_dev_ptrs = new material*[num_materials];
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
        cudaMalloc(&d_materials, num_materials * sizeof(material*));
        cudaMemcpy(d_materials, h_materials_dev_ptrs, num_materials * sizeof(material*), cudaMemcpyHostToDevice);

        // Build BVH from patched objects
        if (num_objects > 0) {
            build_bvh_from_objects(patched_objects, d_nodes, d_objects, num_nodes, num_objects);
        } else {
            d_nodes = nullptr;
            d_objects = nullptr;
            num_nodes = 0;
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