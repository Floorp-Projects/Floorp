/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_ANDROID_VIDEO_RENDER_ANDROID_NATIVE_OPENGL2_H_
#define WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_ANDROID_VIDEO_RENDER_ANDROID_NATIVE_OPENGL2_H_

#include <jni.h>

#include "video_render_defines.h"
#include "video_render_android_impl.h"
#include "video_render_opengles20.h"

namespace webrtc {

class CriticalSectionWrapper;

class AndroidNativeOpenGl2Channel: public AndroidStream {
 public:
  AndroidNativeOpenGl2Channel(
      WebRtc_UWord32 streamId,
      JavaVM* jvm,
      VideoRenderAndroid& renderer,jobject javaRenderObj);
  ~AndroidNativeOpenGl2Channel();

  WebRtc_Word32 Init(WebRtc_Word32 zOrder,
                     const float left,
                     const float top,
                     const float right,
                     const float bottom);

  //Implement VideoRenderCallback
  virtual WebRtc_Word32 RenderFrame(
      const WebRtc_UWord32 streamId,
      I420VideoFrame& videoFrame);

  //Implements AndroidStream
  virtual void DeliverFrame(JNIEnv* jniEnv);

 private:
  static jint CreateOpenGLNativeStatic(
      JNIEnv * env,
      jobject,
      jlong context,
      jint width,
      jint height);
  jint CreateOpenGLNative(int width, int height);

  static void DrawNativeStatic(JNIEnv * env,jobject, jlong context);
  void DrawNative();
  WebRtc_UWord32 _id;
  CriticalSectionWrapper& _renderCritSect;

  I420VideoFrame _bufferToRender;
  VideoRenderAndroid& _renderer;
  JavaVM*     _jvm;
  jobject     _javaRenderObj;

  jmethodID      _redrawCid;
  jmethodID      _registerNativeCID;
  jmethodID      _deRegisterNativeCID;
  VideoRenderOpenGles20 _openGLRenderer;
};


class AndroidNativeOpenGl2Renderer: private VideoRenderAndroid {
 public:
  AndroidNativeOpenGl2Renderer(const WebRtc_Word32 id,
                               const VideoRenderType videoRenderType,
                               void* window,
                               const bool fullscreen);

  ~AndroidNativeOpenGl2Renderer();
  static bool UseOpenGL2(void* window);

  WebRtc_Word32 Init();
  virtual AndroidStream* CreateAndroidRenderChannel(
      WebRtc_Word32 streamId,
      WebRtc_Word32 zOrder,
      const float left,
      const float top,
      const float right,
      const float bottom,
      VideoRenderAndroid& renderer);

 private:
  jobject _javaRenderObj;
  jclass _javaRenderClass;
};

} //namespace webrtc

#endif // WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_ANDROID_VIDEO_RENDER_ANDROID_NATIVE_OPENGL2_H_
