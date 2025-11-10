#pragma once
#include "hittable.h"
#include "ray.h"
#include "hit_record.h"
#include "core/vec3.h"
#include <vector>
#include <algorithm>
#include <cuda_runtime.h>

// BVH node structure for GPU
struct bvh_node {
    // Bounding box for this node
    point3 box_min;
    point3 box_max;
    
    // Left and right child indices (for internal nodes)
    // If left == right, this is a leaf node containing object at index 'left'
    int left;
    int right;
    
    // Object index (only used for leaf nodes)
    int object_index;
    
    // Whether this is a leaf node
    bool is_leaf;
};

// AABB (Axis-Aligned Bounding Box) structure
struct aabb {
    point3 minimum;
    point3 maximum;
    
    __host__ __device__ aabb() {}
    __host__ __device__ aabb(const point3& a, const point3& b) : minimum(a), maximum(b) {}
    
    __host__ __device__ point3 min() const { return minimum; }
    __host__ __device__ point3 max() const { return maximum; }
    
    __host__ __device__ bool hit(const ray& r, float t_min, float t_max) const {
        for (int a = 0; a < 3; a++) {
            float invD = 1.0f / r.direction()[a];
            float t0 = (min()[a] - r.origin()[a]) * invD;
            float t1 = (max()[a] - r.origin()[a]) * invD;
            if (invD < 0.0f) {
                float temp = t0;
                t0 = t1;
                t1 = temp;
            }
            t_min = t0 > t_min ? t0 : t_min;
            t_max = t1 < t_max ? t1 : t_max;
            if (t_max <= t_min) return false;
        }
        return true;
    }
    
    __host__ __device__ float area() const {
        vec3 d = maximum - minimum;
        return d.x() * d.y() + d.y() * d.z() + d.z() * d.x();
    }
};

// BVH tree class for CPU-side construction
class bvh_tree {
private:
    std::vector<bvh_node> nodes;
    std::vector<hittable> objects;
    
    // Build BVH recursively
    int build_bvh(int start, int end);
    
    // Compute bounding box for a range of objects
    aabb compute_bounds(int start, int end) const;
    
    // Split objects along the longest axis
    int partition_objects(int start, int end, float split_value, int axis);
    
public:
    bvh_tree() {}
    
    // Build BVH from a list of objects
    void build(const std::vector<hittable>& scene_objects);
    
    // Get the BVH nodes for GPU transfer
    const std::vector<bvh_node>& get_nodes() const { return nodes; }
    
    // Get the objects for GPU transfer
    const std::vector<hittable>& get_objects() const { return objects; }
    
    // Get the number of nodes
    int get_node_count() const { return static_cast<int>(nodes.size()); }
    
    // Get the number of objects
    int get_object_count() const { return static_cast<int>(objects.size()); }
};

// GPU-side BVH traversal function
__device__ bool bvh_hit(bvh_node* nodes, const hittable* objects, const ray& r, 
                        float t_min, float t_max, hit_record& rec);

// CPU-side BVH construction functions
void build_bvh_from_objects(const std::vector<hittable>& scene_objects,
                           bvh_node*& d_nodes, hittable*& d_objects,
                           int& num_nodes, int& num_objects);

// Cleanup function for BVH memory
void cleanup_bvh(bvh_node*& d_nodes, hittable*& d_objects); 