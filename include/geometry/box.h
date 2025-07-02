#pragma once
#include "hittable.h"
#include "vec3.h"
#include "material.h"

class box : public hittable {
public:
    __host__ __device__ box() {}
    __host__ __device__ box(const point3& p0, const point3& p1, material* ptr);

    __host__ __device__ virtual bool hit(const ray& r, float t_min, float t_max, 
                                        hit_record& rec) const override;

    __host__ __device__ bool hit(const ray& r, float t_min, float t_max, 
                                hit_record& rec, int face) const;

public:
    point3 box_min;
    point3 box_max;
    material* mat_ptr;
};

__host__ __device__ box::box(const point3& p0, const point3& p1, material* ptr) {
    box_min = p0;
    box_max = p1;
    mat_ptr = ptr;
}

__host__ __device__ bool box::hit(const ray& r, float t_min, float t_max, hit_record& rec) const {
    float t0 = (box_min.x() - r.origin().x()) / r.direction().x();
    float t1 = (box_max.x() - r.origin().x()) / r.direction().x();
    if (t0 > t1) {
        float temp = t0;
        t0 = t1;
        t1 = temp;
    }

    float tymin = (box_min.y() - r.origin().y()) / r.direction().y();
    float tymax = (box_max.y() - r.origin().y()) / r.direction().y();
    if (tymin > tymax) {
        float temp = tymin;
        tymin = tymax;
        tymax = temp;
    }

    if ((t0 > tymax) || (tymin > t1))
        return false;

    if (tymin > t0)
        t0 = tymin;

    float tzmin = (box_min.z() - r.origin().z()) / r.direction().z();
    float tzmax = (box_max.z() - r.origin().z()) / r.direction().z();
    if (tzmin > tzmax) {
        float temp = tzmin;
        tzmin = tzmax;
        tzmax = temp;
    }

    if ((t0 > tzmax) || (tzmin > t1))
        return false;

    if (tzmin > t0)
        t0 = tzmin;

    if (t0 < t_max && t0 > t_min) {
        rec.t = t0;
        rec.p = r.at(rec.t);
        rec.mat_ptr = mat_ptr;
        
        // Calculate normal based on which face was hit
        vec3 outward_normal;
        if (t0 == (box_min.x() - r.origin().x()) / r.direction().x())
            outward_normal = vec3(-1, 0, 0);
        else if (t0 == (box_max.x() - r.origin().x()) / r.direction().x())
            outward_normal = vec3(1, 0, 0);
        else if (t0 == (box_min.y() - r.origin().y()) / r.direction().y())
            outward_normal = vec3(0, -1, 0);
        else if (t0 == (box_max.y() - r.origin().y()) / r.direction().y())
            outward_normal = vec3(0, 1, 0);
        else if (t0 == (box_min.z() - r.origin().z()) / r.direction().z())
            outward_normal = vec3(0, 0, -1);
        else
            outward_normal = vec3(0, 0, 1);
            
        rec.set_face_normal(r, outward_normal);
        return true;
    }
    return false;
} 