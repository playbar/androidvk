cmake_minimum_required(VERSION 3.4.1)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -fexceptions")

#add_definitions(-DGLM_ENABLE_EXPERIMENTAL)
add_definitions(-DVK_USE_PLATFORM_ANDROID_KHR)
add_definitions(-DVK_NO_PROTOTYPES)
#add_definitions(-DGLM_LANG_STL11_FORCED)
add_definitions(-DRAPIDJSON_HAS_CXX11_RVALUE_REFS=1)

add_subdirectory(shaderc)

get_filename_component(SHADERC_SRC
                       ${ANDROID_NDK}/sources/third_party/shaderc
                       ABSOLUTE)

# Import shaderc lib in NDK
#add_library(shaderc_lib STATIC IMPORTED)
#set_target_properties(shaderc_lib PROPERTIES IMPORTED_LOCATION
#                      ${SHADERC_SRC}/libs/${ANDROID_STL}/${ANDROID_ABI}/libshaderc.a)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/assimp)
#add_library(assimp_lib STATIC IMPORTED)
#set_target_properties(assimp_lib PROPERTIES IMPORTED_LOCATION
#                      ${CMAKE_CURRENT_SOURCE_DIR}/assimp/libassimp.a)

add_library(vkvulkan SHARED
            android_native_app_glue.c
            cpu-features.c
            jniactivity.c
            src/vulkanandroid.cpp
            src/VulkanBuffer.cpp
            src/VulkanDebug.cpp
            src/VulkanDevice.cpp
            src/VulkanMain.cpp
            src/VulkanTextOverlay.cpp
            src/VulkanModel.cpp
            src/VulkanSwapChain.cpp
            src/VulkanTexture.cpp
            src/VulkanTools.cpp
            )


target_include_directories(vkvulkan PRIVATE
                          ${CMAKE_CURRENT_SOURCE_DIR}
                          ${CMAKE_CURRENT_SOURCE_DIR}/assimp/include
                          ${CMAKE_CURRENT_SOURCE_DIR}/stb
                          ${CMAKE_CURRENT_SOURCE_DIR}/src
                          )

target_link_libraries(vkvulkan
                      android
                      log
                      z
                      assimp
                      shaderc)
