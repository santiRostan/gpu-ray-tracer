#include "geometry/bvh.h"
#include <iostream>
#include <cuda_runtime.h>
#include <algorithm>
#include <limits>

// Compute bounding box for a range of objects
aabb bvh_tree::compute_bounds(int start, int end) const {
    point3 min_bound(std::numeric_limits<float>::max(), 
                     std::numeric_limits<float>::max(), 
                     std::numeric_limits<float>::max());
    point3 max_bound(-std::numeric_limits<float>::max(), 
                     -std::numeric_limits<float>::max(), 
                     -std::numeric_limits<float>::max());
    
    for (int i = start; i < end; ++i) {
        const hittable& obj = objects[i];
        
        // Compute bounding box for each object type
        aabb obj_bounds;
        switch (obj.type) {
            case SPHERE: {
                point3 center = obj.sphere.center;
                float radius = obj.sphere.radius;
                obj_bounds = aabb(center - vec3(radius, radius, radius),
                                 center + vec3(radius, radius, radius));
                break;
            }
            case QUAD: {
                // For quads, compute bounds from corner and edges
                point3 corner = obj.quad.Q;
                vec3 u_edge = obj.quad.u;
                vec3 v_edge = obj.quad.v;
                
                point3 p1 = corner;
                point3 p2 = corner + u_edge;
                point3 p3 = corner + v_edge;
                point3 p4 = corner + u_edge + v_edge;
                
                obj_bounds = aabb(min_per_component(min_per_component(p1, p2), 
                                                   min_per_component(p3, p4)),
                                 max_per_component(max_per_component(p1, p2), 
                                                   max_per_component(p3, p4)));
                break;
            }
            case BOX: {
                obj_bounds = aabb(obj.box.box_min, obj.box.box_max);
                break;
            }
        }
        
        min_bound = min_per_component(min_bound, obj_bounds.min());
        max_bound = max_per_component(max_bound, obj_bounds.max());
    }
    
    return {min_bound, max_bound};
}

// Split objects along the longest axis using median split
int bvh_tree::partition_objects(int start, int end, float split_value, int axis) {
    int i = start;
    int j = end - 1;
    
    while (i <= j) {
        // Find center of object i
        float center_i = 0;
        const hittable& obj_i = objects[i];
        switch (obj_i.type) {
            case SPHERE: center_i = obj_i.sphere.center[axis]; break;
            case QUAD: center_i = obj_i.quad.Q[axis] + 0.5f * (obj_i.quad.u[axis] + obj_i.quad.v[axis]); break;
            case BOX: center_i = 0.5f * (obj_i.box.box_min[axis] + obj_i.box.box_max[axis]); break;
        }
        
        if (center_i < split_value) {
            i++;
        } else {
            // Swap objects
            std::swap(objects[i], objects[j]);
            j--;
        }
    }
    
    return i;
}

// Build BVH recursively
int bvh_tree::build_bvh(int start, int end) {
    const int node_index = static_cast<int>(nodes.size());
    nodes.emplace_back();

    const int n_objects = end - start;
    
    if (n_objects == 1) {
        // Leaf node
        aabb bounds = compute_bounds(start, end);
        nodes[node_index].box_min = bounds.min();
        nodes[node_index].box_max = bounds.max();
        nodes[node_index].left = -1;  // Not used for leaf nodes
        nodes[node_index].right = -1; // Not used for leaf nodes
        nodes[node_index].object_index = start;
        nodes[node_index].is_leaf = true;
        return node_index;
    }
    
    if (n_objects == 2) {
        // Special case: create two leaf nodes
        // Left child
        const int left_child = static_cast<int>(nodes.size());
        nodes.emplace_back();
        const aabb left_bounds = compute_bounds(start, start + 1);
        nodes[left_child].box_min = left_bounds.min();
        nodes[left_child].box_max = left_bounds.max();
        nodes[left_child].left = -1;
        nodes[left_child].right = -1;
        nodes[left_child].object_index = start;
        nodes[left_child].is_leaf = true;
        
        // Right child
        const int right_child = static_cast<int>(nodes.size());
        nodes.emplace_back();
        const aabb right_bounds = compute_bounds(start + 1, end);
        nodes[right_child].box_min = right_bounds.min();
        nodes[right_child].box_max = right_bounds.max();
        nodes[right_child].left = -1;
        nodes[right_child].right = -1;
        nodes[right_child].object_index = start + 1;
        nodes[right_child].is_leaf = true;
        
        // Set up this node
        const aabb bounds = compute_bounds(start, end);
        nodes[node_index].box_min = bounds.min();
        nodes[node_index].box_max = bounds.max();
        nodes[node_index].left = left_child;
        nodes[node_index].right = right_child;
        nodes[node_index].is_leaf = false;

        return node_index;
    }
    
    // Compute bounding box for all objects in this range
    const aabb bounds = compute_bounds(start, end);
    nodes[node_index].box_min = bounds.min();
    nodes[node_index].box_max = bounds.max();
    
    // Simple split: just divide in half
    int mid = start + (end - start) / 2;
    
    // Build children
    nodes[node_index].left = build_bvh(start, mid);
    nodes[node_index].right = build_bvh(mid, end);
    nodes[node_index].is_leaf = false;
    
    return node_index;
}

// Build BVH from a list of objects
void bvh_tree::build(const std::vector<hittable>& scene_objects) {
    objects = scene_objects;
    nodes.clear();
    
    if (objects.empty()) return;
    
    // Build the BVH tree
    build_bvh(0, static_cast<int>(objects.size()));
}

// CPU-side BVH construction functions
void build_bvh_from_objects(const std::vector<hittable>& scene_objects,
                           bvh_node*& d_nodes, hittable*& d_objects,
                           int& num_nodes, int& num_objects) {
    // Build BVH tree on CPU
    bvh_tree tree;
    tree.build(scene_objects);
    
    num_nodes = tree.get_node_count();
    num_objects = tree.get_object_count();
    
    if (num_nodes == 0 || num_objects == 0) {
        d_nodes = nullptr;
        d_objects = nullptr;
        return;
    }
    
    // Allocate GPU memory for nodes
    cudaMalloc(&d_nodes, num_nodes * sizeof(bvh_node));
    
    // Allocate GPU memory for objects
    cudaMalloc(&d_objects, num_objects * sizeof(hittable));
    
    // Copy nodes to GPU
    const std::vector<bvh_node>& nodes = tree.get_nodes();
    
    cudaMemcpy(d_nodes, nodes.data(), num_nodes * sizeof(bvh_node), cudaMemcpyHostToDevice);
    
    // Copy objects to GPU
    const std::vector<hittable>& objects = tree.get_objects();
    cudaMemcpy(d_objects, objects.data(), num_objects * sizeof(hittable), cudaMemcpyHostToDevice);
    
    std::cout << "BVH built with " << num_nodes << " nodes and " << num_objects << " objects" << std::endl;
}

// Cleanup function for BVH memory
void cleanup_bvh(bvh_node*& d_nodes, hittable*& d_objects) {
    if (d_nodes) {
        cudaFree(d_nodes);
        d_nodes = nullptr;
    }
    if (d_objects) {
        cudaFree(d_objects);
        d_objects = nullptr;
    }
} 