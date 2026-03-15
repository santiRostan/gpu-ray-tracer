#pragma once
#include "material.h"
#include "cuda_random.h"

// Fresnel reflectance function (Schlick's approximation)
// Returns a float value between 0.0 and 1.0 that represents the fraction of light that reflects off a surface.
__device__ float reflectance(float cosine, float ref_idx) {
    // Use Schlick's approximation for reflectance.
    auto r0 = (1 - ref_idx) / (1 + ref_idx);
    r0 = r0 * r0;
    return r0 + (1 - r0) * powf((1 - cosine), 5);
}

// Scatter functions for each material type (device-only)
__device__ bool lambertian_scatter(const lambertian* mat, const ray& r_in, const hit_record& rec, color& attenuation, ray& scattered, curandState* state) {
    const vec3 w = unit_vector(rec.normal);
    const vec3 a = fabsf(w.x()) > 0.9f ? vec3(0.0f, 1.0f, 0.0f) : vec3(1.0f, 0.0f, 0.0f);
    const vec3 v = unit_vector(cross(w, a));
    const vec3 u = cross(v, w);
    const vec3 local_direction = random_cosine_direction(state);
    vec3 scatter_direction =
        unit_vector(local_direction.x() * u + local_direction.y() * v + local_direction.z() * w);
    if (scatter_direction.near_zero()) {
        scatter_direction = rec.normal;
    }
    scattered = ray(rec.p, scatter_direction);
    attenuation = mat->albedo;
    return true;
}

__device__ bool metal_scatter(const metal* mat, const ray& r_in, const hit_record& rec, color& attenuation, ray& scattered, curandState* state) {
    vec3 reflected = reflect(unit_vector(r_in.direction()), rec.normal);
    scattered = ray(rec.p, reflected + mat->fuzz * random_in_unit_sphere(state));
    
    // Apply Fresnel effect to metal
    float cos_theta = fmin(dot(-unit_vector(r_in.direction()), rec.normal), 1.0f);
    float fresnel = reflectance(cos_theta, 1.5f);
    attenuation = mat->albedo * (0.3f + 0.7f * fresnel);
    
    return (dot(scattered.direction(), rec.normal) > 1e-8);
}

__device__ bool dielectric_scatter(const dielectric* mat, const ray& r_in, const hit_record& rec, color& attenuation, ray& scattered, curandState* state) {
    attenuation = color(1.0, 1.0, 1.0);
    float refraction_ratio = rec.front_face ? (1.0f / mat->ir) : mat->ir;
    vec3 unit_direction = unit_vector(r_in.direction());
    float cos_theta = fmin(dot(-unit_direction, rec.normal), 1.0f);
    float sin_theta = sqrt(1.0f - cos_theta * cos_theta);
    bool cannot_refract = refraction_ratio * sin_theta > 1.0f;
    
    // Enhanced Fresnel effect for dielectrics
    float fresnel_prob = reflectance(cos_theta, refraction_ratio);
    vec3 direction;
    
    if (cannot_refract || fresnel_prob > random_float(0.0f, 1.0f, state)) {
        direction = reflect(unit_direction, rec.normal);
        // Optimized attenuation calculation
        float fresnel_factor = 0.9f + 0.1f * fresnel_prob;
        attenuation = color(fresnel_factor, fresnel_factor, fresnel_factor);
    } else {
        direction = refract(unit_direction, rec.normal, refraction_ratio);
    }
    scattered = ray(rec.p, direction);
    return true;
}

__device__ bool emissive_scatter(const emissive* mat, const ray& r_in, const hit_record& rec, color& attenuation, ray& scattered, curandState* state) {
    attenuation = mat->emit_color;
    return false; // No scattering, just emit
}

__device__ bool fresnel_scatter(const fresnel* mat, const ray& r_in, const hit_record& rec, color& attenuation, ray& scattered, curandState* state) {
    vec3 unit_direction = unit_vector(r_in.direction());
    float cos_theta = fmin(dot(-unit_direction, rec.normal), 1.0f);
    float fresnel = reflectance(cos_theta, mat->refractive_index);
    
    // Always reflect, but with Fresnel-based attenuation
    vec3 reflected = reflect(unit_direction, rec.normal);
    scattered = ray(rec.p, reflected);
    
    // Blend base color with Fresnel effect
    attenuation = mat->base_color * (0.2f + 0.8f * fresnel);
    
    return true;
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
        case FRESNEL:
            return fresnel_scatter((const fresnel*)mat, r_in, rec, attenuation, scattered, state);
    }
    return false;
} 
