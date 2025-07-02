#include <iostream>
#include <cuda_runtime.h>
#include <vector>
#include "core/vec3.h"
#include "geometry/sphere.h"
#include "materials/material.h"
#include "scene/camera.h"
#include "geometry/quad.h"
#include "scene/xml_scene_parser.h"

void cleanup_scene(
    quad** host_quads, int num_quads,
    sphere** host_spheres, int num_spheres,
    lambertian** host_lambertians, metal** host_metals, dielectric** host_dielectrics, emissive** host_emissives,
    quad** d_quads, sphere** d_spheres,
    lambertian** d_lambertians, metal** d_metals, dielectric** d_dielectrics, emissive** d_emissives
) {
    int max_materials = std::max(num_quads, num_spheres);

    // Cleanup host quads
    if (host_quads) {
        for (int i = 0; i < num_quads; ++i) {
            if (host_quads[i]) delete host_quads[i];
        }
        delete[] host_quads;
    }

    // Cleanup host spheres
    if (host_spheres) {
        for (int i = 0; i < num_spheres; ++i) {
            if (host_spheres[i]) delete host_spheres[i];
        }
        delete[] host_spheres;
    }

    // Cleanup host materials
    if (host_lambertians) {
        for (int i = 0; i < max_materials; ++i) {
            if (host_lambertians[i]) delete host_lambertians[i];
        }
        delete[] host_lambertians;
    }
    if (host_metals) {
        for (int i = 0; i < max_materials; ++i) {
            if (host_metals[i]) delete host_metals[i];
        }
        delete[] host_metals;
    }
    if (host_dielectrics) {
        for (int i = 0; i < max_materials; ++i) {
            if (host_dielectrics[i]) delete host_dielectrics[i];
        }
        delete[] host_dielectrics;
    }
    if (host_emissives) {
        for (int i = 0; i < max_materials; ++i) {
            if (host_emissives[i]) delete host_emissives[i];
        }
        delete[] host_emissives;
    }

    // Cleanup device memory (CUDA)
    if (d_quads && d_quads[0]) cudaFree(d_quads[0]);
    if (d_spheres && d_spheres[0]) cudaFree(d_spheres[0]);
    if (d_lambertians) {
        for (int i = 0; i < max_materials; ++i) {
            if (d_lambertians[i]) cudaFree(d_lambertians[i]);
        }
        delete[] d_lambertians;
    }
    if (d_metals) {
        for (int i = 0; i < max_materials; ++i) {
            if (d_metals[i]) cudaFree(d_metals[i]);
        }
        delete[] d_metals;
    }
    if (d_dielectrics) {
        for (int i = 0; i < max_materials; ++i) {
            if (d_dielectrics[i]) cudaFree(d_dielectrics[i]);
        }
        delete[] d_dielectrics;
    }
    if (d_emissives) {
        for (int i = 0; i < max_materials; ++i) {
            if (d_emissives[i]) cudaFree(d_emissives[i]);
        }
        delete[] d_emissives;
    }
    if (d_quads) delete[] d_quads;
    if (d_spheres) delete[] d_spheres;
}

void create_scene_from_xml(
    quad**& host_quads, int& num_quads,
    sphere**& host_spheres, int& num_spheres,
    lambertian**& host_lambertians, metal**& host_metals, dielectric**& host_dielectrics, emissive**& host_emissives,
    quad**& d_quads, sphere**& d_spheres,
    lambertian**& d_lambertians, metal**& d_metals, dielectric**& d_dielectrics, emissive**& d_emissives,
    const std::string& xml_filename,
    camera& scene_camera,
    int& image_width,
    int& image_height,
    int& samples_per_pixel
) {
    try {
        // Load scene data from XML
        SceneData scene_data = XMLSceneParser::load_scene(xml_filename);
        
        // Set render parameters
        image_width = scene_data.image_width;
        image_height = scene_data.image_height;
        samples_per_pixel = scene_data.samples_per_pixel;
        
        // Set counts
        num_quads = scene_data.quads.size();
        num_spheres = scene_data.spheres.size();
        
        // Allocate arrays
        host_quads = new quad*[num_quads];
        host_spheres = new sphere*[num_spheres];
        host_lambertians = new lambertian*[std::max(num_quads, num_spheres)];
        host_metals = new metal*[std::max(num_quads, num_spheres)];
        host_dielectrics = new dielectric*[std::max(num_quads, num_spheres)];
        host_emissives = new emissive*[std::max(num_quads, num_spheres)];
        d_quads = new quad*[num_quads];
        d_spheres = new sphere*[num_spheres];
        d_lambertians = new lambertian*[std::max(num_quads, num_spheres)];
        d_metals = new metal*[std::max(num_quads, num_spheres)];
        d_dielectrics = new dielectric*[std::max(num_quads, num_spheres)];
        d_emissives = new emissive*[std::max(num_quads, num_spheres)];
        
        // Initialize pointers to nullptr
        int max_materials = std::max(num_quads, num_spheres);
        for (int i = 0; i < max_materials; ++i) {
            host_lambertians[i] = nullptr;
            host_metals[i] = nullptr;
            host_dielectrics[i] = nullptr;
            host_emissives[i] = nullptr;
            d_lambertians[i] = nullptr;
            d_metals[i] = nullptr;
            d_dielectrics[i] = nullptr;
            d_emissives[i] = nullptr;
        }

        // Copy quads from scene data
        for (int i = 0; i < num_quads; ++i) {
            host_quads[i] = scene_data.quads[i];
        }

        // Copy spheres from scene data
        for (int i = 0; i < num_spheres; ++i) {
            host_spheres[i] = scene_data.spheres[i];
        }

        // Allocate contiguous arrays on device
        if (num_quads > 0) {
            quad* d_quads_array;
            cudaMalloc(&d_quads_array, num_quads * sizeof(quad));
            
            // First, allocate materials on device
            std::vector<material*> d_materials(num_quads);
            for (int i = 0; i < num_quads; ++i) {
                material* host_mat = host_quads[i]->mat_ptr;
                size_t material_size;
                if (host_mat->type == LAMBERTIAN) {
                    material_size = sizeof(lambertian);
                } else if (host_mat->type == METAL) {
                    material_size = sizeof(metal);
                } else if (host_mat->type == DIELECTRIC) {
                    material_size = sizeof(dielectric);
                } else if (host_mat->type == EMISSIVE) {
                    material_size = sizeof(emissive);
                } else {
                    material_size = sizeof(material);
                }
                cudaMalloc(&d_materials[i], material_size);
                cudaMemcpy(d_materials[i], host_mat, material_size, cudaMemcpyHostToDevice);
            }
            
            // Create quads with device material pointers and copy to device
            for (int i = 0; i < num_quads; ++i) {
                quad quad_with_device_mat = *host_quads[i];
                quad_with_device_mat.mat_ptr = d_materials[i];
                cudaMemcpy(&d_quads_array[i], &quad_with_device_mat, sizeof(quad), cudaMemcpyHostToDevice);
            }
            
            d_quads[0] = d_quads_array;
        }
        
        if (num_spheres > 0) {
            sphere* d_spheres_array;
            cudaMalloc(&d_spheres_array, num_spheres * sizeof(sphere));
            
            // First, allocate materials on device
            std::vector<material*> d_materials(num_spheres);
            for (int i = 0; i < num_spheres; ++i) {
                material* host_mat = host_spheres[i]->mat_ptr;
                if (host_mat->type == LAMBERTIAN) {
                    lambertian* lam = (lambertian*)host_mat;
                }
                size_t material_size;
                if (host_mat->type == LAMBERTIAN) {
                    material_size = sizeof(lambertian);
                } else if (host_mat->type == METAL) {
                    material_size = sizeof(metal);
                } else if (host_mat->type == DIELECTRIC) {
                    material_size = sizeof(dielectric);
                } else if (host_mat->type == EMISSIVE) {
                    material_size = sizeof(emissive);
                } else {
                    material_size = sizeof(material);
                }
                cudaMalloc(&d_materials[i], material_size);
                cudaMemcpy(d_materials[i], host_mat, material_size, cudaMemcpyHostToDevice);
            }
            
            // Create spheres with device material pointers and copy to device
            for (int i = 0; i < num_spheres; ++i) {
                sphere sphere_with_device_mat = *host_spheres[i];
                sphere_with_device_mat.mat_ptr = d_materials[i];
                cudaMemcpy(&d_spheres_array[i], &sphere_with_device_mat, sizeof(sphere), cudaMemcpyHostToDevice);
            }
            
            d_spheres[0] = d_spheres_array;
        }
        
        // Create camera from scene data
        scene_camera = camera(scene_data.camera_pos, scene_data.camera_look_at, scene_data.camera_up, 
                             scene_data.fov, 16.0f/9.0f, scene_data.aperture, scene_data.focus_distance);
        
        // Materials are handled by the objects themselves
        // The SceneData destructor will clean up any unused materials
        // We need to prevent double deletion by clearing the materials map
        scene_data.materials.clear();
        
    } catch (const std::exception& e) {
        std::cerr << "Error loading XML scene: " << e.what() << std::endl;
        throw;
    }
} 