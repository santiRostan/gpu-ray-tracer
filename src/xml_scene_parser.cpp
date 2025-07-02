#include "scene/xml_scene_parser.h"
#include <iostream>
#include <stdexcept>
#include "third_party/tinyxml2/tinyxml2.h"

// SceneData destructor
SceneData::~SceneData() {
    // Cleanup materials
    for (auto& pair : materials) {
        delete pair.second;
    }
    
    // Cleanup spheres
    for (auto sphere : spheres) {
        delete sphere;
    }
    
    // Cleanup quads
    for (auto quad : quads) {
        delete quad;
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
            scene_data.focus_distance = focus_elem->FloatText();
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
            scene_data.spheres.push_back(parse_sphere(sphere_elem, scene_data.materials));
            sphere_elem = sphere_elem->NextSiblingElement("sphere");
        }
        
        // Parse quads
        const tinyxml2::XMLElement* quad_elem = objects_elem->FirstChildElement("quad");
        while (quad_elem) {
            scene_data.quads.push_back(parse_quad(quad_elem, scene_data.materials));
            quad_elem = quad_elem->NextSiblingElement("quad");
        }
        
        // Parse boxes
        const tinyxml2::XMLElement* box_elem = objects_elem->FirstChildElement("box");
        while (box_elem) {
            parse_box(box_elem, scene_data.materials, scene_data.quads);
            box_elem = box_elem->NextSiblingElement("box");
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
            color albedo = parse_color(color_elem);
            lambertian* lam = new lambertian{{LAMBERTIAN}, albedo};
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
    
    // Default to lambertian if type is unknown
    lambertian* lam = new lambertian{{LAMBERTIAN}, color(0.7, 0.3, 0.3)};
    return (material*)lam;
}

// Parse sphere
sphere* XMLSceneParser::parse_sphere(const tinyxml2::XMLElement* sphere_elem, 
                                    const std::map<std::string, material*>& materials) {
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
    
    return new sphere(center, radius, mat);
}

// Parse quad
quad* XMLSceneParser::parse_quad(const tinyxml2::XMLElement* quad_elem,
                                 const std::map<std::string, material*>& materials) {
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
    
    return new quad(corner, u_edge, v_edge, mat);
}

// Parse box
void XMLSceneParser::parse_box(const tinyxml2::XMLElement* box_elem, const std::map<std::string, material*>& materials, std::vector<quad*>& quads) {
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
    // Compute corners
    float w = size.x() / 2.0f, h = size.y() / 2.0f, d = size.z() / 2.0f;
    float theta = rot_y * 3.14159265f / 180.0f;
    float cos_t = cos(theta), sin_t = sin(theta);
    // 8 corners (local, then rotate, then translate)
    vec3 local[8] = {
        vec3(-w, -h, -d), vec3(w, -h, -d), vec3(w, -h, d), vec3(-w, -h, d),
        vec3(-w, h, -d),  vec3(w, h, -d),  vec3(w, h, d),  vec3(-w, h, d)
    };
    vec3 world[8];
    for (int i = 0; i < 8; ++i) {
        float x = local[i].x(), y = local[i].y(), z = local[i].z();
        float xr = x * cos_t + z * sin_t;
        float zr = -x * sin_t + z * cos_t;
        world[i] = center + vec3(xr, y, zr);
    }
    // Faces: bottom(0,1,2,3), top(4,5,6,7), front(3,2,6,7), back(0,1,5,4), left(0,3,7,4), right(1,2,6,5)
    int faces[6][4] = {
        {0,1,2,3}, {4,5,6,7}, {3,2,6,7}, {0,1,5,4}, {0,3,7,4}, {1,2,6,5}
    };
    for (int f = 0; f < 6; ++f) {
        point3 c = world[faces[f][0]];
        vec3 u = world[faces[f][1]] - c;
        vec3 v = world[faces[f][3]] - c;
        quad* q = new quad(c, u, v, mat);
        quads.push_back(q);
    }
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