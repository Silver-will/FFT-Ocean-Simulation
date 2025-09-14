# CMakeList.txt : CMake project for Setting up executable source, include source and define
# project specific logic here.
#

file(GLOB_RECURSE SRC_FILES
    include/*.h 
    include/*.cpp
    src/*.cpp)

# Add source to this project's executable.
add_executable (${PROJECT_NAME} "main.cpp" ${SRC_FILES})

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 23)
endif()

# TODO: Add tests and install targets if needed.
