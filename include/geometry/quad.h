#pragma once
#include "vec3.h"
#include "ray.h"
#include "material.h"
#include "hit_record.h"

struct quad {
    point3 Q; // Corner point
    vec3 u, v; // Edge vectors
    vec3 normal; // Normal (unit vector)
    material* mat_ptr;

    __device__ quad() {}
    __device__ quad(const point3& Q_, const vec3& u_, const vec3& v_, material* m)
        : Q(Q_), u(u_), v(v_), mat_ptr(m) {
        normal = unit_vector(cross(u, v));
    }

    __device__ bool hit(const ray& r, float t_min, float t_max, hit_record& rec) const {
        float ulen = u.length();
        float vlen = v.length();
        // Ray-plane intersection
        float denom = dot(normal, r.direction());
        if (fabs(denom) < 1e-6f) return false; // Parallel
        float t = dot(Q - r.origin(), normal) / denom;
        if (t < t_min || t > t_max) return false;
        point3 p = r.at(t);
        vec3 d = p - Q;
        float u_proj = dot(d, u) / (ulen * ulen);
        float v_proj = dot(d, v) / (vlen * vlen);
        if (u_proj < 0 || u_proj > 1 || v_proj < 0 || v_proj > 1) return false;
        rec.t = t;
        rec.p = p;
        rec.normal = normal;
        rec.mat_ptr = mat_ptr;
        rec.set_face_normal(r, normal);
        return true;
    }
}; 