cmake_minimum_required(VERSION 3.4.1)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fexceptions -Werror -DVK_USE_PLATFORM_ANDROID_KHR")

add_subdirectory(shaderc)

add_definitions(-DGLM_ENABLE_EXPERIMENTAL)
#add_definitions(-DGLM_LANG_STL11_FORCED)

#tutorial28
add_library(tutorial28 SHARED
            android_native_app_glue.c
            jniactivity.c
            vulkan_wrapper.cpp
            tutorial28/TutorialMain.cpp
            tutorial28/DepthBuffering.cpp
            )

target_include_directories(tutorial28 PRIVATE
                          ${CMAKE_CURRENT_SOURCE_DIR}
                          ${CMAKE_CURRENT_SOURCE_DIR}/data
                          ${CMAKE_CURRENT_SOURCE_DIR}/stb
                          ${CMAKE_CURRENT_SOURCE_DIR}/tinyobjloader
                          ${CMAKE_CURRENT_SOURCE_DIR}/shaderc/include)

target_link_libraries(tutorial28
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


#tutorial29
add_library(tutorial29 SHARED
            android_native_app_glue.c
            jniactivity.c
            vulkan_wrapper.cpp
            tutorial29/TutorialMain.cpp
            tutorial29/TriangleApplication.cpp
            )

target_include_directories(tutorial29 PRIVATE
                          ${CMAKE_CURRENT_SOURCE_DIR}
                          ${CMAKE_CURRENT_SOURCE_DIR}/data
                          ${CMAKE_CURRENT_SOURCE_DIR}/stb
                          ${CMAKE_CURRENT_SOURCE_DIR}/tinyobjloader
                          ${CMAKE_CURRENT_SOURCE_DIR}/shaderc/include)

target_link_libraries(tutorial29
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

#drawingt_riangle
add_library(drawingt_riangle SHARED
            android_native_app_glue.c
            jniactivity.c
            vulkan_wrapper.cpp
            drawingt_riangle/DrawingTriangleMain.cpp
            drawingt_riangle/DrawingTriangle.cpp
            )

target_include_directories(drawingt_riangle PRIVATE
                          ${CMAKE_CURRENT_SOURCE_DIR}
                          ${CMAKE_CURRENT_SOURCE_DIR}/data
                          ${CMAKE_CURRENT_SOURCE_DIR}/stb
                          ${CMAKE_CURRENT_SOURCE_DIR}/tinyobjloader
                          ${CMAKE_CURRENT_SOURCE_DIR}/shaderc/include)

target_link_libraries(drawingt_riangle
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


