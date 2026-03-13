#include "image/texture_loader.h"
#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb_image.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <stdexcept>

TextureData load_texture(const std::string& filename) {
    TextureData tex;

    std::filesystem::path path(filename);
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    tex.channels = 3;
    const bool is_hdr = extension == ".hdr";

    if (is_hdr) {
        float* img = stbi_loadf(filename.c_str(), &tex.width, &tex.height, &tex.channels, 3);
        if (!img) {
            throw std::runtime_error("Failed to load texture: " + filename);
        }

        tex.channels = 3;
        tex.data.assign(img, img + tex.width * tex.height * tex.channels);
        stbi_image_free(img);
        return tex;
    }

    unsigned char* img = stbi_load(filename.c_str(), &tex.width, &tex.height, &tex.channels, 3);
    if (!img) {
        throw std::runtime_error("Failed to load texture: " + filename);
    }

    tex.channels = 3;
    tex.data.resize(tex.width * tex.height * tex.channels);
    for (size_t i = 0; i < tex.data.size(); ++i) {
        tex.data[i] = static_cast<float>(img[i]) / 255.0f;
    }

    stbi_image_free(img);
    return tex;
}
