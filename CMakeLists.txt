cmake_minimum_required(VERSION 3.18)
project(GPURayTracer CUDA CXX)

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

# Set CUDA flags based on build type
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CUDA_FLAGS_DEBUG "-g -G -O0")
    set(CMAKE_CXX_FLAGS_DEBUG "/Od /RTC1")
else()
    set(CMAKE_CUDA_FLAGS_RELEASE "-O3")
    set(CMAKE_CXX_FLAGS_RELEASE "/O2")
endif()

# Set CUDA architecture
set(CMAKE_CUDA_ARCHITECTURES 60 70 75 80 86)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/core
    ${CMAKE_SOURCE_DIR}/include/geometry
    ${CMAKE_SOURCE_DIR}/include/materials
    ${CMAKE_SOURCE_DIR}/include/scene
    ${CMAKE_SOURCE_DIR}/include/image
    ${CMAKE_SOURCE_DIR}/include/third_party
    ${CMAKE_SOURCE_DIR}/include/third_party/tinyxml2
)

# Source files
set(SOURCES
    src/main.cpp
    src/renderer.cpp
    src/scene_controller.cpp
    src/image_writer.cpp
    src/cuda_utils.cpp
    src/xml_scene_parser.cpp
    src/tinyxml2.cpp
)

# CUDA source files
set(CUDA_SOURCES
    src/gpu_raytracer.cu
)

# Create executable
add_executable(gpu_raytracer ${SOURCES} ${CUDA_SOURCES})

# Set CUDA separable compilation
set_target_properties(gpu_raytracer PROPERTIES
    CUDA_SEPARABLE_COMPILATION ON
)

# Set output directory
set_target_properties(gpu_raytracer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

target_compile_definitions(gpu_raytracer PRIVATE SCENES_PATH="${CMAKE_SOURCE_DIR}/scenes")
