cmake_minimum_required(VERSION 3.4.1)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fexceptions -Werror -DVK_USE_PLATFORM_ANDROID_KHR")

add_subdirectory(shaderc)

add_definitions(-DGLM_ENABLE_EXPERIMENTAL)
#add_definitions(-DGLM_LANG_STL11_FORCED)

#vksample
add_library(vktutorial SHARED
            vulkan_wrapper.cpp
            vulkan_tutorial.cpp
            vulkan_tutorial_jni.cpp
            TriangleVK.cpp
            )

target_include_directories(vktutorial PRIVATE
                          ${CMAKE_CURRENT_SOURCE_DIR}
                          ${CMAKE_CURRENT_SOURCE_DIR}/data
                          ${CMAKE_CURRENT_SOURCE_DIR}/stb
                          ${CMAKE_CURRENT_SOURCE_DIR}/tinyobjloader
                          ${NDK_ROOT}/sysroot/usr/include)

target_link_libraries(vktutorial
                      android
                      log
                      shaderc
                      shaderc_util
                      glslang
                      SPIRV
                      SPIRV-Tools
                      SPIRV-Tools-opt
                      OSDependent
                      OGLCompiler)



