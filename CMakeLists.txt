cmake_minimum_required(VERSION 3.16)
project(Raylib3DProject CXX)

# Use C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Tell CMake to download raylib from GitHub automatically
include(FetchContent)
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG        master # Downloads the latest version
)

# 2. Disable building raylib's 100+ example files to save compilation time
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

# 3. Make raylib available to your project
FetchContent_MakeAvailable(raylib)

# 4. Define your executable name and all source files
add_executable(my_program 
    main.cpp
    Object.cpp
    Simulation.cpp
)

# 5. Link raylib to your program (handles all the underlying graphics drivers)
target_link_libraries(my_program PRIVATE raylib)