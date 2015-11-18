/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>

#include "webrtc/engine_configurations.h"
#include "webrtc/modules/video_render/i_video_render.h"
#include "webrtc/modules/video_render/include/video_render_defines.h"
#include "webrtc/modules/video_render/incoming_video_stream.h"
#include "webrtc/modules/video_render/video_render_impl.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/trace.h"

#if defined (_WIN32)
#include "webrtc/modules/video_render/windows/video_render_windows_impl.h"
#define STANDARD_RENDERING kRenderWindows

// WEBRTC_IOS should go before WEBRTC_MAC because WEBRTC_MAC
// gets defined if WEBRTC_IOS is defined
#elif defined(WEBRTC_IOS)
#define STANDARD_RENDERING kRenderiOS
#include "webrtc/modules/video_render/ios/video_render_ios_impl.h"
#elif defined(WEBRTC_MAC)
#if defined(COCOA_RENDERING)
#define STANDARD_RENDERING kRenderCocoa
#include "webrtc/modules/video_render/mac/video_render_mac_cocoa_impl.h"
#elif defined(CARBON_RENDERING)
#define STANDARD_RENDERING kRenderCarbon
#include "webrtc/modules/video_render/mac/video_render_mac_carbon_impl.h"
#endif

#elif defined(WEBRTC_ANDROID)
#include "webrtc/modules/video_render/android/video_render_android_impl.h"
#include "webrtc/modules/video_render/android/video_render_android_native_opengl2.h"
#include "webrtc/modules/video_render/android/video_render_android_surface_view.h"
#define STANDARD_RENDERING kRenderAndroid

#elif defined(WEBRTC_LINUX)
#include "webrtc/modules/video_render/linux/video_render_linux_impl.h"
#define STANDARD_RENDERING kRenderX11

#else
//Other platforms
#endif

// For external rendering
#include "webrtc/modules/video_render/external/video_render_external_impl.h"
#ifndef STANDARD_RENDERING
#define STANDARD_RENDERING kRenderExternal
#endif  // STANDARD_RENDERING

namespace webrtc {

VideoRender*
VideoRender::CreateVideoRender(const int32_t id,
                               void* window,
                               const bool fullscreen,
                               const VideoRenderType videoRenderType/*=kRenderDefault*/)
{
    VideoRenderType resultVideoRenderType = videoRenderType;
    if (videoRenderType == kRenderDefault)
    {
        resultVideoRenderType = STANDARD_RENDERING;
    }
    return new ModuleVideoRenderImpl(id, resultVideoRenderType, window,
                                     fullscreen);
}

void VideoRender::DestroyVideoRender(
                                                         VideoRender* module)
{
    if (module)
    {
        delete module;
    }
}

ModuleVideoRenderImpl::ModuleVideoRenderImpl(
                                             const int32_t id,
                                             const VideoRenderType videoRenderType,
                                             void* window,
                                             const bool fullscreen) :
    _id(id), _moduleCrit(*CriticalSectionWrapper::CreateCriticalSection()),
    _ptrWindow(window), _fullScreen(fullscreen), _ptrRenderer(NULL)
{

    // Create platform specific renderer
    switch (videoRenderType)
    {
#if defined(_WIN32)
        case kRenderWindows:
        {
            VideoRenderWindowsImpl* ptrRenderer;
            ptrRenderer = new VideoRenderWindowsImpl(_id, videoRenderType, window, _fullScreen);
            if (ptrRenderer)
            {
                _ptrRenderer = reinterpret_cast<IVideoRender*>(ptrRenderer);
            }
        }
        break;

#elif defined(WEBRTC_IOS)
        case kRenderiOS:
        {
            VideoRenderIosImpl* ptrRenderer = new VideoRenderIosImpl(_id, window, _fullScreen);
            if(ptrRenderer)
            {
                _ptrRenderer = reinterpret_cast<IVideoRender*>(ptrRenderer);
            }
        }
        break;

#elif defined(WEBRTC_MAC)

#if defined(COCOA_RENDERING)
        case kRenderCocoa:
        {
            VideoRenderMacCocoaImpl* ptrRenderer = new VideoRenderMacCocoaImpl(_id, videoRenderType, window, _fullScreen);
            if(ptrRenderer)
            {
                _ptrRenderer = reinterpret_cast<IVideoRender*>(ptrRenderer);
            }
        }

        break;
#elif defined(CARBON_RENDERING)
        case kRenderCarbon:
        {
            VideoRenderMacCarbonImpl* ptrRenderer = new VideoRenderMacCarbonImpl(_id, videoRenderType, window, _fullScreen);
            if(ptrRenderer)
            {
                _ptrRenderer = reinterpret_cast<IVideoRender*>(ptrRenderer);
            }
        }
        break;
#endif

#elif defined(WEBRTC_ANDROID)
        case kRenderAndroid:
        {
            if(AndroidNativeOpenGl2Renderer::UseOpenGL2(window))
            {
                AndroidNativeOpenGl2Renderer* ptrRenderer = NULL;
                ptrRenderer = new AndroidNativeOpenGl2Renderer(_id, videoRenderType, window, _fullScreen);
                if (ptrRenderer)
                {
                    _ptrRenderer = reinterpret_cast<IVideoRender*> (ptrRenderer);
                }
            }
            else
            {
                AndroidSurfaceViewRenderer* ptrRenderer = NULL;
                ptrRenderer = new AndroidSurfaceViewRenderer(_id, videoRenderType, window, _fullScreen);
                if (ptrRenderer)
                {
                    _ptrRenderer = reinterpret_cast<IVideoRender*> (ptrRenderer);
                }
            }

        }
        break;
#elif defined(WEBRTC_LINUX)
        case kRenderX11:
        {
            VideoRenderLinuxImpl* ptrRenderer = NULL;
            ptrRenderer = new VideoRenderLinuxImpl(_id, videoRenderType, window, _fullScreen);
            if ( ptrRenderer )
            {
                _ptrRenderer = reinterpret_cast<IVideoRender*> (ptrRenderer);
            }
        }
        break;

#else
        // Other platforms
#endif
        case kRenderExternal:
        {
            VideoRenderExternalImpl* ptrRenderer(NULL);
            ptrRenderer = new VideoRenderExternalImpl(_id, videoRenderType,
                                                      window, _fullScreen);
            if (ptrRenderer)
            {
                _ptrRenderer = reinterpret_cast<IVideoRender*> (ptrRenderer);
            }
        }
            break;
        default:
            // Error...
            break;
    }
    if (_ptrRenderer)
    {
        if (_ptrRenderer->Init() == -1)
        {
        }
    }
}

ModuleVideoRenderImpl::~ModuleVideoRenderImpl()
{
    delete &_moduleCrit;

    for (IncomingVideoStreamMap::iterator it = _streamRenderMap.begin();
         it != _streamRenderMap.end();
         ++it) {
      delete it->second;
    }

    // Delete platform specific renderer
    if (_ptrRenderer)
    {
        VideoRenderType videoRenderType = _ptrRenderer->RenderType();

        switch (videoRenderType)
        {
            case kRenderExternal:
            {
                VideoRenderExternalImpl
                        * ptrRenderer =
                                reinterpret_cast<VideoRenderExternalImpl*> (_ptrRenderer);
                _ptrRenderer = NULL;
                delete ptrRenderer;
            }
            break;
#if defined(_WIN32)
            case kRenderWindows:
            {
                VideoRenderWindowsImpl* ptrRenderer = reinterpret_cast<VideoRenderWindowsImpl*>(_ptrRenderer);
                _ptrRenderer = NULL;
                delete ptrRenderer;
            }
            break;
#elif defined(WEBRTC_IOS)
            case kRenderiOS:
            {
              VideoRenderIosImpl* ptrRenderer = reinterpret_cast<VideoRenderIosImpl*> (_ptrRenderer);
              _ptrRenderer = NULL;
              delete ptrRenderer;
            }
            break;
#elif defined(WEBRTC_MAC)

#if defined(COCOA_RENDERING)
            case kRenderCocoa:
            {
                VideoRenderMacCocoaImpl* ptrRenderer = reinterpret_cast<VideoRenderMacCocoaImpl*> (_ptrRenderer);
                _ptrRenderer = NULL;
                delete ptrRenderer;
            }
            break;
#elif defined(CARBON_RENDERING)
            case kRenderCarbon:
            {
                VideoRenderMacCarbonImpl* ptrRenderer = reinterpret_cast<VideoRenderMacCarbonImpl*> (_ptrRenderer);
                _ptrRenderer = NULL;
                delete ptrRenderer;
            }
            break;
#endif

#elif defined(WEBRTC_ANDROID)
            case kRenderAndroid:
            {
                VideoRenderAndroid* ptrRenderer = reinterpret_cast<VideoRenderAndroid*> (_ptrRenderer);
                _ptrRenderer = NULL;
                delete ptrRenderer;
            }
            break;

#elif defined(WEBRTC_LINUX)
            case kRenderX11:
            {
                VideoRenderLinuxImpl* ptrRenderer = reinterpret_cast<VideoRenderLinuxImpl*> (_ptrRenderer);
                _ptrRenderer = NULL;
                delete ptrRenderer;
            }
            break;
#else
            //other platforms
#endif

            default:
                // Error...
                break;
        }
    }
}

int64_t ModuleVideoRenderImpl::TimeUntilNextProcess()
{
    // Not used
    return 50;
}
int32_t ModuleVideoRenderImpl::Process()
{
    // Not used
    return 0;
}

void*
ModuleVideoRenderImpl::Window()
{
    CriticalSectionScoped cs(&_moduleCrit);
    return _ptrWindow;
}

int32_t ModuleVideoRenderImpl::ChangeWindow(void* window)
{

    CriticalSectionScoped cs(&_moduleCrit);

#if defined(WEBRTC_IOS) // WEBRTC_IOS must go before WEBRTC_MAC
    _ptrRenderer = NULL;
    delete _ptrRenderer;

    VideoRenderIosImpl* ptrRenderer;
    ptrRenderer = new VideoRenderIosImpl(_id, window, _fullScreen);
    if (!ptrRenderer)
    {
        return -1;
    }
    _ptrRenderer = reinterpret_cast<IVideoRender*>(ptrRenderer);
    return _ptrRenderer->ChangeWindow(window);
#elif defined(WEBRTC_MAC)

    _ptrRenderer = NULL;
    delete _ptrRenderer;

#if defined(COCOA_RENDERING)
    VideoRenderMacCocoaImpl* ptrRenderer;
    ptrRenderer = new VideoRenderMacCocoaImpl(_id, kRenderCocoa, window, _fullScreen);
#elif defined(CARBON_RENDERING)
    VideoRenderMacCarbonImpl* ptrRenderer;
    ptrRenderer = new VideoRenderMacCarbonImpl(_id, kRenderCarbon, window, _fullScreen);
#endif
    if (!ptrRenderer)
    {
        return -1;
    }
    _ptrRenderer = reinterpret_cast<IVideoRender*>(ptrRenderer);
    return _ptrRenderer->ChangeWindow(window);

#else
    if (!_ptrRenderer)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                     "%s: No renderer", __FUNCTION__);
        return -1;
    }
    return _ptrRenderer->ChangeWindow(window);

#endif
}

int32_t ModuleVideoRenderImpl::Id()
{
    CriticalSectionScoped cs(&_moduleCrit);
    return _id;
}

uint32_t ModuleVideoRenderImpl::GetIncomingFrameRate(const uint32_t streamId) {
  CriticalSectionScoped cs(&_moduleCrit);

  IncomingVideoStreamMap::iterator it = _streamRenderMap.find(streamId);

  if (it == _streamRenderMap.end()) {
    // This stream doesn't exist
    WEBRTC_TRACE(kTraceError,
                 kTraceVideoRenderer,
                 _id,
                 "%s: stream doesn't exist",
                 __FUNCTION__);
    return 0;
  }
  assert(it->second != NULL);
  return it->second->IncomingRate();
}

VideoRenderCallback*
ModuleVideoRenderImpl::AddIncomingRenderStream(const uint32_t streamId,
                                               const uint32_t zOrder,
                                               const float left,
                                               const float top,
                                               const float right,
                                               const float bottom)
{
    CriticalSectionScoped cs(&_moduleCrit);

    if (!_ptrRenderer)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                     "%s: No renderer", __FUNCTION__);
        return NULL;
    }

    if (_streamRenderMap.find(streamId) != _streamRenderMap.end()) {
        // The stream already exists...
        WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                     "%s: stream already exists", __FUNCTION__);
        return NULL;
    }

    VideoRenderCallback* ptrRenderCallback =
            _ptrRenderer->AddIncomingRenderStream(streamId, zOrder, left, top,
                                                  right, bottom);
    if (ptrRenderCallback == NULL)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                     "%s: Can't create incoming stream in renderer",
                     __FUNCTION__);
        return NULL;
    }

    // Create platform independant code
    IncomingVideoStream* ptrIncomingStream = new IncomingVideoStream(_id,
                                                                     streamId);
    if (ptrIncomingStream == NULL)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                     "%s: Can't create incoming stream", __FUNCTION__);
        return NULL;
    }


    if (ptrIncomingStream->SetRenderCallback(ptrRenderCallback) == -1)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                     "%s: Can't set render callback", __FUNCTION__);
        delete ptrIncomingStream;
        _ptrRenderer->DeleteIncomingRenderStream(streamId);
        return NULL;
    }

    VideoRenderCallback* moduleCallback =
            ptrIncomingStream->ModuleCallback();

    // Store the stream
    _streamRenderMap[streamId] = ptrIncomingStream;

    return moduleCallback;
}

int32_t ModuleVideoRenderImpl::DeleteIncomingRenderStream(
                                                                const uint32_t streamId)
{
    CriticalSectionScoped cs(&_moduleCrit);

    if (!_ptrRenderer)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                     "%s: No renderer", __FUNCTION__);
        return -1;
    }

    IncomingVideoStreamMap::iterator item = _streamRenderMap.find(streamId);
    if (item == _streamRenderMap.end())
    {
        WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                     "%s: stream doesn't exist", __FUNCTION__);
        return -1;
    }

    delete item->second;

    _ptrRenderer->DeleteIncomingRenderStream(streamId);

    _streamRenderMap.erase(item);

    return 0;
}

int32_t ModuleVideoRenderImpl::AddExternalRenderCallback(
    const uint32_t streamId,
    VideoRenderCallback* renderObject) {
    CriticalSectionScoped cs(&_moduleCrit);

    IncomingVideoStreamMap::iterator item = _streamRenderMap.find(streamId);

    if (item == _streamRenderMap.end())
    {
        WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                     "%s: stream doesn't exist", __FUNCTION__);
        return -1;
    }

    if (item->second == NULL) {
        WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                     "%s: could not get stream", __FUNCTION__);
        return -1;
    }
    return item->second->SetExternalCallback(renderObject);
}

int32_t ModuleVideoRenderImpl::GetIncomingRenderStreamProperties(
    const uint32_t streamId,
    uint32_t& zOrder,
    float& left,
    float& top,
    float& right,
    float& bottom) const {
    CriticalSectionScoped cs(&_moduleCrit);

    if (!_ptrRenderer)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                     "%s: No renderer", __FUNCTION__);
        return -1;
    }

    return _ptrRenderer->GetIncomingRenderStreamProperties(streamId, zOrder,
                                                           left, top, right,
                                                           bottom);
}

uint32_t ModuleVideoRenderImpl::GetNumIncomingRenderStreams() const
{
    CriticalSectionScoped cs(&_moduleCrit);

    return static_cast<uint32_t>(_streamRenderMap.size());
}

bool ModuleVideoRenderImpl::HasIncomingRenderStream(
    const uint32_t streamId) const {
  CriticalSectionScoped cs(&_moduleCrit);

  return _streamRenderMap.find(streamId) != _streamRenderMap.end();
}

int32_t ModuleVideoRenderImpl::RegisterRawFrameCallback(
    const uint32_t streamId,
    VideoRenderCallback* callbackObj) {
  return -1;
}

int32_t ModuleVideoRenderImpl::StartRender(const uint32_t streamId)
{
    CriticalSectionScoped cs(&_moduleCrit);

    if (!_ptrRenderer)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                     "%s: No renderer", __FUNCTION__);
        return -1;
    }

    // Start the stream
    IncomingVideoStreamMap::iterator item = _streamRenderMap.find(streamId);

    if (item == _streamRenderMap.end())
    {
        return -1;
    }

    if (item->second->Start() == -1)
    {
        return -1;
    }

    // Start the HW renderer
    if (_ptrRenderer->StartRender() == -1)
    {
        return -1;
    }
    return 0;
}

int32_t ModuleVideoRenderImpl::StopRender(const uint32_t streamId)
{
    CriticalSectionScoped cs(&_moduleCrit);

    if (!_ptrRenderer)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                     "%s(%d): No renderer", __FUNCTION__, streamId);
        return -1;
    }

    // Stop the incoming stream
    IncomingVideoStreamMap::iterator item = _streamRenderMap.find(streamId);

    if (item == _streamRenderMap.end())
    {
        return -1;
    }

    if (item->second->Stop() == -1)
    {
        return -1;
    }

    return 0;
}

int32_t ModuleVideoRenderImpl::ResetRender()
{
    CriticalSectionScoped cs(&_moduleCrit);

    int32_t ret = 0;
    // Loop through all incoming streams and reset them
    for (IncomingVideoStreamMap::iterator it = _streamRenderMap.begin();
         it != _streamRenderMap.end();
         ++it) {
      if (it->second->Reset() == -1)
        ret = -1;
    }
    return ret;
}

RawVideoType ModuleVideoRenderImpl::PreferredVideoType() const
{
    CriticalSectionScoped cs(&_moduleCrit);

    if (_ptrRenderer == NULL)
    {
        return kVideoI420;
    }

    return _ptrRenderer->PerferedVideoType();
}

bool ModuleVideoRenderImpl::IsFullScreen()
{
    CriticalSectionScoped cs(&_moduleCrit);

    if (!_ptrRenderer)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                     "%s: No renderer", __FUNCTION__);
        return false;
    }
    return _ptrRenderer->FullScreen();
}

int32_t ModuleVideoRenderImpl::GetScreenResolution(
                                                         uint32_t& screenWidth,
                                                         uint32_t& screenHeight) const
{
    CriticalSectionScoped cs(&_moduleCrit);

    if (!_ptrRenderer)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                     "%s: No renderer", __FUNCTION__);
        return false;
    }
    return _ptrRenderer->GetScreenResolution(screenWidth, screenHeight);
}

uint32_t ModuleVideoRenderImpl::RenderFrameRate(
                                                      const uint32_t streamId)
{
    CriticalSectionScoped cs(&_moduleCrit);

    if (!_ptrRenderer)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                     "%s: No renderer", __FUNCTION__);
        return false;
    }
    return _ptrRenderer->RenderFrameRate(streamId);
}

int32_t ModuleVideoRenderImpl::SetStreamCropping(
                                                       const uint32_t streamId,
                                                       const float left,
                                                       const float top,
                                                       const float right,
                                                       const float bottom)
{
    CriticalSectionScoped cs(&_moduleCrit);

    if (!_ptrRenderer)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                     "%s: No renderer", __FUNCTION__);
        return false;
    }
    return _ptrRenderer->SetStreamCropping(streamId, left, top, right, bottom);
}

int32_t ModuleVideoRenderImpl::SetTransparentBackground(const bool enable)
{
    CriticalSectionScoped cs(&_moduleCrit);

    if (!_ptrRenderer)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                     "%s: No renderer", __FUNCTION__);
        return false;
    }
    return _ptrRenderer->SetTransparentBackground(enable);
}

int32_t ModuleVideoRenderImpl::FullScreenRender(void* window, const bool enable)
{
    return -1;
}

int32_t ModuleVideoRenderImpl::SetText(
                                             const uint8_t textId,
                                             const uint8_t* text,
                                             const int32_t textLength,
                                             const uint32_t textColorRef,
                                             const uint32_t backgroundColorRef,
                                             const float left, const float top,
                                             const float right,
                                             const float bottom)
{
    CriticalSectionScoped cs(&_moduleCrit);

    if (!_ptrRenderer)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                     "%s: No renderer", __FUNCTION__);
        return -1;
    }
    return _ptrRenderer->SetText(textId, text, textLength, textColorRef,
                                 backgroundColorRef, left, top, right, bottom);
}

int32_t ModuleVideoRenderImpl::SetBitmap(const void* bitMap,
                                         const uint8_t pictureId,
                                         const void* colorKey,
                                         const float left,
                                         const float top,
                                         const float right,
                                         const float bottom)
{
    CriticalSectionScoped cs(&_moduleCrit);

    if (!_ptrRenderer)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                     "%s: No renderer", __FUNCTION__);
        return -1;
    }
    return _ptrRenderer->SetBitmap(bitMap, pictureId, colorKey, left, top,
                                   right, bottom);
}

int32_t ModuleVideoRenderImpl::SetExpectedRenderDelay(
    uint32_t stream_id, int32_t delay_ms) {
  CriticalSectionScoped cs(&_moduleCrit);

  if (!_ptrRenderer) {
    WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                 "%s: No renderer", __FUNCTION__);
    return false;
  }

  IncomingVideoStreamMap::const_iterator item =
      _streamRenderMap.find(stream_id);
  if (item == _streamRenderMap.end()) {
    // This stream doesn't exist
    WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                 "%s(%u, %d): stream doesn't exist", __FUNCTION__, stream_id,
                 delay_ms);
    return -1;
  }

  assert(item->second != NULL);
  return item->second->SetExpectedRenderDelay(delay_ms);
}

int32_t ModuleVideoRenderImpl::ConfigureRenderer(
                                                       const uint32_t streamId,
                                                       const unsigned int zOrder,
                                                       const float left,
                                                       const float top,
                                                       const float right,
                                                       const float bottom)
{
    CriticalSectionScoped cs(&_moduleCrit);

    if (!_ptrRenderer)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                     "%s: No renderer", __FUNCTION__);
        return false;
    }
    return _ptrRenderer->ConfigureRenderer(streamId, zOrder, left, top, right,
                                           bottom);
}

int32_t ModuleVideoRenderImpl::SetStartImage(
    const uint32_t streamId,
    const I420VideoFrame& videoFrame)
{
    CriticalSectionScoped cs(&_moduleCrit);

    if (!_ptrRenderer)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                     "%s: No renderer", __FUNCTION__);
        return -1;
    }

    IncomingVideoStreamMap::const_iterator item =
        _streamRenderMap.find(streamId);
    if (item == _streamRenderMap.end())
    {
        // This stream doesn't exist
        WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                     "%s: stream doesn't exist", __FUNCTION__);
        return -1;
    }
    assert (item->second != NULL);
    return item->second->SetStartImage(videoFrame);

}

int32_t ModuleVideoRenderImpl::SetTimeoutImage(
    const uint32_t streamId,
    const I420VideoFrame& videoFrame,
    const uint32_t timeout)
{
    CriticalSectionScoped cs(&_moduleCrit);

    if (!_ptrRenderer)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                     "%s: No renderer", __FUNCTION__);
        return -1;
    }

    IncomingVideoStreamMap::const_iterator item =
        _streamRenderMap.find(streamId);
    if (item == _streamRenderMap.end())
    {
        // This stream doesn't exist
        WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                     "%s: stream doesn't exist", __FUNCTION__);
        return -1;
    }
    assert(item->second != NULL);
    return item->second->SetTimeoutImage(videoFrame, timeout);
}

}  // namespace webrtc
