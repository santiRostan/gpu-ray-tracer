#pragma once
#include "ray.h"
#include "hit_record.h"

class hittable {
public:
    virtual ~hittable() = default;

    __host__ __device__ virtual bool hit(const ray& r, float t_min, float t_max, 
                                        hit_record& rec) const = 0;
}; 