#!/bin/bash

# Default to Release if not specified
BUILD_TYPE=${1:-Release}

# Validate build type
if [ "$BUILD_TYPE" != "Debug" ] && [ "$BUILD_TYPE" != "Release" ]; then
    echo "Invalid build type: $BUILD_TYPE"
    echo "Usage: ./build.sh [Debug|Release]"
    echo "Default: Release"
    exit 1
fi

echo "Building GPU Ray Tracer in $BUILD_TYPE mode..."

# Create build directory if it doesn't exist
mkdir -p build
cd build

echo "Configuring with CMake..."
cmake .. -DCMAKE_BUILD_TYPE=$BUILD_TYPE

if [ $? -ne 0 ]; then
    echo "CMake configuration failed!"
    exit 1
fi

echo "Building project..."
make -j$(nproc)

if [ $? -ne 0 ]; then
    echo "Build failed!"
    exit 1
fi

echo "Build successful!"
echo "Executable created at: build/bin/gpu_raytracer"
cd ..

echo "To run the ray tracer:"
echo "cd build/bin && ./gpu_raytracer" 