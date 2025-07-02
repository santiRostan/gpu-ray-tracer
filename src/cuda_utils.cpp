#include "cuda_utils.h"
#include <iostream>
#include <stdexcept>

void initialize_cuda() {
    int deviceCount;
    cudaError_t err = cudaGetDeviceCount(&deviceCount);
    if (err != cudaSuccess) {
        throw std::runtime_error(std::string("CUDA not available: ") + cudaGetErrorString(err));
    }
    if (deviceCount == 0) {
        throw std::runtime_error("No CUDA devices found");
    }
    std::cout << "Found " << deviceCount << " CUDA device(s)" << std::endl;

    err = cudaSetDevice(0);
    if (err != cudaSuccess) {
        throw std::runtime_error(std::string("CUDA device initialization error: ") + cudaGetErrorString(err));
    }
    cudaDeviceProp prop;
    err = cudaGetDeviceProperties(&prop, 0);
    if (err != cudaSuccess) {
        throw std::runtime_error(std::string("CUDA device properties error: ") + cudaGetErrorString(err));
    }
    std::cout << "Using GPU: " << prop.name << std::endl;
} 