#pragma once
#include <string>
#include <vector>

struct TextureData {
    int width;
    int height;
    int channels;
    std::vector<unsigned char> data;
};

TextureData load_texture(const std::string& filename); 