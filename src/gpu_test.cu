#include <cuda_runtime.h>
#include <iostream>
#include <chrono>

__global__ void gpu_stress_test(float* data, int size, int iterations) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= size) return;
    
    float value = data[idx];
    for (int i = 0; i < iterations; i++) {
        value = sin(value) + cos(value) + sqrt(fabs(value));
        value = value * 1.0001f; // Prevent optimization
    }
    data[idx] = value;
}

int main() {
    std::cout << "Starting GPU stress test..." << std::endl;
    
    // Get GPU info
    cudaDeviceProp prop;
    cudaGetDeviceProperties(&prop, 0);
    std::cout << "GPU: " << prop.name << std::endl;
    std::cout << "Multiprocessors: " << prop.multiProcessorCount << std::endl;
    
    // Allocate memory
    const int size = 10000000; // 10M elements
    float* h_data = new float[size];
    float* d_data;
    
    // Initialize data
    for (int i = 0; i < size; i++) {
        h_data[i] = (float)i;
    }
    
    cudaMalloc(&d_data, size * sizeof(float));
    cudaMemcpy(d_data, h_data, size * sizeof(float), cudaMemcpyHostToDevice);
    
    // Launch kernel with high workload
    dim3 block_size(256);
    dim3 grid_size((size + block_size.x - 1) / block_size.x);
    
    std::cout << "Running GPU stress test for 10 seconds..." << std::endl;
    std::cout << "Check your GPU monitoring tools now!" << std::endl;
    
    auto start = std::chrono::high_resolution_clock::now();
    int iterations = 1000;
    
    while (std::chrono::duration_cast<std::chrono::seconds>(
           std::chrono::high_resolution_clock::now() - start).count() < 10) {
        
        gpu_stress_test<<<grid_size, block_size>>>(d_data, size, iterations);
        cudaDeviceSynchronize();
    }
    
    // Copy result back
    cudaMemcpy(h_data, d_data, size * sizeof(float), cudaMemcpyDeviceToHost);
    
    std::cout << "GPU stress test completed!" << std::endl;
    std::cout << "Final value: " << h_data[0] << std::endl;
    
    // Cleanup
    cudaFree(d_data);
    delete[] h_data;
    
    return 0;
} 