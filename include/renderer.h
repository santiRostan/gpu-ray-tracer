#pragma once
#include "geometry/sphere.h"
#include "materials/material.h"
#include "geometry/quad.h"
#include <string>

// Forward declaration
struct quad;

// CUDA kernel function declaration
extern "C" void launch_render_kernel(color* d_image, int width, int height, int samples_per_pixel, quad* d_quads, int num_quads, sphere* d_spheres, int num_spheres, camera cam);

// Render function for XML scene data
void render_scene(int image_width, int image_height, int samples_per_pixel,
                  int num_quads, int num_spheres,
                  quad** d_quads, sphere** d_spheres, const std::string& scene_name, const camera& cam); 