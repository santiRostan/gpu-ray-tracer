#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <fstream>
#include <algorithm>
#include "geometry/hittable.h"
#include "geometry/bvh.h"
#include "scene/camera.h"
#include "renderer.h"
#include "core/cuda_utils.h"
#include "scene/scene_controller.h"
#include "core/constants.h"

using namespace std;

int main(int argc, char* argv[]) {
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

    string requested_scene;
    bool list_scenes_only = false;
    for (int i = 1; i < argc; ++i) {
        const string arg = argv[i];
        if (arg == "--scene") {
            if (i + 1 >= argc) {
                cerr << "Missing value for --scene" << endl;
                return 1;
            }
            requested_scene = argv[++i];
        } else if (arg == "--list-scenes") {
            list_scenes_only = true;
        } else {
            cerr << "Unknown argument: " << arg << endl;
            return 1;
        }
    }

    if (scene_files.empty() && requested_scene.empty()) {
        cerr << "No scenes found in " << scenes_path << endl;
        return 1;
    }

    if (!scene_files.empty() && (requested_scene.empty() || list_scenes_only)) {
        cout << "\nAvailable scenes:" << endl;
        for (size_t i = 0; i < scene_files.size(); ++i) {
            cout << "  " << (i + 1) << ". " << scene_files[i] << endl;
        }
    }

    if (list_scenes_only && requested_scene.empty()) {
        return 0;
    }

    string xml_filename;
    if (!requested_scene.empty()) {
        filesystem::path requested_path(requested_scene);
        if (requested_path.is_relative()) {
            filesystem::path direct_candidate = requested_path;
            filesystem::path scene_candidate = scenes_dir / requested_path;
            if (exists(direct_candidate)) {
                requested_path = direct_candidate;
            } else if (exists(scene_candidate)) {
                requested_path = scene_candidate;
            } else if (!requested_path.has_parent_path()) {
                filesystem::path filename_candidate = scenes_dir / requested_path.filename();
                if (exists(filename_candidate)) {
                    requested_path = filename_candidate;
                }
            }
        }

        if (!exists(requested_path)) {
            cerr << "Scene file not found: " << requested_scene << endl;
            return 1;
        }

        xml_filename = filesystem::absolute(requested_path).lexically_normal().string();
    } else {
        cout << "\nEnter scene number (1-" << scene_files.size();
        cout << "): ";
        int choice;
        cin >> choice;

        // Validate choice and get filename
        if (choice >= 1 && choice <= static_cast<int>(scene_files.size())) {
            xml_filename = scenes_path + scene_files[choice - 1];
        } else {
            cout << "Invalid choice. Using first scene." << endl;
            xml_filename = scenes_path + scene_files[0];
        }
    }

    try {
        // BVH and object arrays
        bvh_node* d_nodes = nullptr;
        hittable* d_objects = nullptr;
        int num_nodes = 0;
        int num_objects = 0;
        material** d_materials = nullptr;
        int num_materials = 0;
        DeviceEnvironmentMap env_map;
        camera scene_camera;

        // Render parameters (will be set by XML scene)
        int image_width, image_height, samples_per_pixel, max_depth;
        Film::FilmSettings film_settings;

        create_scene_from_xml(
            d_nodes, d_objects, num_nodes, num_objects, d_materials, num_materials, 
            xml_filename, scene_camera, image_width, image_height, samples_per_pixel, max_depth, film_settings, env_map);

        render_scene(image_width, image_height, samples_per_pixel, max_depth,
                     d_nodes, d_objects, xml_filename, scene_camera, film_settings, env_map);

        cleanup_scene(d_nodes, d_objects, d_materials, num_materials, env_map);

        cout << "Scene rendered successfully!" << endl;
    } catch (const runtime_error& e) {
        cerr << "Rendering failed: " << e.what() << endl;
        return 1;
    }

    return 0;
}
