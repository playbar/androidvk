file(GLOB UTILS_SOURCE *.cpp)

set(SAMPLES_DATA_DIR ${SAMPLES_DATA_DIR} "${PROJECT_SOURCE_DIR}/API-Samples/data")
include_directories( ${SAMPLES_DATA_DIR} ${GLSLANG_SPIRV_INCLUDE_DIR} ${GLMINC_PREFIX} )

if(ANDROID)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11  -Werror -DVK_USE_PLATFORM_ANDROID_KHR")
elseif(WIN32)
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_CRT_SECURE_NO_WARNINGS -D_USE_MATH_DEFINES")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_CRT_SECURE_NO_WARNINGS -D_USE_MATH_DEFINES")

    # If MSVC, disable some signed/unsigned mismatch warnings.
    if (MSVC)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4267")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4267")
    endif()
else()
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-sign-compare")
endif()

add_library(${UTILS_NAME} STATIC ${UTILS_SOURCE})

if(ANDROID)
   add_library(native_app_glue STATIC
               ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)
   target_include_directories(${UTILS_NAME} PRIVATE
                              ${NDK_INCLUDE}
                              ${ANDROID_NDK}/sources/android/native_app_glue
                              ${CMAKE_CURRENT_SOURCE_DIR}/../vulkan_wrapper
                              ${CMAKE_CURRENT_SOURCE_DIR}/../data
                              ${ANDROID_NDK}/sources/third_party/shaderc/include)
   target_link_libraries(${UTILS_NAME}
                              android
                              log
                              native_app_glue)

   # Build Shaderc from source code inside NDK and also add
   # the building task to be dependent of $UTILS_NAME lib.
   include(ExternalProject)
   set(SHADERC_OUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/build/shaderc)
   set(lib_shaderc ${SHADERC_OUT_DIR}/${ANDROID_STL}/${ANDROID_ABI}/libshaderc.a)
   set(SHADERC_SRC_DIR ${ANDROID_NDK}/sources/third_party/shaderc)
   if(CMAKE_HOST_WIN32)
        ExternalProject_Add(BuildShaderc
            SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
            CONFIGURE_COMMAND ""
            BUILD_COMMAND ${ANDROID_NDK}/ndk-build.cmd --jobs=8 NDK_DEBUG=1
                          NDK_PROJECT_PATH=${SHADERC_SRC_DIR}
                          NDK_OUT=./shaderc
                          NDK_LIBS_OUT=${SHADERC_OUT_DIR}
                          APP_ABI=${ANDROID_ABI}
                          APP_PLATFORM=${ANDROID_PLATFORM}
                          APP_STL=${ANDROID_STL}
                          NDK_TOOLCHAIN_VERSION=${ANDROID_TOOLCHAIN}
                          APP_BUILD_SCRIPT=${SHADERC_SRC_DIR}/Android.mk
                          libshaderc_combined

            LOG_CONFIGURE 1
            BUILD_ALWAYS 1
            LOG_BUILD    1
            BUILD_BYPRODUCTS ${lib_shaderc}
            INSTALL_COMMAND "")
   else(CMAKE_HOST_WIN32)
        ExternalProject_Add(BuildShaderc
            SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
            CONFIGURE_COMMAND mkdir -p ${SHADERC_OUT_DIR}
            BUILD_COMMAND ${ANDROID_NDK}/ndk-build --jobs=8 NDK_DEBUG=1
                          NDK_PROJECT_PATH=${SHADERC_SRC_DIR}
                          NDK_LIBS_OUT=${SHADERC_OUT_DIR}
                          APP_ABI=${ANDROID_ABI}
                          APP_PLATFORM=${ANDROID_PLATFORM}
                          APP_STL=${ANDROID_STL}
                          NDK_TOOLCHAIN_VERSION=${ANDROID_TOOLCHAIN}
                          APP_BUILD_SCRIPT=${SHADERC_SRC_DIR}/Android.mk
                          libshaderc_combined     # make the combined lib to use
            BUILD_ALWAYS 1    # Force build
            BUILD_BYPRODUCTS ${lib_shaderc}
            INSTALL_COMMAND "")
   endif(CMAKE_HOST_WIN32)

   add_dependencies(${UTILS_NAME} BuildShaderc)
   target_link_libraries(${UTILS_NAME} ${lib_shaderc})

endif()
