/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_render/android/video_render_android_impl.h"

#include "webrtc/modules/video_render/video_render_internal.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/event_wrapper.h"
#include "webrtc/system_wrappers/interface/thread_wrapper.h"
#include "webrtc/system_wrappers/interface/tick_util.h"

#ifdef ANDROID
#include <android/log.h>
#include <stdio.h>

#undef WEBRTC_TRACE
#define WEBRTC_TRACE(a,b,c,...)  __android_log_print(ANDROID_LOG_DEBUG, "*WEBRTCN*", __VA_ARGS__)
#else
#include "webrtc/system_wrappers/interface/trace.h"
#endif

namespace webrtc {

JavaVM* VideoRenderAndroid::g_jvm = NULL;

int32_t SetRenderAndroidVM(JavaVM* javaVM) {
  WEBRTC_TRACE(kTraceDebug, kTraceVideoRenderer, -1, "%s", __FUNCTION__);
  VideoRenderAndroid::g_jvm = javaVM;
  return 0;
}

VideoRenderAndroid::VideoRenderAndroid(
    const int32_t id,
    const VideoRenderType videoRenderType,
    void* window,
    const bool /*fullscreen*/):
    _id(id),
    _critSect(*CriticalSectionWrapper::CreateCriticalSection()),
    _renderType(videoRenderType),
    _ptrWindow((jobject)(window)),
    _javaShutDownFlag(false),
    _javaShutdownEvent(*EventWrapper::Create()),
    _javaRenderEvent(*EventWrapper::Create()),
    _lastJavaRenderEvent(0),
    _javaRenderJniEnv(NULL),
    _javaRenderThread(NULL) {
}

VideoRenderAndroid::~VideoRenderAndroid() {
  WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, _id,
               "VideoRenderAndroid dtor");

  if (_javaRenderThread)
    StopRender();

  for (AndroidStreamMap::iterator it = _streamsMap.begin();
       it != _streamsMap.end();
       ++it) {
    delete it->second;
  }
  delete &_javaShutdownEvent;
  delete &_javaRenderEvent;
  delete &_critSect;
}

int32_t VideoRenderAndroid::ChangeUniqueId(const int32_t id) {
  CriticalSectionScoped cs(&_critSect);
  _id = id;

  return 0;
}

int32_t VideoRenderAndroid::ChangeWindow(void* /*window*/) {
  return -1;
}

VideoRenderCallback*
VideoRenderAndroid::AddIncomingRenderStream(const uint32_t streamId,
                                            const uint32_t zOrder,
                                            const float left, const float top,
                                            const float right,
                                            const float bottom) {
  CriticalSectionScoped cs(&_critSect);

  AndroidStream* renderStream = NULL;
  AndroidStreamMap::iterator item = _streamsMap.find(streamId);
  if (item != _streamsMap.end() && item->second != NULL) {
    WEBRTC_TRACE(kTraceInfo,
                 kTraceVideoRenderer,
                 -1,
                 "%s: Render stream already exists",
                 __FUNCTION__);
    return renderStream;
  }

  renderStream = CreateAndroidRenderChannel(streamId, zOrder, left, top,
                                            right, bottom, *this);
  if (renderStream) {
    _streamsMap[streamId] = renderStream;
  }
  else {
    WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                 "(%s:%d): renderStream is NULL", __FUNCTION__, __LINE__);
    return NULL;
  }
  return renderStream;
}

int32_t VideoRenderAndroid::DeleteIncomingRenderStream(
    const uint32_t streamId) {
  CriticalSectionScoped cs(&_critSect);

  AndroidStreamMap::iterator item = _streamsMap.find(streamId);
  if (item == _streamsMap.end()) {
    WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                 "(%s:%d): renderStream is NULL", __FUNCTION__, __LINE__);
    return -1;
  }
  delete item->second;
  _streamsMap.erase(item);
  return 0;
}

int32_t VideoRenderAndroid::GetIncomingRenderStreamProperties(
    const uint32_t streamId,
    uint32_t& zOrder,
    float& left,
    float& top,
    float& right,
    float& bottom) const {
  return -1;
}

int32_t VideoRenderAndroid::StartRender() {
  CriticalSectionScoped cs(&_critSect);

  if (_javaRenderThread) {
    // StartRender is called when this stream should start render.
    // However StopRender is not called when the streams stop rendering.
    // Thus the the thread  is only deleted when the renderer is removed.
    WEBRTC_TRACE(kTraceDebug, kTraceVideoRenderer, _id,
                 "%s, Render thread already exist", __FUNCTION__);
    return 0;
  }

  _javaRenderThread = ThreadWrapper::CreateThread(JavaRenderThreadFun, this,
                                                  kRealtimePriority,
                                                  "AndroidRenderThread");
  if (!_javaRenderThread) {
    WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                 "%s: No thread", __FUNCTION__);
    return -1;
  }

  unsigned int tId = 0;
  if (_javaRenderThread->Start(tId))
    WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, _id,
                 "%s: thread started: %u", __FUNCTION__, tId);
  else {
    WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                 "%s: Could not start send thread", __FUNCTION__);
    return -1;
  }
  return 0;
}

int32_t VideoRenderAndroid::StopRender() {
  WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, _id, "%s:", __FUNCTION__);
  {
    CriticalSectionScoped cs(&_critSect);
    if (!_javaRenderThread)
    {
      return -1;
    }
    _javaShutDownFlag = true;
    _javaRenderEvent.Set();
  }

  _javaShutdownEvent.Wait(3000);
  CriticalSectionScoped cs(&_critSect);
  _javaRenderThread->SetNotAlive();
  if (_javaRenderThread->Stop()) {
    delete _javaRenderThread;
    _javaRenderThread = NULL;
  }
  else {
    assert(false);
    WEBRTC_TRACE(kTraceWarning, kTraceVideoRenderer, _id,
                 "%s: Not able to stop thread, leaking", __FUNCTION__);
    _javaRenderThread = NULL;
  }
  return 0;
}

void VideoRenderAndroid::ReDraw() {
  CriticalSectionScoped cs(&_critSect);
  // Allow redraw if it was more than 20ms since last.
  if (_lastJavaRenderEvent < TickTime::MillisecondTimestamp() - 20) {
    _lastJavaRenderEvent = TickTime::MillisecondTimestamp();
    _javaRenderEvent.Set();
  }
}

bool VideoRenderAndroid::JavaRenderThreadFun(void* obj) {
  return static_cast<VideoRenderAndroid*> (obj)->JavaRenderThreadProcess();
}

bool VideoRenderAndroid::JavaRenderThreadProcess()
{
  _javaRenderEvent.Wait(1000);

  CriticalSectionScoped cs(&_critSect);
  if (!_javaRenderJniEnv) {
    // try to attach the thread and get the env
    // Attach this thread to JVM
    jint res = g_jvm->AttachCurrentThread(&_javaRenderJniEnv, NULL);

    // Get the JNI env for this thread
    if ((res < 0) || !_javaRenderJniEnv) {
      WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                   "%s: Could not attach thread to JVM (%d, %p)",
                   __FUNCTION__, res, _javaRenderJniEnv);
      return false;
    }
  }

  for (AndroidStreamMap::iterator it = _streamsMap.begin();
       it != _streamsMap.end();
       ++it) {
    it->second->DeliverFrame(_javaRenderJniEnv);
  }

  if (_javaShutDownFlag) {
    if (g_jvm->DetachCurrentThread() < 0)
      WEBRTC_TRACE(kTraceWarning, kTraceVideoRenderer, _id,
                   "%s: Could not detach thread from JVM", __FUNCTION__);
    else {
      WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, _id,
                   "%s: Java thread detached", __FUNCTION__);
    }
    _javaRenderJniEnv = NULL;
    _javaShutDownFlag = false;
    _javaShutdownEvent.Set();
    return false; // Do not run this thread again.
  }
  return true;
}

VideoRenderType VideoRenderAndroid::RenderType() {
  return _renderType;
}

RawVideoType VideoRenderAndroid::PerferedVideoType() {
  return kVideoI420;
}

bool VideoRenderAndroid::FullScreen() {
  return false;
}

int32_t VideoRenderAndroid::GetGraphicsMemory(
    uint64_t& /*totalGraphicsMemory*/,
    uint64_t& /*availableGraphicsMemory*/) const {
  WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
               "%s - not supported on Android", __FUNCTION__);
  return -1;
}

int32_t VideoRenderAndroid::GetScreenResolution(
    uint32_t& /*screenWidth*/,
    uint32_t& /*screenHeight*/) const {
  WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
               "%s - not supported on Android", __FUNCTION__);
  return -1;
}

uint32_t VideoRenderAndroid::RenderFrameRate(
    const uint32_t /*streamId*/) {
  WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
               "%s - not supported on Android", __FUNCTION__);
  return -1;
}

int32_t VideoRenderAndroid::SetStreamCropping(
    const uint32_t /*streamId*/,
    const float /*left*/,
    const float /*top*/,
    const float /*right*/,
    const float /*bottom*/) {
  WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
               "%s - not supported on Android", __FUNCTION__);
  return -1;
}

int32_t VideoRenderAndroid::SetTransparentBackground(const bool enable) {
  WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
               "%s - not supported on Android", __FUNCTION__);
  return -1;
}

int32_t VideoRenderAndroid::ConfigureRenderer(
    const uint32_t streamId,
    const unsigned int zOrder,
    const float left,
    const float top,
    const float right,
    const float bottom) {
  WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
               "%s - not supported on Android", __FUNCTION__);
  return -1;
}

int32_t VideoRenderAndroid::SetText(
    const uint8_t textId,
    const uint8_t* text,
    const int32_t textLength,
    const uint32_t textColorRef,
    const uint32_t backgroundColorRef,
    const float left, const float top,
    const float rigth, const float bottom) {
  WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
               "%s - not supported on Android", __FUNCTION__);
  return -1;
}

int32_t VideoRenderAndroid::SetBitmap(const void* bitMap,
                                      const uint8_t pictureId,
                                      const void* colorKey,
                                      const float left, const float top,
                                      const float right,
                                      const float bottom) {
  WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
               "%s - not supported on Android", __FUNCTION__);
  return -1;
}

}  // namespace webrtc
