cmake_minimum_required(VERSION 3.4.1)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fexceptions -Werror -DVK_USE_PLATFORM_ANDROID_KHR")

add_subdirectory(shaderc)

add_definitions(-DGLM_ENABLE_EXPERIMENTAL)
#add_definitions(-DGLM_LANG_STL11_FORCED)


#tutorial03
add_library(tutorial03 SHARED
            android_native_app_glue.c
            jniactivity.c
            vulkan_wrapper.cpp
            tutorial03/TutorialMain.cpp
            tutorial03/TriangleApplication.cpp
            )

target_include_directories(tutorial03 PRIVATE
                          ${CMAKE_CURRENT_SOURCE_DIR}
                          ${CMAKE_CURRENT_SOURCE_DIR}/data
                          ${CMAKE_CURRENT_SOURCE_DIR}/stb
                          ${CMAKE_CURRENT_SOURCE_DIR}/tinyobjloader
                          ${CMAKE_CURRENT_SOURCE_DIR}/shaderc/include)

target_link_libraries(tutorial03
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


#vktutorial
add_library(vktutorial SHARED
            android_native_app_glue.c
            jniactivity.c
            Tutorial.cpp
            TutorialVK.cpp
            vulkan_wrapper.cpp)

target_include_directories(vktutorial PRIVATE
                          ${CMAKE_CURRENT_SOURCE_DIR}
                          ${CMAKE_CURRENT_SOURCE_DIR}/data
                          ${CMAKE_CURRENT_SOURCE_DIR}/stb
                          ${CMAKE_CURRENT_SOURCE_DIR}/tinyobjloader
                          ${CMAKE_CURRENT_SOURCE_DIR}/shaderc/include)

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
