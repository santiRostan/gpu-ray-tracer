#include "image/texture_loader.h"
#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb_image.h"
#include <stdexcept>

TextureData load_texture(const std::string& filename) {
    TextureData tex;
    unsigned char* img = stbi_load(filename.c_str(), &tex.width, &tex.height, &tex.channels, 0);
    if (!img) throw std::runtime_error("Failed to load texture: " + filename);
    tex.data.assign(img, img + tex.width * tex.height * tex.channels);
    stbi_image_free(img);
    return tex;
} 