/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_ANDROID_VIDEO_RENDER_ANDROID_SURFACE_VIEW_H_
#define WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_ANDROID_VIDEO_RENDER_ANDROID_SURFACE_VIEW_H_

#include <jni.h>

#include "video_render_defines.h"
#include "video_render_android_impl.h"

namespace webrtc {

class CriticalSectionWrapper;

class AndroidSurfaceViewChannel : public AndroidStream {
 public:
  AndroidSurfaceViewChannel(WebRtc_UWord32 streamId,
                            JavaVM* jvm,
                            VideoRenderAndroid& renderer,
                            jobject javaRenderObj);
  ~AndroidSurfaceViewChannel();

  WebRtc_Word32 Init(WebRtc_Word32 zOrder,
                     const float left,
                     const float top,
                     const float right,
                     const float bottom);

  //Implement VideoRenderCallback
  virtual WebRtc_Word32 RenderFrame(const WebRtc_UWord32 streamId,
                                    VideoFrame& videoFrame);

  //Implements AndroidStream
  virtual void DeliverFrame(JNIEnv* jniEnv);

 private:
  WebRtc_UWord32 _id;
  CriticalSectionWrapper& _renderCritSect;

  VideoFrame _bufferToRender;
  VideoRenderAndroid& _renderer;
  JavaVM* _jvm;
  jobject _javaRenderObj;

#ifdef ANDROID_NDK_8_OR_ABOVE
  jclass _javaBitmapClass;
  jmethodID _createBitmapCid;
  jobject _javaBitmapObj;
  jmethodID _drawBitmapCid;
#else
  jobject _javaByteBufferObj;
  unsigned char* _directBuffer;
  jmethodID _createByteBufferCid;
  jmethodID _drawByteBufferCid;
#endif
  jmethodID _setCoordinatesCid;
  unsigned int _bitmapWidth;
  unsigned int _bitmapHeight;
};

class AndroidSurfaceViewRenderer : private VideoRenderAndroid {
 public:
  AndroidSurfaceViewRenderer(const WebRtc_Word32 id,
                             const VideoRenderType videoRenderType,
                             void* window,
                             const bool fullscreen);
  ~AndroidSurfaceViewRenderer();
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

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_ANDROID_VIDEO_RENDER_ANDROID_SURFACE_VIEW_H_
