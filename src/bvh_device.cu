#include "geometry/bvh.h"
#include "geometry/hittable.h"
#include "geometry/ray.h"
#include "geometry/hit_record.h"
#include "core/vec3.h"
#include <cuda_runtime.h>

// GPU-side BVH traversal function
__device__ bool bvh_hit(bvh_node* nodes, const hittable* objects, const ray& r,
                        float t_min, float t_max, hit_record& rec) {
    // Stack for iterative traversal (avoid recursion on GPU)
    int stack[64];  // BVH depth is typically < 64
    int stack_ptr = 0;
    
    // Start with root node (index 0)
    stack[stack_ptr++] = 0;
    
    bool hit_anything = false;
    float closest_so_far = t_max;
    
    int max_iterations = 1000;  // Safety limit
    int iterations = 0;
    
    while (stack_ptr > 0 && iterations < max_iterations) {
        iterations++;
        int node_idx = stack[--stack_ptr];
        bvh_node& node = nodes[node_idx];
        
        // Simple AABB intersection test with division by zero protection
        float t0, t1;
        
        // X-axis
        if (fabs(r.direction().x()) < 1e-6f) {
            if (r.origin().x() < node.box_min.x() || r.origin().x() > node.box_max.x()) continue;
            t0 = -1e6f;
            t1 = 1e6f;
        } else {
            t0 = (node.box_min.x() - r.origin().x()) / r.direction().x();
            t1 = (node.box_max.x() - r.origin().x()) / r.direction().x();
            if (t0 > t1) { float temp = t0; t0 = t1; t1 = temp; }
        }
        
        // Y-axis
        float tymin, tymax;
        if (fabs(r.direction().y()) < 1e-6f) {
            if (r.origin().y() < node.box_min.y() || r.origin().y() > node.box_max.y()) continue;
            tymin = -1e6f;
            tymax = 1e6f;
        } else {
            tymin = (node.box_min.y() - r.origin().y()) / r.direction().y();
            tymax = (node.box_max.y() - r.origin().y()) / r.direction().y();
            if (tymin > tymax) { float temp = tymin; tymin = tymax; tymax = temp; }
        }
        if ((t0 > tymax) || (tymin > t1)) continue;
        if (tymin > t0) t0 = tymin;
        if (tymax < t1) t1 = tymax;
        
        // Z-axis
        float tzmin, tzmax;
        if (fabs(r.direction().z()) < 1e-6f) {
            if (r.origin().z() < node.box_min.z() || r.origin().z() > node.box_max.z()) continue;
            tzmin = -1e6f;
            tzmax = 1e6f;
        } else {
            tzmin = (node.box_min.z() - r.origin().z()) / r.direction().z();
            tzmax = (node.box_max.z() - r.origin().z()) / r.direction().z();
            if (tzmin > tzmax) { float temp = tzmin; tzmin = tzmax; tzmax = temp; }
        }
        if ((t0 > tzmax) || (tzmin > t1)) continue;
        if (tzmin > t0) t0 = tzmin;
        
        if (t0 > closest_so_far) continue;
        
        if (node.is_leaf) {
            // Leaf node - test against the object
            hit_record temp_rec;
            if (hit_device(&objects[node.object_index], r, t_min, closest_so_far, temp_rec)) {
                hit_anything = true;
                closest_so_far = temp_rec.t;
                rec = temp_rec;
            }
        } else {
            // Internal node - add children to stack
            if (stack_ptr < 62) {  // Leave room for 2 more nodes
                stack[stack_ptr++] = node.left;
                stack[stack_ptr++] = node.right;
            }
        }
    }
    
    return hit_anything;
} 