#pragma once
#include "ray.h"
#include "vec3.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class camera {
public:
    __host__ __device__ camera() {
        auto aspect_ratio = 16.0f / 9.0f;
        auto viewport_height = 2.0f;
        auto viewport_width = aspect_ratio * viewport_height;
        auto focal_length = 1.0f;

        origin = point3(0, 0, 0);
        horizontal = vec3(viewport_width, 0.0, 0.0);
        vertical = vec3(0.0, viewport_height, 0.0);
        lower_left_corner = origin - horizontal/2 - vertical/2 - vec3(0, 0, focal_length);
    }

    __host__ __device__ camera(point3 lookfrom, point3 lookat, vec3 vup, 
                              float vfov, float aspect_ratio, float aperture, 
                              float focus_dist) {
        auto theta = degrees_to_radians(vfov);
        auto h = tan(theta/2);
        auto viewport_height = 2.0f * h;
        auto viewport_width = aspect_ratio * viewport_height;

        w = unit_vector(lookfrom - lookat);
        u = unit_vector(cross(vup, w));
        v = cross(w, u);

        origin = lookfrom;
        horizontal = focus_dist * viewport_width * u;
        vertical = focus_dist * viewport_height * v;
        lower_left_corner = origin - horizontal/2 - vertical/2 - focus_dist*w;

        lens_radius = aperture / 2;
    }

    __host__ __device__ ray get_ray(float s, float t) const {
        vec3 rd = lens_radius * random_in_unit_disk();
        vec3 offset = u * rd.x() + v * rd.y();

        return ray(origin + offset, 
                  lower_left_corner + s*horizontal + t*vertical - origin - offset);
    }

    __device__ ray get_ray(float s, float t, curandState* state) const {
        vec3 rd = lens_radius * random_in_unit_disk(state);
        vec3 offset = u * rd.x() + v * rd.y();

        return ray(origin + offset, 
                  lower_left_corner + s*horizontal + t*vertical - origin - offset);
    }

private:
    __host__ __device__ static float degrees_to_radians(float degrees) {
        return degrees * M_PI / 180.0f;
    }

    __host__ __device__ static vec3 random_in_unit_disk() {
        // Placeholder for host compilation
        return vec3(0.5f, 0.5f, 0.0f);
    }

    __device__ static vec3 random_in_unit_disk(curandState* state) {
        while (true) {
            auto p = vec3(curand_uniform(state) * 2 - 1, curand_uniform(state) * 2 - 1, 0);
            if (p.length_squared() >= 1) continue;
            return p;
        }
    }

    __host__ __device__ static float random_float(float min, float max) {
        return min + (max-min)*(rand() / (RAND_MAX + 1.0f));
    }

    __device__ static float random_float(float min, float max, curandState* state) {
        return min + (max-min)*curand_uniform(state);
    }

public:
    point3 origin;
    point3 lower_left_corner;
    vec3 horizontal;
    vec3 vertical;
    vec3 u, v, w;
    float lens_radius;
}; 