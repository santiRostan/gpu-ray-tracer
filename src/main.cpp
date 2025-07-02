#include <iostream>
#include <cuda_runtime.h>
#include <filesystem>
#include <vector>
#include <string>
#include <fstream>
#include "geometry/sphere.h"
#include "materials/material.h"
#include "scene/camera.h"
#include "renderer.h"
#include "core/cuda_utils.h"
#include "scene/scene_controller.h"
#include "core/constants.h"

using namespace std;

int main() {
    cout << "Starting CUDA ray tracer..." << endl;
    try {
        initialize_cuda();
    } catch (const runtime_error& e) {
        cerr << e.what() << endl;
        return 1;
    }
    
    // Image dimensions
    int image_width = 1280;
    int image_height = 720;
    int samples_per_pixel = 1500;

    // Store available XML scene files
    const string scenes_path = SCENES_PATH + string("/");
    vector<string> scene_files;
    filesystem::path scenes_dir(SCENES_PATH);
    if (exists(scenes_dir)) {
        for (const auto& entry : filesystem::directory_iterator(scenes_dir)) {
            if (entry.path().extension() == ".xml") {
                scene_files.push_back(entry.path().filename().string());
            }
        }
    }
    cout << "scene files: " << scene_files.size() << endl;
    
    // Sort scene files for consistent ordering
    sort(scene_files.begin(), scene_files.end());

    // Show available XML scene files with numbers
    cout << "\nAvailable scenes:" << endl;
    for (size_t i = 0; i < scene_files.size(); ++i) {
        cout << "  " << (i + 1) << ". " << scene_files[i] << endl;
    }

    cout << "\nEnter scene number (1-" << scene_files.size();
    cout << "): ";
    
    int choice;
    cin >> choice;
    
    string xml_filename;
    
    // Validate choice and get filename
    if (choice >= 1 && choice <= static_cast<int>(scene_files.size())) {
        xml_filename = scenes_path + scene_files[choice - 1];
    } else {
        cout << "Invalid choice. Using first scene." << endl;
        xml_filename = scenes_path + scene_files[0];
    }
    
    try {
        // Allocate host memory
        quad** host_quads = nullptr;
        sphere** host_spheres = nullptr;
        lambertian** host_lambertians = nullptr;
        metal** host_metals = nullptr;
        dielectric** host_dielectrics = nullptr;
        emissive** host_emissives = nullptr;

        // Allocate device memory
        quad** d_quads = nullptr;
        sphere** d_spheres = nullptr;
        lambertian** d_lambertians = nullptr;
        metal** d_metals = nullptr;
        dielectric** d_dielectrics = nullptr;
        emissive** d_emissives = nullptr;
        int num_quads = 0, num_spheres = 0;
        camera scene_camera;
        
        create_scene_from_xml(host_quads, num_quads, host_spheres, num_spheres,
                            host_lambertians, host_metals, host_dielectrics, host_emissives,
                            d_quads, d_spheres, d_lambertians, d_metals, d_dielectrics, d_emissives,
                            xml_filename, scene_camera,
                            image_width, image_height, samples_per_pixel);
        
        // Render the scene
        render_scene(image_width, image_height, samples_per_pixel, 
                     num_quads, num_spheres, d_quads, d_spheres,
                     xml_filename, scene_camera);

        // FIXME: Wrong memory management.
        cleanup_scene(host_quads, num_quads, host_spheres, num_spheres,
                     host_lambertians, host_metals, host_dielectrics, host_emissives,
                     d_quads, d_spheres, d_lambertians, d_metals, d_dielectrics, d_emissives);
        
        cout << "Scene rendered successfully!" << endl;
    } catch (const runtime_error& e) {
        cerr << "Rendering failed: " << e.what() << endl;
        return 1;
    }
    return 0;
} 