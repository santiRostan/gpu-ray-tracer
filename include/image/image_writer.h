#pragma once
#include "vec3.h"

// Image writing functions
void write_png(const char* filename, int width, int height, color* image);
void write_image(const char* filename, int width, int height, color* image);

// Utility function to convert color to RGB bytes
void color_to_rgb(const color& c, unsigned char& r, unsigned char& g, unsigned char& b); 