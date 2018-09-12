cmake_minimum_required(VERSION 3.4.1)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fexceptions -Werror -DVK_USE_PLATFORM_ANDROID_KHR")

add_subdirectory(shaderc)

#add_definitions(-DGLM_ENABLE_EXPERIMENTAL)
#add_definitions(-DGLM_LANG_STL11_FORCED)

#vk-drawcube
add_library(vk-drawcube SHARED
            android_native_app_glue.c
            vulkan_wrapper/vulkan_wrapper.cpp
            vkutils/util_init.cpp
            vkutils/util.cpp
            drawcube.cpp
            )

target_include_directories(vk-drawcube PRIVATE
                          ${CMAKE_CURRENT_SOURCE_DIR}
                          ${CMAKE_CURRENT_SOURCE_DIR}/data
                          ${CMAKE_CURRENT_SOURCE_DIR}/stb
                          ${CMAKE_CURRENT_SOURCE_DIR}/vkutils
                          ${CMAKE_CURRENT_SOURCE_DIR}/vulkan_wrapper
                          ${CMAKE_CURRENT_SOURCE_DIR}/tinyobjloader
                          ${CMAKE_CURRENT_SOURCE_DIR}/shaderc
                          ${CMAKE_CURRENT_SOURCE_DIR}/shaderc/libshaderc/include
                          )

target_link_libraries(vk-drawcube
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


