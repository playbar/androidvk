cmake_minimum_required(VERSION 3.4.1)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fexceptions -Werror -DVK_USE_PLATFORM_ANDROID_KHR")

add_subdirectory(shaderc)

add_definitions(-DGLM_ENABLE_EXPERIMENTAL)
#add_definitions(-DGLM_LANG_STL11_FORCED)

#LoadVK.cpp
#TriangleMain.cpp
#TriangleVK.cpp
#PrebuildLayer.cpp
#PrebuildLayerVK.cpp
#TraceableLayer.cpp
#TraceableLayerVK.cpp
#FirstWindow.cpp
#FirstWindowVK.cpp

#vksample
add_library(vksample SHARED
            android_native_app_glue.c
            jniactivity.c
            Tutorial.cpp
            TutorialVK.cpp
            vulkan_wrapper.cpp
            )

target_include_directories(vksample PRIVATE
                          ${CMAKE_CURRENT_SOURCE_DIR}
                          ${CMAKE_CURRENT_SOURCE_DIR}/data
                          ${CMAKE_CURRENT_SOURCE_DIR}/stb
                          ${CMAKE_CURRENT_SOURCE_DIR}/tinyobjloader
                          ${ANDROID_NDK}/sources/third_party/shaderc/include)

target_link_libraries(vksample
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

###vktriangle
add_library(vktriangle SHARED
            android_native_app_glue.c
            jniactivity.c
            TriangleMain.cpp
            TriangleVK.cpp
            vulkan_wrapper.cpp
            )

target_include_directories(vktriangle PRIVATE
                          ${CMAKE_CURRENT_SOURCE_DIR}
                          ${CMAKE_CURRENT_SOURCE_DIR}/data
                          ${CMAKE_CURRENT_SOURCE_DIR}/stb
                          ${CMAKE_CURRENT_SOURCE_DIR}/tinyobjloader
                          ${ANDROID_NDK}/sources/third_party/shaderc/include)

target_link_libraries(vktriangle
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

##vkprebuild
add_library(vkprebuild SHARED
            android_native_app_glue.c
            jniactivity.c
            PrebuildLayer.cpp
            PrebuildLayerVK.cpp
            vulkan_wrapper.cpp
            )

target_include_directories(vkprebuild PRIVATE
                          ${CMAKE_CURRENT_SOURCE_DIR}
                          ${CMAKE_CURRENT_SOURCE_DIR}/data
                          ${CMAKE_CURRENT_SOURCE_DIR}/stb
                          ${CMAKE_CURRENT_SOURCE_DIR}/tinyobjloader
                          ${ANDROID_NDK}/sources/third_party/shaderc/include)

target_link_libraries(vkprebuild
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


##vktrace
add_library(vktrace SHARED
            android_native_app_glue.c
            jniactivity.c
            TraceableLayer.cpp
            TraceableLayerVK.cpp
            vulkan_wrapper.cpp
            )

target_include_directories(vktrace PRIVATE
                          ${CMAKE_CURRENT_SOURCE_DIR}
                          ${CMAKE_CURRENT_SOURCE_DIR}/data
                          ${CMAKE_CURRENT_SOURCE_DIR}/stb
                          ${CMAKE_CURRENT_SOURCE_DIR}/tinyobjloader
                          ${ANDROID_NDK}/sources/third_party/shaderc/include)

target_link_libraries(vktrace
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

##vkwindow
add_library(vkwindow SHARED
            android_native_app_glue.c
            jniactivity.c
            FirstWindow.cpp
            FirstWindowVK.cpp
            vulkan_wrapper.cpp
            )

target_include_directories(vkwindow PRIVATE
                          ${CMAKE_CURRENT_SOURCE_DIR}
                          ${CMAKE_CURRENT_SOURCE_DIR}/data
                          ${CMAKE_CURRENT_SOURCE_DIR}/stb
                          ${CMAKE_CURRENT_SOURCE_DIR}/tinyobjloader
                          ${ANDROID_NDK}/sources/third_party/shaderc/include)

target_link_libraries(vkwindow
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

