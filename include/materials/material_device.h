#pragma once
#include "material.h"
#include "cuda_random.h"

// Scatter functions for each material type (device-only)
__device__ bool lambertian_scatter(const lambertian* mat, const ray& r_in, const hit_record& rec, color& attenuation, ray& scattered, curandState* state) {
    auto scatter_direction = rec.normal + random_unit_vector(state);
    if (scatter_direction.near_zero())
        scatter_direction = rec.normal;
    scattered = ray(rec.p, scatter_direction);
    attenuation = mat->albedo;
    return true;
}

__device__ bool metal_scatter(const metal* mat, const ray& r_in, const hit_record& rec, color& attenuation, ray& scattered, curandState* state) {
    vec3 reflected = reflect(unit_vector(r_in.direction()), rec.normal);
    scattered = ray(rec.p, reflected + mat->fuzz * random_in_unit_sphere(state));
    attenuation = mat->albedo;
    return (dot(scattered.direction(), rec.normal) > 0);
}

__device__ float reflectance(float cosine, float ref_idx) {
    // Use Schlick's approximation for reflectance.
    auto r0 = (1 - ref_idx) / (1 + ref_idx);
    r0 = r0 * r0;
    return r0 + (1 - r0) * powf((1 - cosine), 5);
}

__device__ bool dielectric_scatter(const dielectric* mat, const ray& r_in, const hit_record& rec, color& attenuation, ray& scattered, curandState* state) {
    attenuation = color(1.0, 1.0, 1.0);
    float refraction_ratio = rec.front_face ? (1.0f / mat->ir) : mat->ir;
    vec3 unit_direction = unit_vector(r_in.direction());
    float cos_theta = fmin(dot(-unit_direction, rec.normal), 1.0f);
    float sin_theta = sqrt(1.0f - cos_theta * cos_theta);
    bool cannot_refract = refraction_ratio * sin_theta > 1.0f;
    vec3 direction;
    float rand_val = random_float(0.0f, 1.0f, state);
    if (cannot_refract || reflectance(cos_theta, refraction_ratio) > rand_val)
        direction = reflect(unit_direction, rec.normal);
    else
        direction = refract(unit_direction, rec.normal, refraction_ratio);
    scattered = ray(rec.p, direction);
    return true;
}

__device__ bool emissive_scatter(const emissive* mat, const ray& r_in, const hit_record& rec, color& attenuation, ray& scattered, curandState* state) {
    attenuation = mat->emit_color;
    return false; // No scattering, just emit
}

// Unified scatter dispatcher
__device__ bool scatter_device(const material* mat, const ray& r_in, const hit_record& rec, color& attenuation, ray& scattered, curandState* state) {
    switch (mat->type) {
        case LAMBERTIAN:
            return lambertian_scatter((const lambertian*)mat, r_in, rec, attenuation, scattered, state);
        case METAL:
            return metal_scatter((const metal*)mat, r_in, rec, attenuation, scattered, state);
        case DIELECTRIC:
            return dielectric_scatter((const dielectric*)mat, r_in, rec, attenuation, scattered, state);
        case EMISSIVE:
            return emissive_scatter((const emissive*)mat, r_in, rec, attenuation, scattered, state);
    }
    return false;
} 