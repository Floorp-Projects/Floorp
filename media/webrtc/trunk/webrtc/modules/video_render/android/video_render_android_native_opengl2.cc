/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_render_android_native_opengl2.h"
#include "critical_section_wrapper.h"
#include "tick_util.h"

#ifdef ANDROID_LOG
#include <stdio.h>
#include <android/log.h>

#undef WEBRTC_TRACE
#define WEBRTC_TRACE(a,b,c,...)  __android_log_print(ANDROID_LOG_DEBUG, "*WEBRTC*", __VA_ARGS__)
#else
#include "trace.h"
#endif

namespace webrtc {

AndroidNativeOpenGl2Renderer::AndroidNativeOpenGl2Renderer(
    const int32_t id,
    const VideoRenderType videoRenderType,
    void* window,
    const bool fullscreen) :
    VideoRenderAndroid(id, videoRenderType, window, fullscreen),
    _javaRenderObj(NULL),
    _javaRenderClass(NULL) {
}

bool AndroidNativeOpenGl2Renderer::UseOpenGL2(void* window) {
  if (!g_jvm) {
    WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, -1,
                 "RendererAndroid():UseOpenGL No JVM set.");
    return false;
  }
  bool isAttached = false;
  JNIEnv* env = NULL;
  if (g_jvm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) {
    // try to attach the thread and get the env
    // Attach this thread to JVM
    jint res = g_jvm->AttachCurrentThread(&env, NULL);

    // Get the JNI env for this thread
    if ((res < 0) || !env) {
      WEBRTC_TRACE(
          kTraceError,
          kTraceVideoRenderer,
          -1,
          "RendererAndroid(): Could not attach thread to JVM (%d, %p)",
          res, env);
      return false;
    }
    isAttached = true;
  }

  // get the renderer class
  jclass javaRenderClassLocal =
      env->FindClass("org/webrtc/videoengine/ViEAndroidGLES20");
  if (!javaRenderClassLocal) {
    WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, -1,
                 "%s: could not find ViEAndroidRenderer class",
                 __FUNCTION__);
    return false;
  }

  // get the method ID for UseOpenGL
  jmethodID cidUseOpenGL = env->GetStaticMethodID(javaRenderClassLocal,
                                                  "UseOpenGL2",
                                                  "(Ljava/lang/Object;)Z");
  if (cidUseOpenGL == NULL) {
    WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, -1,
                 "%s: could not get UseOpenGL ID", __FUNCTION__);
    return false;
  }
  jboolean res = env->CallStaticBooleanMethod(javaRenderClassLocal,
                                              cidUseOpenGL, (jobject) window);

  // Detach this thread if it was attached
  if (isAttached) {
    if (g_jvm->DetachCurrentThread() < 0) {
      WEBRTC_TRACE(kTraceWarning, kTraceVideoRenderer, -1,
                   "%s: Could not detach thread from JVM", __FUNCTION__);
    }
  }
  return res;
}

AndroidNativeOpenGl2Renderer::~AndroidNativeOpenGl2Renderer() {
  WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, _id,
               "AndroidNativeOpenGl2Renderer dtor");
  if (g_jvm) {
    // get the JNI env for this thread
    bool isAttached = false;
    JNIEnv* env = NULL;
    if (g_jvm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) {
      // try to attach the thread and get the env
      // Attach this thread to JVM
      jint res = g_jvm->AttachCurrentThread(&env, NULL);

      // Get the JNI env for this thread
      if ((res < 0) || !env) {
        WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                     "%s: Could not attach thread to JVM (%d, %p)",
                     __FUNCTION__, res, env);
        env = NULL;
      }
      else {
        isAttached = true;
      }
    }

    env->DeleteGlobalRef(_javaRenderObj);
    env->DeleteGlobalRef(_javaRenderClass);

    if (isAttached) {
      if (g_jvm->DetachCurrentThread() < 0) {
        WEBRTC_TRACE(kTraceWarning, kTraceVideoRenderer, _id,
                     "%s: Could not detach thread from JVM",
                     __FUNCTION__);
      }
    }
  }
}

int32_t AndroidNativeOpenGl2Renderer::Init() {
  WEBRTC_TRACE(kTraceDebug, kTraceVideoRenderer, _id, "%s", __FUNCTION__);
  if (!g_jvm) {
    WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                 "(%s): Not a valid Java VM pointer.", __FUNCTION__);
    return -1;
  }
  if (!_ptrWindow) {
    WEBRTC_TRACE(kTraceWarning, kTraceVideoRenderer, _id,
                 "(%s): No window have been provided.", __FUNCTION__);
    return -1;
  }

  // get the JNI env for this thread
  bool isAttached = false;
  JNIEnv* env = NULL;
  if (g_jvm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) {
    // try to attach the thread and get the env
    // Attach this thread to JVM
    jint res = g_jvm->AttachCurrentThread(&env, NULL);

    // Get the JNI env for this thread
    if ((res < 0) || !env) {
      WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                   "%s: Could not attach thread to JVM (%d, %p)",
                   __FUNCTION__, res, env);
      return -1;
    }
    isAttached = true;
  }

  // get the ViEAndroidGLES20 class
  jclass javaRenderClassLocal =
      env->FindClass("org/webrtc/videoengine/ViEAndroidGLES20");
  if (!javaRenderClassLocal) {
    WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                 "%s: could not find ViEAndroidGLES20", __FUNCTION__);
    return -1;
  }

  // create a global reference to the class (to tell JNI that
  // we are referencing it after this function has returned)
  _javaRenderClass =
      reinterpret_cast<jclass> (env->NewGlobalRef(javaRenderClassLocal));
  if (!_javaRenderClass) {
    WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                 "%s: could not create Java SurfaceHolder class reference",
                 __FUNCTION__);
    return -1;
  }

  // Delete local class ref, we only use the global ref
  env->DeleteLocalRef(javaRenderClassLocal);

  // create a reference to the object (to tell JNI that we are referencing it
  // after this function has returned)
  _javaRenderObj = env->NewGlobalRef(_ptrWindow);
  if (!_javaRenderObj) {
    WEBRTC_TRACE(
        kTraceError,
        kTraceVideoRenderer,
        _id,
        "%s: could not create Java SurfaceRender object reference",
        __FUNCTION__);
    return -1;
  }

  // Detach this thread if it was attached
  if (isAttached) {
    if (g_jvm->DetachCurrentThread() < 0) {
      WEBRTC_TRACE(kTraceWarning, kTraceVideoRenderer, _id,
                   "%s: Could not detach thread from JVM", __FUNCTION__);
    }
  }

  WEBRTC_TRACE(kTraceDebug, kTraceVideoRenderer, _id, "%s done",
               __FUNCTION__);
  return 0;

}
AndroidStream*
AndroidNativeOpenGl2Renderer::CreateAndroidRenderChannel(
    int32_t streamId,
    int32_t zOrder,
    const float left,
    const float top,
    const float right,
    const float bottom,
    VideoRenderAndroid& renderer) {
  WEBRTC_TRACE(kTraceDebug, kTraceVideoRenderer, _id, "%s: Id %d",
               __FUNCTION__, streamId);
  AndroidNativeOpenGl2Channel* stream =
      new AndroidNativeOpenGl2Channel(streamId, g_jvm, renderer,
                                      _javaRenderObj);
  if (stream && stream->Init(zOrder, left, top, right, bottom) == 0)
    return stream;
  else {
    delete stream;
  }
  return NULL;
}

AndroidNativeOpenGl2Channel::AndroidNativeOpenGl2Channel(
    uint32_t streamId,
    JavaVM* jvm,
    VideoRenderAndroid& renderer,jobject javaRenderObj):
    _id(streamId),
    _renderCritSect(*CriticalSectionWrapper::CreateCriticalSection()),
    _renderer(renderer), _jvm(jvm), _javaRenderObj(javaRenderObj),
    _registerNativeCID(NULL), _deRegisterNativeCID(NULL),
    _openGLRenderer(streamId) {

}
AndroidNativeOpenGl2Channel::~AndroidNativeOpenGl2Channel() {
  WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, _id,
               "AndroidNativeOpenGl2Channel dtor");
  delete &_renderCritSect;
  if (_jvm) {
    // get the JNI env for this thread
    bool isAttached = false;
    JNIEnv* env = NULL;
    if (_jvm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) {
      // try to attach the thread and get the env
      // Attach this thread to JVM
      jint res = _jvm->AttachCurrentThread(&env, NULL);

      // Get the JNI env for this thread
      if ((res < 0) || !env) {
        WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                     "%s: Could not attach thread to JVM (%d, %p)",
                     __FUNCTION__, res, env);
        env = NULL;
      } else {
        isAttached = true;
      }
    }
    if (env && _deRegisterNativeCID) {
      env->CallVoidMethod(_javaRenderObj, _deRegisterNativeCID);
    }

    if (isAttached) {
      if (_jvm->DetachCurrentThread() < 0) {
        WEBRTC_TRACE(kTraceWarning, kTraceVideoRenderer, _id,
                     "%s: Could not detach thread from JVM",
                     __FUNCTION__);
      }
    }
  }
}

int32_t AndroidNativeOpenGl2Channel::Init(int32_t zOrder,
                                          const float left,
                                          const float top,
                                          const float right,
                                          const float bottom)
{
  WEBRTC_TRACE(kTraceDebug, kTraceVideoRenderer, _id,
               "%s: AndroidNativeOpenGl2Channel", __FUNCTION__);
  if (!_jvm) {
    WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                 "%s: Not a valid Java VM pointer", __FUNCTION__);
    return -1;
  }

  // get the JNI env for this thread
  bool isAttached = false;
  JNIEnv* env = NULL;
  if (_jvm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) {
    // try to attach the thread and get the env
    // Attach this thread to JVM
    jint res = _jvm->AttachCurrentThread(&env, NULL);

    // Get the JNI env for this thread
    if ((res < 0) || !env) {
      WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                   "%s: Could not attach thread to JVM (%d, %p)",
                   __FUNCTION__, res, env);
      return -1;
    }
    isAttached = true;
  }

  jclass javaRenderClass =
      env->FindClass("org/webrtc/videoengine/ViEAndroidGLES20");
  if (!javaRenderClass) {
    WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                 "%s: could not find ViESurfaceRenderer", __FUNCTION__);
    return -1;
  }

  // get the method ID for the ReDraw function
  _redrawCid = env->GetMethodID(javaRenderClass, "ReDraw", "()V");
  if (_redrawCid == NULL) {
    WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                 "%s: could not get ReDraw ID", __FUNCTION__);
    return -1;
  }

  _registerNativeCID = env->GetMethodID(javaRenderClass,
                                        "RegisterNativeObject", "(J)V");
  if (_registerNativeCID == NULL) {
    WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                 "%s: could not get RegisterNativeObject ID", __FUNCTION__);
    return -1;
  }

  _deRegisterNativeCID = env->GetMethodID(javaRenderClass,
                                          "DeRegisterNativeObject", "()V");
  if (_deRegisterNativeCID == NULL) {
    WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                 "%s: could not get DeRegisterNativeObject ID",
                 __FUNCTION__);
    return -1;
  }

  JNINativeMethod nativeFunctions[2] = {
    { "DrawNative",
      "(J)V",
      (void*) &AndroidNativeOpenGl2Channel::DrawNativeStatic, },
    { "CreateOpenGLNative",
      "(JII)I",
      (void*) &AndroidNativeOpenGl2Channel::CreateOpenGLNativeStatic },
  };
  if (env->RegisterNatives(javaRenderClass, nativeFunctions, 2) == 0) {
    WEBRTC_TRACE(kTraceDebug, kTraceVideoRenderer, -1,
                 "%s: Registered native functions", __FUNCTION__);
  }
  else {
    WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, -1,
                 "%s: Failed to register native functions", __FUNCTION__);
    return -1;
  }

  env->CallVoidMethod(_javaRenderObj, _registerNativeCID, (jlong) this);

  // Detach this thread if it was attached
  if (isAttached) {
    if (_jvm->DetachCurrentThread() < 0) {
      WEBRTC_TRACE(kTraceWarning, kTraceVideoRenderer, _id,
                   "%s: Could not detach thread from JVM", __FUNCTION__);
    }
  }

  if (_openGLRenderer.SetCoordinates(zOrder, left, top, right, bottom) != 0) {
    return -1;
  }
  WEBRTC_TRACE(kTraceDebug, kTraceVideoRenderer, _id,
               "%s: AndroidNativeOpenGl2Channel done", __FUNCTION__);
  return 0;
}

int32_t AndroidNativeOpenGl2Channel::RenderFrame(
    const uint32_t /*streamId*/,
    I420VideoFrame& videoFrame) {
  //   WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer,_id, "%s:" ,__FUNCTION__);
  _renderCritSect.Enter();
  _bufferToRender.SwapFrame(&videoFrame);
  _renderCritSect.Leave();
  _renderer.ReDraw();
  return 0;
}

/*Implements AndroidStream
 * Calls the Java object and render the buffer in _bufferToRender
 */
void AndroidNativeOpenGl2Channel::DeliverFrame(JNIEnv* jniEnv) {
  //TickTime timeNow=TickTime::Now();

  //Draw the Surface
  jniEnv->CallVoidMethod(_javaRenderObj, _redrawCid);

  // WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer,_id,
  // "%s: time to deliver %lld" ,__FUNCTION__,
  // (TickTime::Now()-timeNow).Milliseconds());
}

/*
 * JNI callback from Java class. Called when the render
 * want to render a frame. Called from the GLRenderThread
 * Method:    DrawNative
 * Signature: (J)V
 */
void JNICALL AndroidNativeOpenGl2Channel::DrawNativeStatic(
    JNIEnv * env, jobject, jlong context) {
  AndroidNativeOpenGl2Channel* renderChannel =
      reinterpret_cast<AndroidNativeOpenGl2Channel*>(context);
  renderChannel->DrawNative();
}

void AndroidNativeOpenGl2Channel::DrawNative() {
  _openGLRenderer.Render(_bufferToRender);
}

/*
 * JNI callback from Java class. Called when the GLSurfaceview
 * have created a surface. Called from the GLRenderThread
 * Method:    CreateOpenGLNativeStatic
 * Signature: (JII)I
 */
jint JNICALL AndroidNativeOpenGl2Channel::CreateOpenGLNativeStatic(
    JNIEnv * env,
    jobject,
    jlong context,
    jint width,
    jint height) {
  AndroidNativeOpenGl2Channel* renderChannel =
      reinterpret_cast<AndroidNativeOpenGl2Channel*> (context);
  WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, -1, "%s:", __FUNCTION__);
  return renderChannel->CreateOpenGLNative(width, height);
}

jint AndroidNativeOpenGl2Channel::CreateOpenGLNative(
    int width, int height) {
  return _openGLRenderer.Setup(width, height);
}

}  //namespace webrtc
