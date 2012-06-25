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

#include "org_webrtc_capturemoduleandroidtest_VideoCaptureModuleTest.h"
#include "../../../interface/video_capture_factory.h"
#include "../../../../../video_render/main/interface/video_render.h"
#include "../../testAPI/testPlatformDependent.h"
#include "../../testAPI/testPlatformDependent.h"
#ifdef RENDER_PREVIEW
#include "../../testAPI/Renderer.h"
#endif

using namespace webrtc;
#define WEBRTC_LOG_TAG "*WEBRTCN*" // As in WEBRTC Native...
// ADM data struct
typedef struct
{
    // Other
    JavaVM* jvm;
    Renderer* renderer;
    VideoCaptureModule* _videoCapture;
    VideoCaptureModule::DeviceInfo*_captureInfo;
} JniData;

// Global variables visible in this file
static JniData jniData;

//////////////////////////////////////////////////////////////////
// General functions
//////////////////////////////////////////////////////////////////

/////////////////////////////////////////////
// JNI_OnLoad
//
jint JNI_OnLoad(JavaVM* vm, void* /*reserved*/)
{
  __android_log_write(ANDROID_LOG_DEBUG, WEBRTC_LOG_TAG, "JNI_OnLoad");
  if (!vm)
  {
    __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                        "JNI_OnLoad did not receive a valid VM pointer");
    return -1;
  }

  // Get JNI
  JNIEnv* env;
  if (JNI_OK != vm->GetEnv(reinterpret_cast<void**> (&env), JNI_VERSION_1_4))
  {
    __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                        "JNI_OnLoad could not get JNI env");
    return -1;
  }

  // Init JniData data
  memset(&jniData, 0, sizeof(jniData));

  // Store the JVM
  jniData.jvm = vm;

  return JNI_VERSION_1_4;
}

/////////////////////////////////////////////
// Run Test
//
JNIEXPORT jint JNICALL
Java_org_webrtc_capturemoduleandroidtest_VideoCaptureModuleTest_RunTest(
    JNIEnv * env,
    jobject context,
    jobject surface)
{
  __android_log_write(ANDROID_LOG_DEBUG, WEBRTC_LOG_TAG, "Run test");
  // Set instance independent Java objects
  VideoCaptureModule::SetAndroidObjects(jniData.jvm, context);

  // Start test
  __android_log_write(ANDROID_LOG_DEBUG, WEBRTC_LOG_TAG,
                      "Create testPlatformDependent");
  testPlatformDependent testPlatformDependent;
  testPlatformDependent.SetRenderer(jniData.renderer);
  testPlatformDependent.DoTest();

  // Clear instance independent Java objects
  VideoCaptureModule::SetAndroidObjects(NULL, NULL);

  return 0;
}

JNIEXPORT jint JNICALL
Java_org_webrtc_capturemoduleandroidtest_VideoCaptureModuleTest_RenderInit(
    JNIEnv * env,
    jobject context,
    jobject surface)
{
  VideoRender::SetAndroidObjects(jniData.jvm);
#ifdef RENDER_PREVIEW
  Renderer::SetRenderWindow(surface);
  jniData.renderer=new Renderer(true);
#endif
}

JNIEXPORT jint JNICALL
Java_org_webrtc_capturemoduleandroidtest_VideoCaptureModuleTest_StartCapture(
    JNIEnv * env,
    jobject context)
{
  if (!jniData._captureInfo) {
    VideoCaptureModule::SetAndroidObjects(jniData.jvm, context);
    jniData._captureInfo = VideoCaptureFactory::CreateDeviceInfo(5);
    WebRtc_UWord8 id[256];
    WebRtc_UWord8 name[256];
    jniData._captureInfo->GetDeviceName(0, name, 256, id, 256);
    jniData._videoCapture = VideoCaptureFactory::Create(0, id);
    VideoCaptureCapability capability;

    jniData._captureInfo->GetCapability(id, 0, capability);
    capability.width = 176;
    capability.height = 144;
    capability.maxFPS = 15;

    jniData._videoCapture->StartCapture(capability);
  }
  return 0;
}

JNIEXPORT jint JNICALL
Java_org_webrtc_capturemoduleandroidtest_VideoCaptureModuleTest_StopCapture(
    JNIEnv * env,
    jobject context)
{
  if (jniData._videoCapture) {
    jniData._videoCapture->StopCapture();
    delete jniData._captureInfo;
    VideoCaptureModule::Destroy(jniData._videoCapture);
    jniData._videoCapture = NULL;
    jniData._captureInfo = NULL;
  }
  return 0;
}
