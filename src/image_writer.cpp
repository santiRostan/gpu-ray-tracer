#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include "image_writer.h"

// Include stb_image_write for PNG support
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// Utility function to convert color to RGB bytes
void color_to_rgb(const color& c, unsigned char& r, unsigned char& g, unsigned char& b) {
    // Clamp values to [0,1] and convert to [0,255]
    float red = fmax(0.0f, fmin(1.0f, c.x()));
    float green = fmax(0.0f, fmin(1.0f, c.y()));
    float blue = fmax(0.0f, fmin(1.0f, c.z()));
    
    r = static_cast<unsigned char>(255.999f * red);
    g = static_cast<unsigned char>(255.999f * green);
    b = static_cast<unsigned char>(255.999f * blue);
}

// Write PNG format using stb_image_write
void write_png(const char* filename, int width, int height, color* image) {
    // Convert image data to RGB format for stb_image_write
    std::vector<unsigned char> rgb_data(width * height * 3);
    
    for (int j = 0; j < height; ++j) {
        for (int i = 0; i < width; ++i) {
            // Flip Y coordinate (PPM format has Y=0 at bottom, PNG has Y=0 at top)
            int src_index = (height - 1 - j) * width + i;
            int dst_index = (j * width + i) * 3;
            
            color pixel_color = image[src_index];
            color_to_rgb(pixel_color, 
                        rgb_data[dst_index],     // R
                        rgb_data[dst_index + 1], // G
                        rgb_data[dst_index + 2]); // B
        }
    }
    
    // Write PNG file
    int result = stbi_write_png(filename, width, height, 3, rgb_data.data(), width * 3);
    
    if (result) {
        std::cout << "PNG image saved to " << filename << std::endl;
    } else {
        std::cerr << "Error: Failed to write PNG file " << filename << std::endl;
    }
}

// Write image in PNG format
void write_image(const char* filename, int width, int height, color* image) {
    // Add .png extension if not present
    std::string png_filename = std::string(filename);
    if (png_filename.find(".png") == std::string::npos) {
        png_filename += ".png";
    }
    write_png(png_filename.c_str(), width, height, image);
} 