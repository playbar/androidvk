#ifndef __VULKAN_TUTORIAL_JNI_H__
#define __VULKAN_TUTORIAL_JNI_H__

#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT jlong JNICALL
Java_com_vk_androidjava_VkTutorial_create(
        JNIEnv *env, jclass type, jobject assetManager_, jstring vertexShader_,
        jstring fragmentShader_);

JNIEXPORT void JNICALL
Java_com_vk_androidjava_VkTutorial_run__JLandroid_view_Surface_2(
        JNIEnv *env, jclass type, jlong nativeHandle, jobject surface);

JNIEXPORT void JNICALL
Java_com_vk_androidjava_VkTutorial_pause__J(JNIEnv *env, jclass type, jlong nativeHandle);

JNIEXPORT void JNICALL
Java_com_vk_androidjava_VkTutorial_resume__J(JNIEnv *env, jclass type, jlong nativeHandle);

JNIEXPORT void JNICALL
Java_com_vk_androidjava_VkTutorial_surfaceChanged__J(JNIEnv *env, jclass type, jlong nativeHandle);

JNIEXPORT void JNICALL
Java_com_vk_androidjava_VkTutorial_stop__J(JNIEnv *env, jclass type, jlong nativeHandle);

#ifdef __cplusplus
}
#endif


#endif

