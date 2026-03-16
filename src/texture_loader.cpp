#include "image/texture_loader.h"
#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb_image.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace {

void skip_ppm_comments(std::istream& input) {
    while (true) {
        input >> std::ws;
        if (input.peek() != '#') {
            return;
        }

        std::string comment;
        std::getline(input, comment);
    }
}

TextureData load_ascii_ppm_texture(const std::string& filename) {
    std::ifstream input(filename);
    if (!input) {
        throw std::runtime_error("Failed to open texture: " + filename);
    }

    std::string magic;
    input >> magic;
    if (magic != "P3") {
        throw std::runtime_error("Unsupported ASCII PPM header in: " + filename);
    }

    TextureData tex;
    tex.channels = 3;

    skip_ppm_comments(input);
    input >> tex.width;
    skip_ppm_comments(input);
    input >> tex.height;
    skip_ppm_comments(input);

    int max_value = 0;
    input >> max_value;

    if (!input || tex.width <= 0 || tex.height <= 0 || max_value <= 0) {
        throw std::runtime_error("Invalid ASCII PPM header in: " + filename);
    }

    tex.data.resize(static_cast<size_t>(tex.width) * tex.height * tex.channels);
    for (size_t i = 0; i < tex.data.size(); ++i) {
        skip_ppm_comments(input);

        int component = 0;
        input >> component;
        if (!input) {
            throw std::runtime_error("Unexpected end of ASCII PPM data in: " + filename);
        }

        component = std::max(0, std::min(max_value, component));
        tex.data[i] = static_cast<float>(component) / static_cast<float>(max_value);
    }

    return tex;
}

}

TextureData load_texture(const std::string& filename) {
    TextureData tex;

    std::filesystem::path path(filename);
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    tex.channels = 3;
    const bool is_hdr = extension == ".hdr";
    const bool is_ppm = extension == ".ppm";

    if (is_ppm) {
        std::ifstream ppm_probe(filename);
        std::string magic;
        ppm_probe >> magic;
        if (ppm_probe && magic == "P3") {
            return load_ascii_ppm_texture(filename);
        }
        // Fall through for binary PPM (P6) files, which stb_image can decode.
    }

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
