#pragma once
#include "ray.h"
#include "hit_record.h"
#include "material.h"
#include <cmath>

// Unified primitive type tag
enum hittable_type { SPHERE, QUAD, BOX };

// Sphere data
struct sphere_data {
    point3 center;
    float radius;
};

// Quad data
struct quad_data {
    point3 Q; // Corner point
    vec3 u, v; // Edge vectors
    vec3 normal; // Normal (unit vector)
};

// box_data represents an axis-aligned bounding box (AABB) in 3D space.
// The box is defined by its minimum and maximum corners (box_min and box_max),
// where box_min is the corner with the smallest x, y, z values, and box_max is the corner with the largest x, y, z values.
// This is a common and efficient representation for box primitives and acceleration structures in ray tracing.
struct box_data {
    point3 box_min;
    point3 box_max;
};

// We cannot use a union for the primitive data here because C++ unions require all members to be Plain Old Data (POD) types with trivial constructors and destructors.
// Our vec3 types have user-defined constructors and member functions, so they are not PODs.
// Attempting to use a union with such types results in compiler errors.
// We use a struct with all possible members (sphere, quad, box) and select the relevant one using the 'type' tag.
struct hittable {
    hittable_type type;
    sphere_data sphere;
    quad_data quad;
    box_data box;
    material* mat_ptr;
};

// Sphere intersection
__device__ inline bool sphere_hit(const sphere_data* s, const ray& r, float t_min, float t_max, hit_record& rec, material* mat_ptr) {
    vec3 oc = r.origin() - s->center;
    auto a = r.direction().length_squared();
    auto half_b = dot(oc, r.direction());
    auto c = oc.length_squared() - s->radius * s->radius;
    auto discriminant = half_b * half_b - a * c;
    if (discriminant < 0) return false;
    auto sqrtd = sqrt(discriminant);
    auto root = (-half_b - sqrtd) / a;
    if (root < t_min || t_max < root) {
        root = (-half_b + sqrtd) / a;
        if (root < t_min || t_max < root)
            return false;
    }
    rec.t = root;
    rec.p = r.at(rec.t);
    vec3 outward_normal = (rec.p - s->center) / s->radius;
    rec.set_face_normal(r, outward_normal);
    rec.mat_ptr = mat_ptr;
    return true;
}

// Quad intersection
__device__ inline bool quad_hit(const quad_data* q, const ray& r, float t_min, float t_max, hit_record& rec, material* mat_ptr) {
    float ulen = q->u.length();
    float vlen = q->v.length();
    float denom = dot(q->normal, r.direction());
    if (fabs(denom) < 1e-6f) return false;
    float t = dot(q->Q - r.origin(), q->normal) / denom;
    if (t < t_min || t > t_max) return false;
    point3 p = r.at(t);
    vec3 d = p - q->Q;
    float u_proj = dot(d, q->u) / (ulen * ulen);
    float v_proj = dot(d, q->v) / (vlen * vlen);
    if (u_proj < 0 || u_proj > 1 || v_proj < 0 || v_proj > 1) return false;
    rec.t = t;
    rec.p = p;
    rec.mat_ptr = mat_ptr;
    rec.set_face_normal(r, q->normal);
    return true;
}

// Box intersection (AABB)
__device__ inline bool box_hit(const box_data* b, const ray& r, float t_min, float t_max, hit_record& rec, material* mat_ptr) {
    float t0 = (b->box_min.x() - r.origin().x()) / r.direction().x();
    float t1 = (b->box_max.x() - r.origin().x()) / r.direction().x();
    if (t0 > t1) { float temp = t0; t0 = t1; t1 = temp; }
    float tymin = (b->box_min.y() - r.origin().y()) / r.direction().y();
    float tymax = (b->box_max.y() - r.origin().y()) / r.direction().y();
    if (tymin > tymax) { float temp = tymin; tymin = tymax; tymax = temp; }
    if ((t0 > tymax) || (tymin > t1)) return false;
    if (tymin > t0) t0 = tymin;
    float tzmin = (b->box_min.z() - r.origin().z()) / r.direction().z();
    float tzmax = (b->box_max.z() - r.origin().z()) / r.direction().z();
    if (tzmin > tzmax) { float temp = tzmin; tzmin = tzmax; tzmax = temp; }
    if ((t0 > tzmax) || (tzmin > t1)) return false;
    if (tzmin > t0) t0 = tzmin;
    if (t0 < t_max && t0 > t_min) {
        rec.t = t0;
        rec.p = r.at(rec.t);
        rec.mat_ptr = mat_ptr;
        // Calculate normal based on which face was hit
        vec3 outward_normal;
        point3 hit_point = r.at(t0);
        
        // Determine which face was hit by checking which component is closest to the box boundary
        float dx = fabs(hit_point.x() - b->box_min.x()) < fabs(hit_point.x() - b->box_max.x()) ? -1.0f : 1.0f;
        float dy = fabs(hit_point.y() - b->box_min.y()) < fabs(hit_point.y() - b->box_max.y()) ? -1.0f : 1.0f;
        float dz = fabs(hit_point.z() - b->box_min.z()) < fabs(hit_point.z() - b->box_max.z()) ? -1.0f : 1.0f;
        
        // Find the face with the smallest distance to the boundary
        float dist_x = fabs(hit_point.x() - (dx < 0 ? b->box_min.x() : b->box_max.x()));
        float dist_y = fabs(hit_point.y() - (dy < 0 ? b->box_min.y() : b->box_max.y()));
        float dist_z = fabs(hit_point.z() - (dz < 0 ? b->box_min.z() : b->box_max.z()));
        
        if (dist_x <= dist_y && dist_x <= dist_z) {
            outward_normal = vec3(dx, 0, 0);
        } else if (dist_y <= dist_z) {
            outward_normal = vec3(0, dy, 0);
        } else {
            outward_normal = vec3(0, 0, dz);
        }
        rec.set_face_normal(r, outward_normal);
        return true;
    }
    return false;
}

// Unified dispatcher
__device__ inline bool hit_device(const hittable* obj, const ray& r, float t_min, float t_max, hit_record& rec) {
    switch (obj->type) {
        case SPHERE: return sphere_hit(&obj->sphere, r, t_min, t_max, rec, obj->mat_ptr);
        case QUAD:   return quad_hit(&obj->quad, r, t_min, t_max, rec, obj->mat_ptr);
        case BOX:    return box_hit(&obj->box, r, t_min, t_max, rec, obj->mat_ptr);
    }
    return false;
}
