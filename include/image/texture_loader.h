#pragma once
#include <string>
#include <vector>

struct TextureData {
    int width = 0;
    int height = 0;
    int channels = 0;
    std::vector<float> data;
};

TextureData load_texture(const std::string& filename);
