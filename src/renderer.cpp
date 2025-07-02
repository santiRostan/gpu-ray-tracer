#include <iostream>
#include <cuda_runtime.h>
#include <chrono>
#include "core/vec3.h"
#include "geometry/sphere.h"
#include "materials/material.h"
#include "scene/camera.h"
#include "image/image_writer.h"
#include "geometry/quad.h"
#include "renderer.h"
#include "scene/scene_controller.h"

extern "C" void launch_render_kernel(color* d_image, int width, int height, int samples_per_pixel, quad* d_quads, int num_quads, sphere* d_spheres, int num_spheres, camera cam);

void render_scene(int image_width, int image_height, int samples_per_pixel,
                  int num_quads, int num_spheres,
                  quad** d_quads, sphere** d_spheres, const std::string& scene_name, const camera& cam) {
    std::cout << "Rendering XML scene: " << scene_name << std::endl;
    auto total_start = std::chrono::high_resolution_clock::now();
    
    // Allocate image buffer
    color* d_image;
    cudaMalloc(&d_image, image_width * image_height * sizeof(color));
    
    // Launch kernel
    launch_render_kernel(
        d_image, image_width, image_height, samples_per_pixel,
        (num_quads > 0 && d_quads) ? d_quads[0] : nullptr, num_quads,
        (num_spheres > 0 && d_spheres) ? d_spheres[0] : nullptr, num_spheres, cam
    );
        
    // Copy result back to host
    color* image = new color[image_width * image_height];
    cudaMemcpy(image, d_image, image_width * image_height * sizeof(color), cudaMemcpyDeviceToHost);
    
    // Write output file
    std::string output_name = scene_name + "_output";
    write_image(output_name.c_str(), image_width, image_height, image);
    
    // Cleanup
    delete[] image;
    cudaFree(d_image);
    
    auto total_end = std::chrono::high_resolution_clock::now();
    
    auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(total_end - total_start);
    std::cout << "Total render time:  " << total_duration.count() << " ms" << std::endl;
}
