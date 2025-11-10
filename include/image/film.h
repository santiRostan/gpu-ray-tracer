#pragma once
#include "vec3.h"
#include <vector>

class Film {
public:
    // Film settings structure
    struct FilmSettings {
        // Bloom settings
        bool bloom_enabled = false;
        float bloom_threshold = 1.0f;
        float bloom_intensity = 0.3f;
        
        // Tone mapping settings
        bool tone_mapping_enabled = true;
        float exposure = 1.0f;
        float gamma = 2.2f;
        
        // Vignette settings
        bool vignette_enabled = false;
        float vignette_strength = 0.1f;
    };
    
    Film(int width, int height, const FilmSettings& settings = FilmSettings{});
    
    // Main post-processing pipeline
    void apply_filters(color* image, int width, int height);
private:
    int image_width;
    int image_height;
    FilmSettings settings;

    // Individual effects (internal use)
    void apply_bloom(color* image, int width, int height);
    void apply_tone_mapping(color* image, int width, int height);
    void apply_vignette(color* image, int width, int height);
    
    // Utility functions
    void gaussian_blur(color* image, int width, int height, float sigma = 1.0f);
    color reinhard_tone_mapping(color pixel);
    
    // Temporary buffers for processing
    std::vector<color> temp_buffer;
    std::vector<color> bloom_buffer;
}; 