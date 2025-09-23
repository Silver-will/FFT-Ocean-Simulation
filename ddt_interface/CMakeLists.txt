cmake_minimum_required(VERSION 3.15)
project(main_app LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Eigen3
find_package(Eigen3 REQUIRED)

# Add executable
add_executable(main_app 
    main_app.cpp
    sim_utils.cpp
)

# Link Eigen
target_link_libraries(main_app PRIVATE Eigen3::Eigen)