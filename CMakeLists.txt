# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.15)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()


project ("FFT")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Vulkan SDK
find_package(Vulkan REQUIRED)    

#set(KTX_FEATURE_LOADTEST_APPS OFF CACHE BOOL "" FORCE)
#set(KTX_FEATURE_TOOLS OFF CACHE BOOL "" FORCE)
#set(KTX_FEATURE_ASTC=OFF)
#set(KTX_FEATURE_TESTS OFF CACHE BOOL "" FORCE)
#set(KTX_FEATURE_STATIC_LIBRARY OFF CACHE BOOL "" FORCE)
#set(KTX_FEATURE_TOOLS OFF CACHE BOOL "" FORCE)


# Include sub-projects.
add_subdirectory ("FFT-Sim")
add_subdirectory ("third_party/glm")
add_subdirectory ("third_party/glfw")
add_subdirectory ("third_party/imgui")
add_subdirectory ("third_party/stb")
add_subdirectory ("third_party/vma")
add_subdirectory ("third_party/vk-bootstrap")

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    FFT-Sim/
    FFT-Sim/include/
    FFT-Sim/src/
    third_party/vma/include
    third_party/vma
    third_party/vma/src
    third_party/glfw/include
    third_party/glm
    third_party/imgui/include
    third_party/vk-bootstrap/src
    third_party/stb
    ${Vulkan_INCLUDE_DIRS}
)

set(VULKAN_LIBRARY "$ENV{VK_SDK_PATH}/Lib/vulkan-1.lib")
target_compile_definitions(${PROJECT_NAME} PRIVATE
                            ENGINE_ASSET_PATH="${CMAKE_CURRENT_LIST_DIR}/assets")

target_link_libraries(${PROJECT_NAME}
    stb
    imgui
    glfw
    ${VULKAN_LIBRARY}
    vk-bootstrap::vk-bootstrap
    GPUOpen::VulkanMemoryAllocator
)

#add_custom_target(assets COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_LIST_DIR}/assets.cmake)
#add_dependencies(${PROJECT_NAME} assets)