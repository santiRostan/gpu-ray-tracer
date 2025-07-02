#pragma once
#include <string>
#include <vector>
#include <map>
#include "vec3.h"
#include "sphere.h"
#include "material.h"
#include "quad.h"
#include "camera.h"
#include "tinyxml2/tinyxml2.h"

// Forward declarations
struct quad;
struct lambertian;
struct metal;
struct dielectric;
struct emissive;

// Scene data structure to hold parsed XML data
struct SceneData {
    // Camera settings
    point3 camera_pos;
    point3 camera_look_at;
    vec3 camera_up;
    float fov;
    float aperture;
    float focus_distance;
    
    // Materials (id -> material)
    std::map<std::string, material*> materials;
    
    // Objects
    std::vector<sphere*> spheres;
    std::vector<quad*> quads;
    
    // Render parameters
    int image_width = 1280;
    int image_height = 720;
    int samples_per_pixel = 1500;
    
    // Cleanup
    ~SceneData();
};

// XML Scene Parser class
class XMLSceneParser {
public:
    // Main parsing function
    static SceneData load_scene(const std::string& filename);
    
private:
    // Helper parsing functions
    static camera parse_camera(const tinyxml2::XMLElement* camera_elem);
    static material* parse_material(const tinyxml2::XMLElement* material_elem);
    static sphere* parse_sphere(const tinyxml2::XMLElement* sphere_elem, 
                               const std::map<std::string, material*>& materials);
    static quad* parse_quad(const tinyxml2::XMLElement* quad_elem,
                           const std::map<std::string, material*>& materials);
    static void parse_box(const tinyxml2::XMLElement* box_elem, const std::map<std::string, material*>& materials, std::vector<quad*>& quads);
    
    // Utility functions
    static vec3 parse_vec3(const tinyxml2::XMLElement* elem);
    static color parse_color(const tinyxml2::XMLElement* elem);
    static float parse_float(const tinyxml2::XMLElement* elem, const char* attr);
    static std::string parse_string(const tinyxml2::XMLElement* elem, const char* attr);
}; 