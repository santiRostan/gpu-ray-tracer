#pragma once

#include <string>

struct EnvironmentSettings {
    bool enabled = false;
    std::string texture_path;
    float intensity = 1.0f;
    float rotation_degrees = 0.0f;
};

struct DeviceEnvironmentMap {
    float* pixels = nullptr;
    int width = 0;
    int height = 0;
    float intensity = 1.0f;
    float rotation_radians = 0.0f;
    bool enabled = false;
};
