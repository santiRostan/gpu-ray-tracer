#include <iostream>
#include <cuda_runtime.h>
#include <thread>
#include <chrono>

class GPUMonitor {
private:
    std::chrono::high_resolution_clock::time_point start_time;
    
public:
    static void printGPUInfo() {
        cudaDeviceProp prop{};
        cudaError_t err = cudaGetDeviceProperties(&prop, 0);
        if (err != cudaSuccess) {
            std::cout << "GPU error: " << cudaGetErrorString(err) << std::endl;
            return;
        }
        
        size_t free_mem, total_mem;
        err = cudaMemGetInfo(&free_mem, &total_mem);
        if (err == cudaSuccess) {
            size_t used_mem = total_mem - free_mem;
            unsigned long utilization = (used_mem * 100) / total_mem;
            std::cout << prop.name << " | " << prop.multiProcessorCount << " SMs | " 
                      << utilization << "% memory (" << used_mem / (1024*1024) << "MB)" << std::endl;
        } else {
            std::cout << prop.name << " | " << prop.multiProcessorCount << " SMs" << std::endl;
        }
    }

    [[noreturn]] static void monitorGPU() {
        std::cout << "Monitoring GPU memory usage (press Ctrl+C to stop)..." << std::endl;
        
        while (true) {
            size_t free_mem, total_mem;
            cudaError_t err = cudaMemGetInfo(&free_mem, &total_mem);
            
            if (err == cudaSuccess) {
                size_t used_mem = total_mem - free_mem;
                unsigned long utilization = (used_mem * 100) / total_mem;
                
                std::cout << "\rMemory: " << utilization << "% | Used: " << used_mem / (1024*1024) 
                          << "MB / " << total_mem / (1024*1024) << "MB" << std::flush;
            } else {
                std::cout << "\rError getting memory info: " << cudaGetErrorString(err) << std::flush;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }
    
    void startTimer() {
        start_time = std::chrono::high_resolution_clock::now();
    }
    
    double getTimer() const {
        const auto end_time = std::chrono::high_resolution_clock::now();
        const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        return duration.count() / 1000.0; // Return in seconds (milliseconds to seconds)
    }

    static void printPerformanceStats(double kernel_time, size_t total_rays, int width, int height, int samples) {
        double rays_per_second = total_rays / kernel_time;
        
        std::cout << "=== Performance Statistics ===" << std::endl;
        std::cout << "Kernel time: " << kernel_time << " seconds" << std::endl;
        std::cout << "Total rays: " << total_rays << std::endl;
        std::cout << "Performance: " << rays_per_second / 1000000.0 << " M rays/sec" << std::endl;
        std::cout << "============================" << std::endl;
    }
};

// Global monitor instance
static GPUMonitor g_monitor;

// Function to be called from other parts of the code
extern "C" void print_gpu_status() {
    g_monitor.printGPUInfo();
}

extern "C" void start_gpu_monitoring() {
    g_monitor.monitorGPU();
}

extern "C" void start_performance_timer() {
    g_monitor.startTimer();
}

extern "C" double get_performance_timer() {
    return g_monitor.getTimer();
}

extern "C" void print_performance_stats(double kernel_time, size_t total_rays, int width, int height, int samples) {
    g_monitor.printPerformanceStats(kernel_time, total_rays, width, height, samples);
} 