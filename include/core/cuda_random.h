#pragma once
#include <curand_kernel.h>
#include "vec3.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

__device__ inline float random_float(float min, float max, curandState* state) {
    return min + (max-min)*curand_uniform(state);
}

__device__ inline vec3 random_unit_vector(curandState* state) {
    float a = random_float(0.0f, 2.0f * M_PI, state);
    float z = random_float(-1.0f, 1.0f, state);
    float r = sqrtf(1.0f - z*z);
    return vec3(r*cosf(a), r*sinf(a), z);
}

__device__ inline vec3 random_in_unit_sphere(curandState* state) {
    while (true) {
        vec3 p = vec3(random_float(-1.0f, 1.0f, state), random_float(-1.0f, 1.0f, state), random_float(-1.0f, 1.0f, state));
        if (p.length_squared() >= 1.0f) continue;
        return p;
    }
}

__device__ inline vec3 random_cosine_direction(curandState* state) {
    const float r1 = random_float(0.0f, 1.0f, state);
    const float r2 = random_float(0.0f, 1.0f, state);
    const float phi = 2.0f * M_PI * r1;
    const float sqrt_r2 = sqrtf(r2);
    const float x = cosf(phi) * sqrt_r2;
    const float y = sinf(phi) * sqrt_r2;
    const float z = sqrtf(fmaxf(0.0f, 1.0f - r2));
    return vec3(x, y, z);
}
