#pragma once
#include "geometry/hittable.h"
#include "geometry/bvh.h"
#include "materials/material.h"
#include "image/film.h"
#include <string>

// CUDA kernel function declaration
extern "C" void launch_render_kernel(color* d_image, int width, int height, int samples_per_pixel, int max_depth, 
                                    bvh_node* d_nodes, hittable* d_objects, camera cam);

// Render function for XML scene data
void render_scene(int image_width, int image_height, int samples_per_pixel, int max_depth,
                  bvh_node* d_nodes, hittable* d_objects, const std::string& scene_name, const camera& cam,
                  const Film::FilmSettings& film_settings = Film::FilmSettings{}); 