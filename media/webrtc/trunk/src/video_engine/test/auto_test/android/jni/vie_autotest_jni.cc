/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>
#include <string.h>
#include <android/log.h>
#include <pthread.h>
#include <unistd.h>

#include "org_webrtc_vieautotest_vie_autotest.h"

#include "vie_autotest_android.h"

#define WEBRTC_LOG_TAG "*WEBRTCN*"

// VideoEngine data struct
typedef struct
{
    JavaVM* jvm;
} VideoEngineData;

// Global variables
JavaVM* webrtcGlobalVM;

// Global variables visible in this file
static VideoEngineData vieData;

// "Local" functions (i.e. not Java accessible)
#define WEBRTC_TRACE_MAX_MESSAGE_SIZE 1024

static bool GetSubAPIs(VideoEngineData& vieData);
static bool ReleaseSubAPIs(VideoEngineData& vieData);

//
// General functions
//

// JNI_OnLoad
jint JNI_OnLoad(JavaVM* vm, void* /*reserved*/) {
  if (!vm) {
    __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                        "JNI_OnLoad did not receive a valid VM pointer");
    return -1;
  }

  JNIEnv* env;
  if (JNI_OK != vm->GetEnv(reinterpret_cast<void**> (&env),
                           JNI_VERSION_1_4)) {
    __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                        "JNI_OnLoad could not get JNI env");
    return -1;
  }

  // Init ViE data
  vieData.jvm = vm;

  return JNI_VERSION_1_4;
}

// Class:     org_webrtc_vieautotest_ViEAutotest
// Method:    RunTest
// Signature: (IILandroid/opengl/GLSurfaceView;Landroid/opengl/GLSurfaceView;)I
JNIEXPORT jint JNICALL
Java_org_webrtc_vieautotest_ViEAutotest_RunTest__IILandroid_opengl_GLSurfaceView_2Landroid_opengl_GLSurfaceView_2(
    JNIEnv* env,
    jobject context,
    jint testType,
    jint subtestType,
    jobject glView1,
    jobject glView2)
{
  int numErrors = -1;
  numErrors = ViEAutoTestAndroid::RunAutotest(testType, subtestType, glView1,
                                              glView2, vieData.jvm, env,
                                              context);
  return numErrors;
}

// Class:     org_webrtc_vieautotest_ViEAutotest
// Method:    RunTest
// Signature: (IILandroid/view/SurfaceView;Landroid/view/SurfaceView;)I
JNIEXPORT jint JNICALL
Java_org_webrtc_vieautotest_ViEAutotest_RunTest__IILandroid_view_SurfaceView_2Landroid_view_SurfaceView_2(
    JNIEnv* env,
    jobject context,
    jint testType,
    jint subtestType,
    jobject surfaceHolder1,
    jobject surfaceHolder2)
{
  int numErrors = -1;
  numErrors = ViEAutoTestAndroid::RunAutotest(testType, subtestType,
                                              surfaceHolder1, surfaceHolder2,
                                              vieData.jvm, env, context);
  return numErrors;
}

//
//local function
//

bool GetSubAPIs(VideoEngineData& vieData) {
  bool retVal = true;
  //vieData.base = ViEBase::GetInterface(vieData.vie);
  //if (vieData.base == NULL)
  {
    __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                        "Could not get Base API");
    retVal = false;
  }
  return retVal;
}

bool ReleaseSubAPIs(VideoEngineData& vieData) {
  bool releaseOk = true;
  //if (vieData.base)
  {
    //if (vieData.base->Release() != 0)
    if (false) {
      __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                          "Release base sub-API failed");
      releaseOk = false;
    }
    else {
      //vieData.base = NULL;
    }
  }

  return releaseOk;
}
