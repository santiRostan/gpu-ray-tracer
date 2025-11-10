#pragma once
#include "material.h"
#include "image/film.h"
#include <string>
#include "geometry/hittable.h"
#include "geometry/bvh.h"

void cleanup_scene(bvh_node*& d_nodes, hittable*& d_objects, material**& d_materials, int num_materials);

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
); 