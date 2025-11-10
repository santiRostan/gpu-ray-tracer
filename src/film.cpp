#include "image/film.h"
#include <algorithm>

Film::Film(const int width, const int height, const FilmSettings& settings)
    : image_width(width), image_height(height), settings(settings) {
    temp_buffer.resize(width * height);
    bloom_buffer.resize(width * height);
}

void Film::apply_filters(color* image, int width, int height) {
    // Apply post-processing pipeline in order
    
    // 1. Bloom
    if (settings.bloom_enabled) {
        apply_bloom(image, width, height);
    }
    
    // 2. Tone mapping
    if (settings.tone_mapping_enabled) {
        apply_tone_mapping(image, width, height);
    }
    
    // 3. Vignette
    if (settings.vignette_enabled) {
        apply_vignette(image, width, height);
    }
}

void Film::apply_bloom(color* image, int width, int height) {
    // Step 1: Extract bright areas
    for (int i = 0; i < width * height; i++) {
        float brightness = image[i].length();
        if (brightness > settings.bloom_threshold)
            bloom_buffer[i] = image[i] * (brightness - settings.bloom_threshold) / brightness;
        else
            bloom_buffer[i] = color(0, 0, 0);
    }

    // Step 2: Blur the bright areas
    gaussian_blur(bloom_buffer.data(), width, height, 2.0f);

    // Step 3: Add bloom back to original image
    for (int i = 0; i < width * height; i++) {
        image[i] = image[i] + bloom_buffer[i] * settings.bloom_intensity;
    }
}

void Film::apply_tone_mapping(color* image, int width, int height) {
    for (int i = 0; i < width * height; i++) {
        // Apply exposure
        color exposed = image[i] * settings.exposure;
        
        // Apply tone mapping (Reinhard)
        image[i] = reinhard_tone_mapping(exposed);
        
        // Apply gamma correction
        image[i] = color(
            powf(image[i].x(), 1.0f / settings.gamma),
            powf(image[i].y(), 1.0f / settings.gamma),
            powf(image[i].z(), 1.0f / settings.gamma)
        );
        
        // Clamp to valid range
        image[i] = color(
            std::min(1.0f, std::max(0.0f, image[i].x())),
            std::min(1.0f, std::max(0.0f, image[i].y())),
            std::min(1.0f, std::max(0.0f, image[i].z()))
        );
    }
}

void Film::apply_vignette(color* image, const int width, const int height) {
    const float center_x = static_cast<float>(width) * 0.5f;
    const float center_y = static_cast<float>(height) * 0.5f;
    const float max_distance = sqrtf(center_x * center_x + center_y * center_y);
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            const int index = y * width + x;

            const float dx = static_cast<float>(x) - center_x;
            const float dy = static_cast<float>(y) - center_y;
            const float distance = sqrtf(dx * dx + dy * dy);
            float vignette_factor = 1.0f - (distance / max_distance) * settings.vignette_strength;
            vignette_factor = std::max(0.0f, vignette_factor);
            
            image[index] = image[index] * vignette_factor;
        }
    }
}

void Film::gaussian_blur(color* image, const int width, const int height, float sigma) {
    // Simple 5x5 Gaussian blur
    constexpr int kernel_size = 5;
    constexpr int half_kernel = kernel_size / 2;
    
    // Create Gaussian kernel
    std::vector<float> kernel(kernel_size * kernel_size);
    float sum = 0.0f;
    
    for (int y = 0; y < kernel_size; y++) {
        for (int x = 0; x < kernel_size; x++) {
            const float dx = static_cast<float>(x) - half_kernel;
            const float dy = static_cast<float>(y) - half_kernel;
            const float value = expf(-(dx * dx + dy * dy) / (2.0f * sigma * sigma));
            kernel[y * kernel_size + x] = value;
            sum += value;
        }
    }
    
    // Normalize kernel
    for (int i = 0; i < kernel_size * kernel_size; i++) {
        kernel[i] /= sum;
    }
    
    // Apply blur horizontally and vertically
    // Copy to temp buffer
    std::copy_n(image, width * height, temp_buffer.begin());
    
    // Apply blur
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            color sum_color(0, 0, 0);
            
            for (int ky = 0; ky < kernel_size; ky++) {
                for (int kx = 0; kx < kernel_size; kx++) {
                    int sample_x = x + kx - half_kernel;
                    int sample_y = y + ky - half_kernel;
                    
                    // Clamp to image bounds
                    sample_x = std::max(0, std::min(width - 1, sample_x));
                    sample_y = std::max(0, std::min(height - 1, sample_y));
                    
                    int sample_index = sample_y * width + sample_x;
                    float kernel_value = kernel[ky * kernel_size + kx];
                    
                    sum_color = sum_color + temp_buffer[sample_index] * kernel_value;
                }
            }
            
            image[y * width + x] = sum_color;
        }
    }
}

color Film::reinhard_tone_mapping(color pixel) {
    // TODO: White point should be set in the xml, currently using default
    auto white_point = 1.0f; // Default white point
    float white_sq = white_point * white_point;

    auto r = pixel.x();
    auto g = pixel.y();
    auto b = pixel.z();

    r = (r * (1.0f + r / white_sq)) / (1.0f + r);
    g = (g * (1.0f + g / white_sq)) / (1.0f + g);
    b = (b * (1.0f + b / white_sq)) / (1.0f + b);

    return {r, g, b};
}
