cmake_minimum_required(VERSION 3.18)
project(GPURayTracer LANGUAGES CUDA CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set CUDA standard
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)

# Set build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Set CUDA architecture
set(CMAKE_CUDA_ARCHITECTURES 60 70 75 80 86)

# Find CUDA using modern CMake (helps CLion's indexer)
find_package(CUDAToolkit REQUIRED)

# Create common headers interface library
add_library(common_headers INTERFACE)
target_include_directories(common_headers INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/include/core
        ${CMAKE_CURRENT_SOURCE_DIR}/include/geometry
        ${CMAKE_CURRENT_SOURCE_DIR}/include/materials
        ${CMAKE_CURRENT_SOURCE_DIR}/include/scene
        ${CMAKE_CURRENT_SOURCE_DIR}/include/image
        ${CMAKE_CURRENT_SOURCE_DIR}/include/third_party
        ${CMAKE_CURRENT_SOURCE_DIR}/include/third_party/tinyxml2
        ${CUDAToolkit_INCLUDE_DIRS}
)

# Source files
set(SOURCES
    src/main.cpp
    src/renderer.cpp
    src/scene_controller.cpp
    src/image_writer.cpp
    src/texture_loader.cpp
    src/cuda_utils.cpp
    src/xml_scene_parser.cpp
    src/tinyxml2.cpp
    src/gpu_monitor.cpp
    src/film.cpp
    src/bvh.cpp
)

# CUDA source files
set(CUDA_SOURCES
    src/gpu_raytracer.cu
    src/bvh_device.cu
)

# Create executable
add_executable(gpu_raytracer ${SOURCES} ${CUDA_SOURCES})

# Link common headers
target_link_libraries(gpu_raytracer PRIVATE common_headers)

# Set CUDA-specific properties
set_target_properties(gpu_raytracer PROPERTIES
        CUDA_SEPARABLE_COMPILATION ON
        CUDA_RESOLVE_DEVICE_SYMBOLS ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Set CUDA flags based on build type
target_compile_options(gpu_raytracer PRIVATE
        $<$<COMPILE_LANGUAGE:CUDA>:$<$<CONFIG:Debug>:-g -G -O0>>
        $<$<COMPILE_LANGUAGE:CUDA>:$<$<CONFIG:Release>:-O3>>
        $<$<COMPILE_LANGUAGE:CXX>:$<$<CONFIG:Debug>:/Od /RTC1>>
        $<$<COMPILE_LANGUAGE:CXX>:$<$<CONFIG:Release>:/O2>>
)

# Add compile definitions
target_compile_definitions(gpu_raytracer PRIVATE
        SCENES_PATH="${CMAKE_CURRENT_SOURCE_DIR}/scenes"
)

# Link CUDA runtime using modern CMake
target_link_libraries(gpu_raytracer PRIVATE
        CUDA::cudart
        CUDA::cuda_driver
)
