#pragma once
#include "vec3.h"
#include "ray.h"
#include "hit_record.h"
#include <curand_kernel.h>
#include "cuda_random.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Material type tag
enum MaterialType { LAMBERTIAN, METAL, DIELECTRIC, EMISSIVE };

struct material {
    MaterialType type;
};

struct lambertian {
    material base;
    color albedo;
};

struct metal {
    material base;
    color albedo;
    float fuzz;
};

struct dielectric {
    material base;
    float ir; // Index of Refraction
};

struct emissive {
    material base;
    color emit_color;
}; 