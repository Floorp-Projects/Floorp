/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string.h> // memset
#include <android/log.h>

#include "org_webrtc_voiceengine_test_AudioDeviceAndroidTest.h"

#include "../../../../interface/audio_device.h"

#define LOG_TAG "WebRTC ADM Native"

void api_test();
void func_test(int);

typedef struct
{
    // Other
    JavaVM* jvm;
} AdmData;

static AdmData admData;

jint JNI_OnLoad(JavaVM* vm, void* /*reserved*/)
{
    if (!vm)
    {
        __android_log_write(ANDROID_LOG_ERROR, LOG_TAG,
                            "JNI_OnLoad did not receive a valid VM pointer");
        return -1;
    }

    // Get JNI
    JNIEnv* env;
    if (JNI_OK != vm->GetEnv(reinterpret_cast<void**> (&env),
                             JNI_VERSION_1_4))
    {
        __android_log_write(ANDROID_LOG_ERROR, LOG_TAG,
                            "JNI_OnLoad could not get JNI env");
        return -1;
    }

    // Get class to register the native functions with
    // jclass regClass =
    // env->FindClass("org/webrtc/voiceengine/test/AudioDeviceAndroidTest");
    // if (!regClass) {
    // return -1; // Exception thrown
    // }

    // Register native functions
    // JNINativeMethod methods[1];
    // methods[0].name = NULL;
    // methods[0].signature = NULL;
    // methods[0].fnPtr = NULL;
    // if (JNI_OK != env->RegisterNatives(regClass, methods, 1))
    // {
    // return -1;
    // }

    // Init VoiceEngine data
    memset(&admData, 0, sizeof(admData));

    // Store the JVM
    admData.jvm = vm;

    return JNI_VERSION_1_4;
}

JNIEXPORT jboolean JNICALL
Java_org_webrtc_voiceengine_test_AudioDeviceAndroidTest_NativeInit(JNIEnv * env,
                                                                   jclass)
{
    // Look up and cache any interesting class, field and method IDs for
    // any used java class here

    return true;
}

JNIEXPORT jint JNICALL
Java_org_webrtc_voiceengine_test_AudioDeviceAndroidTest_RunTest(JNIEnv *env,
                                                                jobject context,
                                                                jint test)
{
    // Set instance independent Java objects
    webrtc::AudioDeviceModule::SetAndroidObjects(admData.jvm, env, context);

    // Start test
    if (0 == test)
    {
        api_test();
    }
    else
    {
        func_test(test);
    }

    // Clear instance independent Java objects
    webrtc::AudioDeviceModule::SetAndroidObjects(NULL, NULL, NULL);

    return 0;
}
