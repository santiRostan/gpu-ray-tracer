#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Print current GPU status
void print_gpu_status();

// Start real-time GPU monitoring (runs in a loop)
void start_gpu_monitoring();

// Performance monitoring functions
void start_performance_timer();
double get_performance_timer();
void print_performance_stats(double kernel_time, size_t total_rays, int width, int height, int samples);

#ifdef __cplusplus
}
#endif 