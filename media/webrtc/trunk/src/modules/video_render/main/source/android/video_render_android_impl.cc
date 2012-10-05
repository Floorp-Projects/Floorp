/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_render_android_impl.h"

#include "critical_section_wrapper.h"
#include "event_wrapper.h"
#include "thread_wrapper.h"
#include "tick_util.h"

#ifdef ANDROID_LOG
#include <stdio.h>
#include <android/log.h>

#undef WEBRTC_TRACE
#define WEBRTC_TRACE(a,b,c,...)  __android_log_print(ANDROID_LOG_DEBUG, "*WEBRTCN*", __VA_ARGS__)
#else
#include "trace.h"
#endif

namespace webrtc {

JavaVM* VideoRenderAndroid::g_jvm = NULL;

#if defined(WEBRTC_ANDROID) && !defined(WEBRTC_CHROMIUM_BUILD)
WebRtc_Word32 SetRenderAndroidVM(void* javaVM) {
  WEBRTC_TRACE(kTraceDebug, kTraceVideoRenderer, -1, "%s", __FUNCTION__);
  VideoRenderAndroid::g_jvm = (JavaVM*)javaVM;
  return 0;
}
#endif

VideoRenderAndroid::VideoRenderAndroid(
    const WebRtc_Word32 id,
    const VideoRenderType videoRenderType,
    void* window,
    const bool /*fullscreen*/):
    _id(id),
    _critSect(*CriticalSectionWrapper::CreateCriticalSection()),
    _renderType(videoRenderType),
    _ptrWindow((jobject)(window)),
    _streamsMap(),
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

  for (MapItem* item = _streamsMap.First(); item != NULL; item
           = _streamsMap.Next(item)) { // Delete streams
    delete static_cast<AndroidStream*> (item->GetItem());
  }
  delete &_javaShutdownEvent;
  delete &_javaRenderEvent;
  delete &_critSect;
}

WebRtc_Word32 VideoRenderAndroid::ChangeUniqueId(const WebRtc_Word32 id) {
  CriticalSectionScoped cs(&_critSect);
  _id = id;

  return 0;
}

WebRtc_Word32 VideoRenderAndroid::ChangeWindow(void* /*window*/) {
  return -1;
}

VideoRenderCallback*
VideoRenderAndroid::AddIncomingRenderStream(const WebRtc_UWord32 streamId,
                                            const WebRtc_UWord32 zOrder,
                                            const float left, const float top,
                                            const float right,
                                            const float bottom) {
  CriticalSectionScoped cs(&_critSect);

  AndroidStream* renderStream = NULL;
  MapItem* item = _streamsMap.Find(streamId);
  if (item) {
    renderStream = (AndroidStream*) (item->GetItem());
    if (NULL != renderStream) {
      WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, -1,
                   "%s: Render stream already exists", __FUNCTION__);
      return renderStream;
    }
  }

  renderStream = CreateAndroidRenderChannel(streamId, zOrder, left, top,
                                            right, bottom, *this);
  if (renderStream) {
    _streamsMap.Insert(streamId, renderStream);
  }
  else {
    WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                 "(%s:%d): renderStream is NULL", __FUNCTION__, __LINE__);
    return NULL;
  }
  return renderStream;
}

WebRtc_Word32 VideoRenderAndroid::DeleteIncomingRenderStream(
    const WebRtc_UWord32 streamId) {
  CriticalSectionScoped cs(&_critSect);

  MapItem* item = _streamsMap.Find(streamId);
  if (item) {
    delete (AndroidStream*) item->GetItem();
    _streamsMap.Erase(streamId);
  }
  else {
    WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                 "(%s:%d): renderStream is NULL", __FUNCTION__, __LINE__);
    return -1;
  }
  return 0;
}

WebRtc_Word32 VideoRenderAndroid::GetIncomingRenderStreamProperties(
    const WebRtc_UWord32 streamId,
    WebRtc_UWord32& zOrder,
    float& left,
    float& top,
    float& right,
    float& bottom) const {
  return -1;
}

WebRtc_Word32 VideoRenderAndroid::StartRender() {
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

WebRtc_Word32 VideoRenderAndroid::StopRender() {
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

  for (MapItem* item = _streamsMap.First(); item != NULL;
       item = _streamsMap.Next(item)) {
    static_cast<AndroidStream*> (item->GetItem())->DeliverFrame(
        _javaRenderJniEnv);
  }

  if (_javaShutDownFlag) {
    if (g_jvm->DetachCurrentThread() < 0)
      WEBRTC_TRACE(kTraceWarning, kTraceVideoRenderer, _id,
                   "%s: Could not detach thread from JVM", __FUNCTION__);
    else {
      WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, _id,
                   "%s: Java thread detached", __FUNCTION__);
    }
    _javaRenderJniEnv = false;
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

WebRtc_Word32 VideoRenderAndroid::GetGraphicsMemory(
    WebRtc_UWord64& /*totalGraphicsMemory*/,
    WebRtc_UWord64& /*availableGraphicsMemory*/) const {
  WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
               "%s - not supported on Android", __FUNCTION__);
  return -1;
}

WebRtc_Word32 VideoRenderAndroid::GetScreenResolution(
    WebRtc_UWord32& /*screenWidth*/,
    WebRtc_UWord32& /*screenHeight*/) const {
  WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
               "%s - not supported on Android", __FUNCTION__);
  return -1;
}

WebRtc_UWord32 VideoRenderAndroid::RenderFrameRate(
    const WebRtc_UWord32 /*streamId*/) {
  WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
               "%s - not supported on Android", __FUNCTION__);
  return -1;
}

WebRtc_Word32 VideoRenderAndroid::SetStreamCropping(
    const WebRtc_UWord32 /*streamId*/,
    const float /*left*/,
    const float /*top*/,
    const float /*right*/,
    const float /*bottom*/) {
  WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
               "%s - not supported on Android", __FUNCTION__);
  return -1;
}

WebRtc_Word32 VideoRenderAndroid::SetTransparentBackground(const bool enable) {
  WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
               "%s - not supported on Android", __FUNCTION__);
  return -1;
}

WebRtc_Word32 VideoRenderAndroid::ConfigureRenderer(
    const WebRtc_UWord32 streamId,
    const unsigned int zOrder,
    const float left,
    const float top,
    const float right,
    const float bottom) {
  WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
               "%s - not supported on Android", __FUNCTION__);
  return -1;
}

WebRtc_Word32 VideoRenderAndroid::SetText(
    const WebRtc_UWord8 textId,
    const WebRtc_UWord8* text,
    const WebRtc_Word32 textLength,
    const WebRtc_UWord32 textColorRef,
    const WebRtc_UWord32 backgroundColorRef,
    const float left, const float top,
    const float rigth, const float bottom) {
  WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
               "%s - not supported on Android", __FUNCTION__);
  return -1;
}

WebRtc_Word32 VideoRenderAndroid::SetBitmap(const void* bitMap,
                                            const WebRtc_UWord8 pictureId,
                                            const void* colorKey,
                                            const float left, const float top,
                                            const float right,
                                            const float bottom) {
  WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
               "%s - not supported on Android", __FUNCTION__);
  return -1;
}

}  // namespace webrtc
