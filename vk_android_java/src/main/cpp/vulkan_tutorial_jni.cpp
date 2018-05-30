#include <android/native_window_jni.h>
#include <android/asset_manager_jni.h>

#include "vulkan_tutorial_jni.h"
#include "vulkan_tutorial.h"

JNIEXPORT jlong JNICALL
Java_com_vk_androidjava_VkTutorial_create(
        JNIEnv *env, jclass type, jobject assetManager_, jstring vertexShader_,
        jstring fragmentShader_) {

    AAssetManager *assetManager = AAssetManager_fromJava(env, assetManager_);
    if (assetManager == nullptr) {
        LOGE("get assetManager fail!");
        return 0;
    }

    const char *vertexShader = env->GetStringUTFChars(vertexShader_, 0);
    const char *fragmentShader = env->GetStringUTFChars(fragmentShader_, 0);

    VKTutorial *app = new VKTutorial(assetManager, vertexShader, fragmentShader);

    env->ReleaseStringUTFChars(vertexShader_, vertexShader);
    env->ReleaseStringUTFChars(fragmentShader_, fragmentShader);

    return (jlong) app;
}

JNIEXPORT void JNICALL
Java_com_vk_androidjava_VkTutorial_run__JLandroid_view_Surface_2(
        JNIEnv *env, jclass type, jlong nativeHandle, jobject surface) {
    ANativeWindow *window = ANativeWindow_fromSurface(env, surface);
    if (window == nullptr) {
        LOGE("get window from surface fail!");
        return;
    }

    VKTutorial *app = reinterpret_cast<VKTutorial *>(nativeHandle);
    app->run(window);
}

JNIEXPORT void JNICALL
Java_com_vk_androidjava_VkTutorial_pause__J(JNIEnv *env, jclass type,
                                                                       jlong nativeHandle) {
    VKTutorial *app = reinterpret_cast<VKTutorial *>(nativeHandle);
    app->pause();
}

JNIEXPORT void JNICALL
Java_com_vk_androidjava_VkTutorial_resume__J(JNIEnv *env, jclass type, jlong nativeHandle) {
    VKTutorial *app = reinterpret_cast<VKTutorial *>(nativeHandle);
    app->resume();
}

JNIEXPORT void JNICALL
Java_com_vk_androidjava_VkTutorial_surfaceChanged__J(JNIEnv *env, jclass type, jlong nativeHandle)
{
    VKTutorial *app = reinterpret_cast<VKTutorial *>(nativeHandle);
    app->surfaceChanged();
}

JNIEXPORT void JNICALL
Java_com_vk_androidjava_VkTutorial_stop__J(JNIEnv *env, jclass type, jlong nativeHandle) {
    VKTutorial *app = reinterpret_cast<VKTutorial *>(nativeHandle);
    app->stop();
}

