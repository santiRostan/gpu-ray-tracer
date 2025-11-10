#include "scene/xml_scene_parser.h"
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <vector>
#include "third_party/tinyxml2/tinyxml2.h"

// SceneData destructor
SceneData::~SceneData() {
    // Cleanup materials
    for (auto& pair : materials) {
        delete pair.second;
    }
}

// Main parsing function
SceneData XMLSceneParser::load_scene(const std::string& filename) {
    tinyxml2::XMLDocument doc;
    tinyxml2::XMLError result = doc.LoadFile(filename.c_str());
    if (result != tinyxml2::XML_SUCCESS) {
        throw std::runtime_error("Failed to load XML file: " + filename);
    }
    const tinyxml2::XMLElement* scene_elem = doc.FirstChildElement("scene");
    if (!scene_elem) {
        throw std::runtime_error("No <scene> element found in XML file");
    }
    SceneData scene_data;
    // Parse camera
    const tinyxml2::XMLElement* camera_elem = scene_elem->FirstChildElement("camera");
    std::string focus_object_id;
    if (camera_elem) {
        camera cam = parse_camera(camera_elem);
        scene_data.camera_pos = cam.origin;
        scene_data.camera_look_at = cam.origin - cam.w; // Calculate look_at from camera direction
        scene_data.camera_up = cam.v; // Use camera's up vector
        // Parse camera parameters directly from XML
        const tinyxml2::XMLElement* fov_elem = camera_elem->FirstChildElement("fov");
        if (fov_elem) {
            scene_data.fov = fov_elem->FloatText();
        } else {
            scene_data.fov = 90.0f; // Default FOV
        }
        const tinyxml2::XMLElement* aperture_elem = camera_elem->FirstChildElement("aperture");
        if (aperture_elem) {
            scene_data.aperture = aperture_elem->FloatText();
        } else {
            scene_data.aperture = 0.0f; // Default aperture
        }
        const tinyxml2::XMLElement* focus_elem = camera_elem->FirstChildElement("focus_distance");
        if (focus_elem) {
            // Check for object attribute
            const char* object_id = focus_elem->Attribute("object");
            if (object_id) {
                focus_object_id = object_id; // Store for later calculation
                scene_data.focus_distance = 1.0f; // Temporary default
            } else {
                scene_data.focus_distance = focus_elem->FloatText();
            }
        } else {
            scene_data.focus_distance = 1.0f; // Default focus distance
        }
    } else {
        // Default camera if not specified in XML
        scene_data.camera_pos = point3(0, 0, 1);
        scene_data.camera_look_at = point3(0, 0, -1);
        scene_data.camera_up = vec3(0, 1, 0);
        scene_data.fov = 90.0f;
        scene_data.aperture = 0.1f;
        scene_data.focus_distance = 1.0f;
    }
    // Parse render parameters
    const tinyxml2::XMLElement* render_elem = scene_elem->FirstChildElement("render");
    if (render_elem) {
        const tinyxml2::XMLElement* width_elem = render_elem->FirstChildElement("image_width");
        if (width_elem) scene_data.image_width = width_elem->IntText(1280);
        const tinyxml2::XMLElement* height_elem = render_elem->FirstChildElement("image_height");
        if (height_elem) scene_data.image_height = height_elem->IntText(720);
        const tinyxml2::XMLElement* spp_elem = render_elem->FirstChildElement("samples_per_pixel");
        if (spp_elem) scene_data.samples_per_pixel = spp_elem->IntText(1500);
        const tinyxml2::XMLElement* depth_elem = render_elem->FirstChildElement("max_depth");
        if (depth_elem) scene_data.max_depth = depth_elem->IntText(50);
    }
    // Parse film settings
    const tinyxml2::XMLElement* film_elem = scene_elem->FirstChildElement("film");
    if (film_elem) {
        // Parse bloom settings
        const tinyxml2::XMLElement* bloom_elem = film_elem->FirstChildElement("bloom");
        if (bloom_elem) {
            scene_data.film_settings.bloom_enabled = true;
            const tinyxml2::XMLElement* threshold_elem = bloom_elem->FirstChildElement("threshold");
            if (threshold_elem) scene_data.film_settings.bloom_threshold = threshold_elem->FloatText();
            const tinyxml2::XMLElement* intensity_elem = bloom_elem->FirstChildElement("intensity");
            if (intensity_elem) scene_data.film_settings.bloom_intensity = intensity_elem->FloatText();
        }
        // Parse tone mapping settings
        const tinyxml2::XMLElement* tone_elem = film_elem->FirstChildElement("tone_mapping");
        if (tone_elem) {
            scene_data.film_settings.tone_mapping_enabled = true;
            const tinyxml2::XMLElement* exposure_elem = tone_elem->FirstChildElement("exposure");
            if (exposure_elem) scene_data.film_settings.exposure = exposure_elem->FloatText();
            const tinyxml2::XMLElement* gamma_elem = tone_elem->FirstChildElement("gamma");
            if (gamma_elem) scene_data.film_settings.gamma = gamma_elem->FloatText();
        }
        // Parse vignette settings
        const tinyxml2::XMLElement* vignette_elem = film_elem->FirstChildElement("vignette");
        if (vignette_elem) {
            scene_data.film_settings.vignette_enabled = true;
            const tinyxml2::XMLElement* strength_elem = vignette_elem->FirstChildElement("strength");
            if (strength_elem) scene_data.film_settings.vignette_strength = strength_elem->FloatText();
        }
    }
    // Parse materials
    const tinyxml2::XMLElement* materials_elem = scene_elem->FirstChildElement("materials");
    if (materials_elem) {
        const tinyxml2::XMLElement* material_elem = materials_elem->FirstChildElement("material");
        while (material_elem) {
            std::string id = parse_string(material_elem, "id");
            if (!id.empty()) {
                scene_data.materials[id] = parse_material(material_elem);
            }
            material_elem = material_elem->NextSiblingElement("material");
        }
    }
    // Parse objects
    const tinyxml2::XMLElement* objects_elem = scene_elem->FirstChildElement("objects");
    if (objects_elem) {
        // Parse spheres
        const tinyxml2::XMLElement* sphere_elem = objects_elem->FirstChildElement("sphere");
        while (sphere_elem) {
            std::string id = parse_string(sphere_elem, "id");
            point3 center(0, 0, 0);
            const tinyxml2::XMLElement* center_elem = sphere_elem->FirstChildElement("center");
            if (center_elem) {
                center = parse_vec3(center_elem);
            }
            if (!id.empty()) {
                scene_data.object_positions[id] = center;
            }
            scene_data.objects.push_back(parse_sphere(sphere_elem, scene_data.materials));
            sphere_elem = sphere_elem->NextSiblingElement("sphere");
        }
        // Parse quads
        const tinyxml2::XMLElement* quad_elem = objects_elem->FirstChildElement("quad");
        while (quad_elem) {
            scene_data.objects.push_back(parse_quad(quad_elem, scene_data.materials));
            quad_elem = quad_elem->NextSiblingElement("quad");
        }
        // Parse boxes
        const tinyxml2::XMLElement* box_elem = objects_elem->FirstChildElement("box");
        while (box_elem) {
            parse_box(box_elem, scene_data.materials, scene_data.objects);
            box_elem = box_elem->NextSiblingElement("box");
        }
    }
    // Calculate focus distance from object if specified
    if (!focus_object_id.empty()) {
        auto it = scene_data.object_positions.find(focus_object_id);
        if (it != scene_data.object_positions.end()) {
            point3 obj_pos = it->second;
            point3 cam_pos = scene_data.camera_pos;
            scene_data.focus_distance = distance(obj_pos, cam_pos);
        } else {
            std::cout << "Focus object not found: " << focus_object_id << std::endl;
            scene_data.focus_distance = 1.0f; // fallback if object not found
        }
    }

    return scene_data;
}

// Parse camera settings
camera XMLSceneParser::parse_camera(const tinyxml2::XMLElement* camera_elem) {
    point3 pos(0, 0, 1);
    point3 look_at(0, 0, -1);
    vec3 up(0, 1, 0);
    float fov = 90.0f;
    float aspect_ratio = 16.0f / 9.0f;
    float aperture = 0.0f;
    float focus_distance = 1.0f;
    
    // Parse position
    const tinyxml2::XMLElement* pos_elem = camera_elem->FirstChildElement("position");
    if (pos_elem) {
        pos = parse_vec3(pos_elem);
    }
    
    // Parse look_at
    const tinyxml2::XMLElement* look_elem = camera_elem->FirstChildElement("look_at");
    if (look_elem) {
        look_at = parse_vec3(look_elem);
    }
    
    // Parse up vector
    const tinyxml2::XMLElement* up_elem = camera_elem->FirstChildElement("up");
    if (up_elem) {
        up = parse_vec3(up_elem);
    }
    
    // Parse other camera parameters
    const tinyxml2::XMLElement* fov_elem = camera_elem->FirstChildElement("fov");
    if (fov_elem) {
        fov = fov_elem->FloatText();
    }
    
    const tinyxml2::XMLElement* aperture_elem = camera_elem->FirstChildElement("aperture");
    if (aperture_elem) {
        aperture = aperture_elem->FloatText();
    }
    
    const tinyxml2::XMLElement* focus_elem = camera_elem->FirstChildElement("focus_distance");
    if (focus_elem) {
        focus_distance = focus_elem->FloatText();
    }
    
    return camera(pos, look_at, up, fov, aspect_ratio, aperture, focus_distance);
}

// Parse material
material* XMLSceneParser::parse_material(const tinyxml2::XMLElement* material_elem) {
    std::string type = parse_string(material_elem, "type");
    
    if (type == "lambertian") {
        const tinyxml2::XMLElement* color_elem = material_elem->FirstChildElement("color");
        if (color_elem) {
            const color albedo = parse_color(color_elem);
            const auto lam = new lambertian{{LAMBERTIAN}, albedo};
            return (material*)lam;
        }
    }
    else if (type == "metal") {
        const tinyxml2::XMLElement* color_elem = material_elem->FirstChildElement("color");
        const tinyxml2::XMLElement* fuzz_elem = material_elem->FirstChildElement("fuzz");
        
        color albedo(0.7, 0.7, 0.7);
        float fuzz = 0.0f;
        
        if (color_elem) {
            albedo = parse_color(color_elem);
        }
        if (fuzz_elem) {
            fuzz = fuzz_elem->FloatText();
        }
        
        metal* met = new metal{{METAL}, albedo, fuzz};
        return (material*)met;
    }
    else if (type == "dielectric") {
        const tinyxml2::XMLElement* ir_elem = material_elem->FirstChildElement("refraction_index");
        float ir = 1.5f;
        if (ir_elem) {
            ir = ir_elem->FloatText();
        }
        dielectric* die = new dielectric{{DIELECTRIC}, ir};
        return (material*)die;
    }
    else if (type == "emissive") {
        const tinyxml2::XMLElement* color_elem = material_elem->FirstChildElement("color");
        color emit(1, 1, 1);
        if (color_elem) {
            emit = parse_color(color_elem);
        }
        emissive* em = new emissive{{EMISSIVE}, emit};
        return (material*)em;
    }
    else if (type == "fresnel") {
        const tinyxml2::XMLElement* color_elem = material_elem->FirstChildElement("color");
        const tinyxml2::XMLElement* ir_elem = material_elem->FirstChildElement("refraction_index");
        
        color base_color(0.8, 0.8, 0.8);
        float ir = 1.5f;
        
        if (color_elem) {
            base_color = parse_color(color_elem);
        }
        if (ir_elem) {
            ir = ir_elem->FloatText();
        }
        
        fresnel* fr = new fresnel{{FRESNEL}, base_color, ir};
        return (material*)fr;
    }
    
    // Default to lambertian if type is unknown
    lambertian* lam = new lambertian{{LAMBERTIAN}, color(0.7, 0.3, 0.3)};
    return (material*)lam;
}

// Parse sphere
hittable XMLSceneParser::parse_sphere(const tinyxml2::XMLElement* sphere_elem, const std::map<std::string, material*>& materials) {
    point3 center(0, 0, 0);
    float radius = 1.0f;
    lambertian* default_mat = new lambertian{{LAMBERTIAN}, color(0.7, 0.3, 0.3)};
    material* mat = (material*)default_mat;
    // Parse center
    const tinyxml2::XMLElement* center_elem = sphere_elem->FirstChildElement("center");
    if (center_elem) {
        center = parse_vec3(center_elem);
    }
    // Parse radius
    const tinyxml2::XMLElement* radius_elem = sphere_elem->FirstChildElement("radius");
    if (radius_elem) {
        radius = radius_elem->FloatText();
    }
    // Parse material
    std::string material_id = parse_string(sphere_elem, "material");
    if (!material_id.empty()) {
        auto it = materials.find(material_id);
        if (it != materials.end()) {
            mat = it->second;
        }
    }
    hittable h;
    h.type = SPHERE;
    h.sphere.center = center;
    h.sphere.radius = radius;
    h.mat_ptr = mat;
    return h;
}

// Parse quad
hittable XMLSceneParser::parse_quad(const tinyxml2::XMLElement* quad_elem, const std::map<std::string, material*>& materials) {
    point3 corner(0, 0, 0);
    vec3 u_edge(1, 0, 0);
    vec3 v_edge(0, 1, 0);
    lambertian* default_mat = new lambertian{{LAMBERTIAN}, color(0.7, 0.3, 0.3)};
    material* mat = (material*)default_mat;
    // Parse corner
    const tinyxml2::XMLElement* corner_elem = quad_elem->FirstChildElement("corner");
    if (corner_elem) {
        corner = parse_vec3(corner_elem);
    }
    // Parse u_edge
    const tinyxml2::XMLElement* u_elem = quad_elem->FirstChildElement("u_edge");
    if (u_elem) {
        u_edge = parse_vec3(u_elem);
    }
    // Parse v_edge
    const tinyxml2::XMLElement* v_elem = quad_elem->FirstChildElement("v_edge");
    if (v_elem) {
        v_edge = parse_vec3(v_elem);
    }
    // Parse material
    std::string material_id = parse_string(quad_elem, "material");
    if (!material_id.empty()) {
        auto it = materials.find(material_id);
        if (it != materials.end()) {
            mat = it->second;
        }
    }
    hittable h;
    h.type = QUAD;
    h.quad.Q = corner;
    h.quad.u = u_edge;
    h.quad.v = v_edge;
    h.quad.normal = unit_vector(cross(u_edge, v_edge));
    h.mat_ptr = mat;
    return h;
}

// Parse box
void XMLSceneParser::parse_box(const tinyxml2::XMLElement* box_elem, const std::map<std::string, material*>& materials, std::vector<hittable>& objects) {
    // Parse center
    point3 center(0, 0, 0);
    const tinyxml2::XMLElement* center_elem = box_elem->FirstChildElement("center");
    if (center_elem) center = parse_vec3(center_elem);
    // Parse size
    vec3 size(1, 1, 1);
    const tinyxml2::XMLElement* size_elem = box_elem->FirstChildElement("size");
    if (size_elem) size = parse_vec3(size_elem);
    // Parse rotation (degrees)
    float rot_y = 0.0f;
    const tinyxml2::XMLElement* rot_elem = box_elem->FirstChildElement("rotation");
    if (rot_elem && rot_elem->Attribute("y")) rot_y = rot_elem->FloatAttribute("y");
    // Parse material
    std::string material_id = parse_string(box_elem, "material");
    material* mat = nullptr;
    if (!material_id.empty()) {
        auto it = materials.find(material_id);
        if (it != materials.end()) mat = it->second;
    }
    if (!mat) {
        lambertian* lam = new lambertian{{LAMBERTIAN}, color(0.7, 0.3, 0.3)};
        mat = (material*)lam;
    }
    
    // Convert rotation to radians
    float rot_rad = rot_y * M_PI / 180.0f;
    float cos_rot = cos(rot_rad);
    float sin_rot = sin(rot_rad);
    
    // Create rotation matrix for Y-axis rotation
    vec3 half = size * 0.5f;
    
    // Define the 8 vertices of the box (before rotation)
    std::vector<point3> vertices = {
        center + vec3(-half.x(), -half.y(), -half.z()), // 0: bottom-back-left
        center + vec3( half.x(), -half.y(), -half.z()), // 1: bottom-back-right
        center + vec3( half.x(), -half.y(),  half.z()), // 2: bottom-front-right
        center + vec3(-half.x(), -half.y(),  half.z()), // 3: bottom-front-left
        center + vec3(-half.x(),  half.y(), -half.z()), // 4: top-back-left
        center + vec3( half.x(),  half.y(), -half.z()), // 5: top-back-right
        center + vec3( half.x(),  half.y(),  half.z()), // 6: top-front-right
        center + vec3(-half.x(),  half.y(),  half.z())  // 7: top-front-left
    };
    
    // Apply rotation to all vertices
    for (auto& vertex : vertices) {
        vec3 offset = vertex - center;
        float new_x = offset.x() * cos_rot - offset.z() * sin_rot;
        float new_z = offset.x() * sin_rot + offset.z() * cos_rot;
        vertex = center + vec3(new_x, offset.y(), new_z);
    }
    
    // Create 6 quads for the box faces
    // Face 1: bottom face (y = -half.y)
    hittable quad1;
    quad1.type = QUAD;
    quad1.quad.Q = vertices[0];
    quad1.quad.u = vertices[1] - vertices[0];
    quad1.quad.v = vertices[3] - vertices[0];
    quad1.quad.normal = unit_vector(cross(quad1.quad.u, quad1.quad.v));
    quad1.mat_ptr = mat;
    objects.push_back(quad1);
    
    // Face 2: top face (y = half.y)
    hittable quad2;
    quad2.type = QUAD;
    quad2.quad.Q = vertices[4];
    quad2.quad.u = vertices[5] - vertices[4];
    quad2.quad.v = vertices[7] - vertices[4];
    quad2.quad.normal = unit_vector(cross(quad2.quad.u, quad2.quad.v));
    quad2.mat_ptr = mat;
    objects.push_back(quad2);
    
    // Face 3: back face (z = -half.z)
    hittable quad3;
    quad3.type = QUAD;
    quad3.quad.Q = vertices[0];
    quad3.quad.u = vertices[1] - vertices[0];
    quad3.quad.v = vertices[4] - vertices[0];
    quad3.quad.normal = unit_vector(cross(quad3.quad.u, quad3.quad.v));
    quad3.mat_ptr = mat;
    objects.push_back(quad3);
    
    // Face 4: front face (z = half.z)
    hittable quad4;
    quad4.type = QUAD;
    quad4.quad.Q = vertices[3];
    quad4.quad.u = vertices[2] - vertices[3];
    quad4.quad.v = vertices[7] - vertices[3];
    quad4.quad.normal = unit_vector(cross(quad4.quad.u, quad4.quad.v));
    quad4.mat_ptr = mat;
    objects.push_back(quad4);
    
    // Face 5: left face (x = -half.x)
    hittable quad5;
    quad5.type = QUAD;
    quad5.quad.Q = vertices[0];
    quad5.quad.u = vertices[3] - vertices[0];
    quad5.quad.v = vertices[4] - vertices[0];
    quad5.quad.normal = unit_vector(cross(quad5.quad.u, quad5.quad.v));
    quad5.mat_ptr = mat;
    objects.push_back(quad5);
    
    // Face 6: right face (x = half.x)
    hittable quad6;
    quad6.type = QUAD;
    quad6.quad.Q = vertices[1];
    quad6.quad.u = vertices[2] - vertices[1];
    quad6.quad.v = vertices[5] - vertices[1];
    quad6.quad.normal = unit_vector(cross(quad6.quad.u, quad6.quad.v));
    quad6.mat_ptr = mat;
    objects.push_back(quad6);
}

// Utility functions
vec3 XMLSceneParser::parse_vec3(const tinyxml2::XMLElement* elem) {
    float x = parse_float(elem, "x");
    float y = parse_float(elem, "y");
    float z = parse_float(elem, "z");
    return vec3(x, y, z);
}

color XMLSceneParser::parse_color(const tinyxml2::XMLElement* elem) {
    float r = parse_float(elem, "r");
    float g = parse_float(elem, "g");
    float b = parse_float(elem, "b");
    return color(r, g, b);
}

float XMLSceneParser::parse_float(const tinyxml2::XMLElement* elem, const char* attr) {
    if (elem && elem->Attribute(attr)) {
        return elem->FloatAttribute(attr);
    }
    return 0.0f;
}

std::string XMLSceneParser::parse_string(const tinyxml2::XMLElement* elem, const char* attr) {
    if (elem && elem->Attribute(attr)) {
        return elem->Attribute(attr);
    }
    return "";
} 