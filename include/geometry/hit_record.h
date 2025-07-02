#pragma once
#include "vec3.h"
#include "ray.h"

class material;

struct hit_record {
    point3 p;
    vec3 normal;
    material* mat_ptr;
    float t;
    bool front_face;

    __host__ __device__ inline void set_face_normal(const ray& r, const vec3& outward_normal) {
        front_face = dot(r.direction(), outward_normal) < 0;
        normal = front_face ? outward_normal : -outward_normal;
    }
}; 