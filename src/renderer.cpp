#include <iostream>
#include <cuda_runtime.h>
#include <chrono>
#include "core/vec3.h"
#include "geometry/hittable.h"
#include "geometry/bvh.h"
#include "materials/material.h"
#include "scene/camera.h"
#include "image/image_writer.h"
#include "renderer.h"
#include "scene/scene_controller.h"
#include "core/gpu_monitor.h"
#include "image/film.h"

void render_scene(int image_width, int image_height, int samples_per_pixel, int max_depth,
                  bvh_node* d_nodes, hittable* d_objects, const std::string& scene_name, const camera& cam,
                  const Film::FilmSettings& film_settings) {
    std::cout << "Rendering: " << image_width << "x" << image_height << " | " 
              << samples_per_pixel << " spp | " << max_depth << " depth" << std::endl;
    const auto total_start = std::chrono::high_resolution_clock::now();
    
    // Allocate image buffer
    color* d_image;
    cudaMalloc(&d_image, image_width * image_height * sizeof(color));
    
    // Show GPU status before rendering
    print_gpu_status();
    
    // Start performance timer
    start_performance_timer();
    
    // Launch kernel
    launch_render_kernel(
        d_image, image_width, image_height, samples_per_pixel, max_depth,
        d_nodes, d_objects, cam  // Re-enable BVH
    );
    
    // Get performance metrics
    double kernel_time = get_performance_timer();
    size_t total_rays = static_cast<size_t>(image_width) * image_height * samples_per_pixel;
    
    // Print detailed performance statistics
    print_performance_stats(kernel_time, total_rays, image_width, image_height, samples_per_pixel);
    
    // Copy result back to host
    auto* image = new color[image_width * image_height];
    cudaMemcpy(image, d_image, image_width * image_height * sizeof(color), cudaMemcpyDeviceToHost);
    
    // Apply post-processing with Film class
    Film film(image_width, image_height, film_settings);
    
    // Apply all enabled filters
    film.apply_filters(image, image_width, image_height);
    
    // Write output file
    std::string output_name = scene_name + "_output";
    write_image(output_name.c_str(), image_width, image_height, image);
    
    // Cleanup
    delete[] image;
    cudaFree(d_image);
    
    auto total_end = std::chrono::high_resolution_clock::now();
    auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(total_end - total_start);
    std::cout << "Total time: " << total_duration.count() << " ms" << std::endl;
}
