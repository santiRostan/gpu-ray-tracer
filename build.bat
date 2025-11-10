@echo off
setlocal

REM Default to Release if not specified
set BUILD_TYPE=%1
if "%BUILD_TYPE%"=="" set BUILD_TYPE=Release

REM Validate build type
if not "%BUILD_TYPE%"=="Debug" if not "%BUILD_TYPE%"=="Release" (
    echo Invalid build type: %BUILD_TYPE%
    echo Usage: build.bat [Debug^|Release]
    echo Default: Release
    exit /b 1
)

echo Building GPU Ray Tracer in %BUILD_TYPE% mode...

if not exist build mkdir build
cd build

echo Configuring with CMake...
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=%BUILD_TYPE%

if %ERRORLEVEL% NEQ 0 (
    echo CMake configuration failed!
    exit /b 1
)

echo Building project...
cmake --build . --config %BUILD_TYPE%

if %ERRORLEVEL% NEQ 0 (
    echo Build failed!
    exit /b 1
)

echo Build successful!
echo Executable created at: build\bin\%BUILD_TYPE%\gpu_raytracer.exe
cd .. 