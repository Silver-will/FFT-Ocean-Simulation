# CMakeList.txt : CMake project for Setting up executable source, include source and define
# project specific logic here.
#

file(GLOB_RECURSE SRC_FILES
    include/*.h 
    include/*.cpp
    src/*.cpp)

# Add source to this project's executable.
add_executable (${PROJECT_NAME} "main.cpp" ${SRC_FILES})


# TODO: Add tests and install targets if needed.
