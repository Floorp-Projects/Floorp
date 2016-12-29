/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_ANDROID_VIDEO_RENDER_ANDROID_IMPL_H_
#define WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_ANDROID_VIDEO_RENDER_ANDROID_IMPL_H_

#include <jni.h>

#include <map>

#include "webrtc/base/platform_thread.h"
#include "webrtc/modules/video_render/i_video_render.h"


namespace webrtc {

//#define ANDROID_LOG

class CriticalSectionWrapper;
class EventWrapper;

// The object a module user uses to send new frames to the java renderer
// Base class for android render streams.

class AndroidStream : public VideoRenderCallback {
 public:
  // DeliverFrame is called from a thread connected to the Java VM.
  // Used for Delivering frame for rendering.
  virtual void DeliverFrame(JNIEnv* jniEnv)=0;

  virtual ~AndroidStream() {};
};

class VideoRenderAndroid: IVideoRender {
 public:
  VideoRenderAndroid(const int32_t id,
                     const VideoRenderType videoRenderType,
                     void* window,
                     const bool fullscreen);

  virtual ~VideoRenderAndroid();

  virtual int32_t Init()=0;

  virtual int32_t ChangeWindow(void* window);

  virtual VideoRenderCallback* AddIncomingRenderStream(
      const uint32_t streamId,
      const uint32_t zOrder,
      const float left, const float top,
      const float right, const float bottom);

  virtual int32_t DeleteIncomingRenderStream(
      const uint32_t streamId);

  virtual int32_t GetIncomingRenderStreamProperties(
      const uint32_t streamId,
      uint32_t& zOrder,
      float& left, float& top,
      float& right, float& bottom) const;

  virtual int32_t StartRender();

  virtual int32_t StopRender();

  virtual void ReDraw();

  // Properties

  virtual VideoRenderType RenderType();

  virtual RawVideoType PerferedVideoType();

  virtual bool FullScreen();

  virtual int32_t GetGraphicsMemory(
      uint64_t& totalGraphicsMemory,
      uint64_t& availableGraphicsMemory) const;

  virtual int32_t GetScreenResolution(
      uint32_t& screenWidth,
      uint32_t& screenHeight) const;

  virtual uint32_t RenderFrameRate(const uint32_t streamId);

  virtual int32_t SetStreamCropping(const uint32_t streamId,
                                    const float left, const float top,
                                    const float right, const float bottom);

  virtual int32_t SetTransparentBackground(const bool enable);

  virtual int32_t ConfigureRenderer(const uint32_t streamId,
                                    const unsigned int zOrder,
                                    const float left, const float top,
                                    const float right, const float bottom);

  virtual int32_t SetText(const uint8_t textId,
                          const uint8_t* text,
                          const int32_t textLength,
                          const uint32_t textColorRef,
                          const uint32_t backgroundColorRef,
                          const float left, const float top,
                          const float rigth, const float bottom);

  virtual int32_t SetBitmap(const void* bitMap,
                            const uint8_t pictureId,
                            const void* colorKey, const float left,
                            const float top, const float right,
                            const float bottom);
  static JavaVM* g_jvm;

 protected:
  virtual AndroidStream* CreateAndroidRenderChannel(
      int32_t streamId,
      int32_t zOrder,
      const float left,
      const float top,
      const float right,
      const float bottom,
      VideoRenderAndroid& renderer) = 0;

  int32_t _id;
  CriticalSectionWrapper& _critSect;
  VideoRenderType _renderType;
  jobject _ptrWindow;

 private:
  static bool JavaRenderThreadFun(void* obj);
  bool JavaRenderThreadProcess();

  // Map with streams to render.
  typedef std::map<int32_t, AndroidStream*> AndroidStreamMap;
  AndroidStreamMap _streamsMap;
  // True if the _javaRenderThread thread shall be detached from the JVM.
  bool _javaShutDownFlag;
  EventWrapper& _javaShutdownEvent;
  EventWrapper& _javaRenderEvent;
  int64_t _lastJavaRenderEvent;
  JNIEnv* _javaRenderJniEnv; // JNIEnv for the java render thread.
  // TODO(pbos): Remove scoped_ptr and use the member directly.
  rtc::scoped_ptr<rtc::PlatformThread> _javaRenderThread;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_ANDROID_VIDEO_RENDER_ANDROID_IMPL_H_
