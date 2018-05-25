/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#include <string.h>
#include <jni.h>
#include <android/log.h>
#include <assert.h>
#include <errno.h>
#include "my_log.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/system_properties.h>
#include <dlfcn.h>
#include <unordered_map>
#include "vector"

#define LOG_TAG "test"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define ABI "armeabi-v7a"

void * m_hDLL;

typedef uint64_t (*FP_GetTicks)(void);
FP_GetTicks gpFunGetTicks;

extern JavaVM *gs_jvm;

extern void testFun1();

void testcode6()
{
    FILE *stream;
    stream = popen("pwd", "r");
    char ch[1024];
    fgets(ch, 1024, stream);
    LOGW("pwd: %s",ch);
    pclose(stream);
    stream = popen("ls", "r");
    if( NULL == stream )
    {
        LOGE("Unable to execute the command");
    }
    else
    {
        char buffer[1024];
        int status;
        while( NULL != fgets(buffer, 1024, stream))
        {
            LOGE("read: %s", buffer);
        }
        status = pclose(stream);
        LOGW("process exited with status %d", status);
    }
}

void testProperties()
{
    char value[PROP_VALUE_MAX];
    if(0 == __system_property_get("ro.product.model", value))
    {
        LOGE("error");
    }
    else
    {
        LOGW("product model: %s", value);
    }

    const prop_info *property;
    property = __system_property_find("ro.product.model");
    if( NULL == property )
    {
        LOGE("error");
    }
    else
    {
        char name[PROP_NAME_MAX];
        char value[PROP_VALUE_MAX];
        if( 0 == __system_property_read(property, name, value))
        {
            LOGE("is empty");
        }
        else
        {
            LOGW("%s, %s", name, value);
        }
    }
    return;
}

int is_file_exist(const char *file_path)
{
    if(file_path == NULL )
    {
        return  -1;
    }
    if( access(file_path, F_OK) == 0 )
        return 0;
    return -1;
}

int func2(int a, int b)
{
    return a + b;
}

int func4(int a, int b, int c, int d)
{
    a = 0xF200;
    a = 0xF201;
    b = 0;
    b = 1;
    b = 2;
    b = 3;
    b = 4;
    b = 0x10;
    return a + b + c + d;
}

typedef struct
{
    char b;
    short c;
    int a;

}StB;

#pragma pack(1)
unsigned char Buffer[1000]; // 待解析数据缓冲区
#pragma unpack();

void funcTest()
{
    int size= sizeof(StB);
    unsigned char *pPos = Buffer;// 记录缓冲区起始地址，并用于解析缓冲区中的数据

    float *pValueA = (float *)pPos;// 记录浮点数 A 的地址
    pPos+= sizeof(float);// 跳过浮点数 A

//    unsigned char cTag = *pPos++;// 跳过一个字节的标记位
    pPos = pPos + 1;

    float *pValueB = (float *)pPos;// 记录浮点数 B 的地址

    float a = *pValueA;
    float b = *pValueB;
//    float b = *(float*)((char*)pValueB + 2);



    *pValueA = 1.0f;

    *pValueA = (a) *(*pValueB);

//    (*pValueA) = (*pValueA) * (*pValueB); // 此处崩溃！！
    (*pValueB) = (*pValueB) * (*pValueB);
}

void loadLib()
{
    const char *filename = "/data/data/com.reverse/lib/libgperf.so";
    LOGE("name:%s", filename);
    is_file_exist(filename);
    m_hDLL = dlopen(filename, RTLD_LAZY);
    if( m_hDLL == NULL)
    {
        LOGE( "dlopen err:%s.\n",dlerror());
    }
    gpFunGetTicks = (FP_GetTicks)dlsym(m_hDLL, "GetTicks");
    uint64_t tick = gpFunGetTicks();
    LOGE("tick:%lld", tick);
    if (m_hDLL)
        dlclose(m_hDLL);
    return;
}

void testFun()
{
//    int * p = malloc(sizeof(int) * 1);
//    int *p = 0xdbebf7d0;
//    *p = 1;
    asm(
        "movs r0, #0xdb \n\t"
        "str r0, [sp] \n\t"
        "movs r1, #1 \n\t"
        "str r1, [r0]"
    );
}

JNIEXPORT void JNICALL
Java_com_mktest_HelloJni_nativeMsg(JNIEnv* env, jobject thiz)
{
//    loadLib();
//    testFun();
//    int i = 1, j = 2;
//    funcTest();
//    int res = func(1, 2, 3, 4);
//    LOGE("res:%d", res );
//    int ival = 0;
//    ival = func2(1, 2);
//    testFun1();
    int bkval = 10;
    int ival = 0;
    int bval = 0;
    asm(
    "mov r0, #1 \n\t"
    "mov r1, #2 \n\t"
//    "mov r2, #3 \n\t"
//    "mov r3, #4 \n\t"
    "blx func2 \n\t"
    "str r0, [sp, #0x20]"
    );
    LOGE("func2 %d, %d, %d", bkval, ival, bval);
}

JNIEXPORT jstring JNICALL
Java_com_mktest_HelloJni_stringFromJNI( JNIEnv* env, jobject thiz )
{

//    testProperties();
//    testcode6();

//    MY_LOG_VERBOSE("The stringFromJNI is called");
//    LOGE( "The stringFromJNI is called");
//    MY_LOG_DEBUG("env=%p thiz=%p", env, thiz);
//    MY_LOG_DEBUG("%s", "=========>test");
//    MY_LOG_ASSERT(0!=env, "JNIEnv cannot be NULL");
//    MY_LOG_INFO("REturning a new string");

    if( JNI_OK == (*env)->MonitorEnter(env, thiz)){
        LOGE("MonitorEnterr");
    }

    int result = 0;
//    system("pwd");
    result = system("mkdir /data/data/com.reverse/temp");
    if( -1 == result || 127 == result )
    {
        LOGE("error");
    }

    pid_t pid = getpid();
    uid_t uid = getuid();

    char *username = getlogin();
    LOGE("%s", username);

//    char *buffer;
//    size_t i;
//    buffer = (char*)malloc(4);
//    for(i = 0; i < 5; ++i )
//    {
//        buffer[i] = 'a';
//    }
//    free(buffer);

//    if( 0 != errno )
//    {
//        __android_log_assert("0!=errno","hello-jni", "There is an error.");
//    }

    if(JNI_OK == (*env)->MonitorExit(env, thiz)){
        LOGE("MonitorExit");
    }
//    (*env)->ExceptionClear(env);
    return (*env)->NewStringUTF(env, "Hello from JNI !  Compiled with ABI " ABI ".");
}

JNIEXPORT jstring JNICALL
Java_com_mktest_HelloJni_stringFromJNI_11(JNIEnv* env, jobject thiz )
{
    int result = 0;
//    system("pwd");
    result = system("mkdir /data/data/com.bar.hellojni/temp");
    if( -1 == result || 127 == result )
    {
        LOGI("error");
        LOGE("error");
    }

    pid_t pid = getpid();
    uid_t uid = getuid();

    char *username = getlogin();
    LOGE("F:%s,%s", __FUNCTION__, username);
    return (*env)->NewStringUTF(env, "stringFromJNI_11");
}

