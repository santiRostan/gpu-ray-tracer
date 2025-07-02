#pragma once
#include "sphere.h"
#include "material.h"
#include "quad.h"
#include <string>

// Forward declarations
struct quad;
struct lambertian;
struct metal;
struct dielectric;
struct emissive;

void cleanup_scene(
    quad** host_quads, int num_quads,
    sphere** host_spheres, int num_spheres,
    lambertian** host_lambertians, metal** host_metals, dielectric** host_dielectrics, emissive** host_emissives,
    quad** d_quads, sphere** d_spheres,
    lambertian** d_lambertians, metal** d_metals, dielectric** d_dielectrics, emissive** d_emissives
);

// XML scene loading function
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
); 