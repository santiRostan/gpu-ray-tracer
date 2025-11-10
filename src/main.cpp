#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <fstream>
#include "geometry/hittable.h"
#include "geometry/bvh.h"
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
        // BVH and object arrays
        bvh_node* d_nodes = nullptr;
        hittable* d_objects = nullptr;
        int num_nodes = 0;
        int num_objects = 0;
        material** d_materials = nullptr;
        int num_materials = 0;
        camera scene_camera;

        // Render parameters (will be set by XML scene)
        int image_width, image_height, samples_per_pixel, max_depth;
        Film::FilmSettings film_settings;

        create_scene_from_xml(
            d_nodes, d_objects, num_nodes, num_objects, d_materials, num_materials, 
            xml_filename, scene_camera, image_width, image_height, samples_per_pixel, max_depth, film_settings);

        render_scene(image_width, image_height, samples_per_pixel, max_depth,
                     d_nodes, d_objects, xml_filename, scene_camera, film_settings);

        cleanup_scene(d_nodes, d_objects, d_materials, num_materials);

        cout << "Scene rendered successfully!" << endl;
    } catch (const runtime_error& e) {
        cerr << "Rendering failed: " << e.what() << endl;
        return 1;
    }

    return 0;
} 