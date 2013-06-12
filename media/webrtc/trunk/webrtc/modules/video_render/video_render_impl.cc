/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_render_impl.h"
#include "engine_configurations.h"
#include "critical_section_wrapper.h"
#include "video_render_defines.h"
#include "trace.h"
#include "incoming_video_stream.h"
#include "webrtc/modules/video_render/i_video_render.h"

#include <cassert>

#ifdef WEBRTC_INCLUDE_INTERNAL_VIDEO_RENDER

#if defined (_WIN32)
#include "windows/video_render_windows_impl.h"
#define STANDARD_RENDERING kRenderWindows

// WEBRTC_IOS should go before WEBRTC_MAC because WEBRTC_MAC
// gets defined if WEBRTC_IOS is defined
#elif defined(WEBRTC_IOS)
#if defined(IPHONE_GLES_RENDERING)
#define STANDARD_RENDERING kRenderiPhone
#include "iPhone/video_render_iphone_impl.h"
#endif

#elif defined(WEBRTC_MAC)
#if defined(COCOA_RENDERING)
#define STANDARD_RENDERING kRenderCocoa
#include "mac/video_render_mac_cocoa_impl.h"
#elif defined(CARBON_RENDERING)
#define STANDARD_RENDERING kRenderCarbon
#include "mac/video_render_mac_carbon_impl.h"
#endif

#elif defined(WEBRTC_ANDROID)
#include "android/video_render_android_impl.h"
#include "android/video_render_android_surface_view.h"
#include "android/video_render_android_native_opengl2.h"
#define STANDARD_RENDERING	kRenderAndroid

#elif defined(WEBRTC_LINUX)
#include "linux/video_render_linux_impl.h"
#define STANDARD_RENDERING kRenderX11

#else
//Other platforms
#endif

#endif  // WEBRTC_INCLUDE_INTERNAL_VIDEO_RENDER

// For external rendering
#include "external/video_render_external_impl.h"
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
    _ptrWindow(window), _fullScreen(fullscreen), _ptrRenderer(NULL),
    _streamRenderMap(*(new MapWrapper()))
{

    // Create platform specific renderer
    switch (videoRenderType)
    {
#ifdef WEBRTC_INCLUDE_INTERNAL_VIDEO_RENDER

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
        case kRenderiPhone:
        {
            VideoRenderIPhoneImpl* ptrRenderer = new VideoRenderIPhoneImpl(_id, videoRenderType, window, _fullScreen);
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

#endif  // WEBRTC_INCLUDE_INTERNAL_VIDEO_RENDER
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

    while (_streamRenderMap.Size() > 0)
    {
        MapItem* item = _streamRenderMap.First();
        IncomingVideoStream* ptrIncomingStream =
                static_cast<IncomingVideoStream*> (item->GetItem());
        assert(ptrIncomingStream != NULL);
        delete ptrIncomingStream;
        _streamRenderMap.Erase(item);
    }
    delete &_streamRenderMap;

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
#ifdef WEBRTC_INCLUDE_INTERNAL_VIDEO_RENDER

#if defined(_WIN32)
            case kRenderWindows:
            {
                VideoRenderWindowsImpl* ptrRenderer = reinterpret_cast<VideoRenderWindowsImpl*>(_ptrRenderer);
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

#elif defined(WEBRTC_IOS)
            case kRenderiPhone:
            break;

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

#endif  // WEBRTC_INCLUDE_INTERNAL_VIDEO_RENDER

            default:
                // Error...
                break;
        }
    }
}

int32_t ModuleVideoRenderImpl::ChangeUniqueId(const int32_t id)
{

    CriticalSectionScoped cs(&_moduleCrit);

    _id = id;

    if (_ptrRenderer)
    {
        _ptrRenderer->ChangeUniqueId(_id);
    }

    return 0;
}

int32_t ModuleVideoRenderImpl::TimeUntilNextProcess()
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

#ifdef WEBRTC_INCLUDE_INTERNAL_VIDEO_RENDER

#if defined(WEBRTC_IOS) // WEBRTC_IOS must go before WEBRTC_MAC
    _ptrRenderer = NULL;
    delete _ptrRenderer;

    VideoRenderIPhoneImpl* ptrRenderer;
    ptrRenderer = new VideoRenderIPhoneImpl(_id, kRenderiPhone, window, _fullScreen);
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

#else  // WEBRTC_INCLUDE_INTERNAL_VIDEO_RENDER
    return -1;
#endif
}

int32_t ModuleVideoRenderImpl::Id()
{
    CriticalSectionScoped cs(&_moduleCrit);
    return _id;
}

uint32_t ModuleVideoRenderImpl::GetIncomingFrameRate(
                                                           const uint32_t streamId)
{
    CriticalSectionScoped cs(&_moduleCrit);

    MapItem* mapItem = _streamRenderMap.Find(streamId);
    if (mapItem == NULL)
    {
        // This stream doesn't exist
        WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                     "%s: stream doesn't exist", __FUNCTION__);
        return 0;
    }
    IncomingVideoStream* incomingStream =
            static_cast<IncomingVideoStream*> (mapItem->GetItem());
    if (incomingStream == NULL)
    {
        // This should never happen
        assert(false);
        _streamRenderMap.Erase(mapItem);
        return 0;
    }
    return incomingStream->IncomingRate();
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

    if (_streamRenderMap.Find(streamId) != NULL)
    {
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
    _streamRenderMap.Insert(streamId, ptrIncomingStream);

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

    MapItem* mapItem = _streamRenderMap.Find(streamId);
    if (!mapItem)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                     "%s: stream doesn't exist", __FUNCTION__);
        return -1;
    }

    IncomingVideoStream* ptrIncomingStream =
            static_cast<IncomingVideoStream*> (mapItem->GetItem());
    delete ptrIncomingStream;
    ptrIncomingStream = NULL;
    _ptrRenderer->DeleteIncomingRenderStream(streamId);
    _streamRenderMap.Erase(mapItem);

    return 0;
}

int32_t ModuleVideoRenderImpl::AddExternalRenderCallback(
                                                               const uint32_t streamId,
                                                               VideoRenderCallback* renderObject)
{
    CriticalSectionScoped cs(&_moduleCrit);

    MapItem* mapItem = _streamRenderMap.Find(streamId);
    if (!mapItem)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                     "%s: stream doesn't exist", __FUNCTION__);
        return -1;
    }

    IncomingVideoStream* ptrIncomingStream =
            static_cast<IncomingVideoStream*> (mapItem->GetItem());
    if (!ptrIncomingStream) {
        WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                     "%s: could not get stream", __FUNCTION__);
        return -1;
    }
    return ptrIncomingStream->SetExternalCallback(renderObject);
}

int32_t ModuleVideoRenderImpl::GetIncomingRenderStreamProperties(
                                                                       const uint32_t streamId,
                                                                       uint32_t& zOrder,
                                                                       float& left,
                                                                       float& top,
                                                                       float& right,
                                                                       float& bottom) const
{
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

    return (uint32_t) _streamRenderMap.Size();
}

bool ModuleVideoRenderImpl::HasIncomingRenderStream(
                                                    const uint32_t streamId) const
{
    CriticalSectionScoped cs(&_moduleCrit);

    bool hasStream = false;
    if (_streamRenderMap.Find(streamId) != NULL)
    {
        hasStream = true;
    }
    return hasStream;
}

int32_t ModuleVideoRenderImpl::RegisterRawFrameCallback(
                                                              const uint32_t streamId,
                                                              VideoRenderCallback* callbackObj)
{
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
    MapItem* item = _streamRenderMap.Find(streamId);
    if (item == NULL)
    {
        return -1;
    }

    IncomingVideoStream* incomingStream =
            static_cast<IncomingVideoStream*> (item->GetItem());
    if (incomingStream->Start() == -1)
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
    MapItem* item = _streamRenderMap.Find(streamId);
    if (item == NULL)
    {
        return -1;
    }

    IncomingVideoStream* incomingStream =
            static_cast<IncomingVideoStream*> (item->GetItem());
    if (incomingStream->Stop() == -1)
    {
        return -1;
    }

    return 0;
}

int32_t ModuleVideoRenderImpl::ResetRender()
{
    CriticalSectionScoped cs(&_moduleCrit);

    int32_t error = 0;

    // Loop through all incoming streams and stop them
    MapItem* item = _streamRenderMap.First();
    while (item)
    {
        IncomingVideoStream* incomingStream =
                static_cast<IncomingVideoStream*> (item->GetItem());
        if (incomingStream->Reset() == -1)
        {
            error = -1;
        }
        item = _streamRenderMap.Next(item);
    }
    return error;
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

int32_t ModuleVideoRenderImpl::GetLastRenderedFrame(
    const uint32_t streamId,
    I420VideoFrame &frame) const
{
    CriticalSectionScoped cs(&_moduleCrit);

    if (!_ptrRenderer)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                     "%s: No renderer", __FUNCTION__);
        return -1;
    }

    MapItem *item = _streamRenderMap.Find(streamId);
    if (item == NULL)
    {
        // This stream doesn't exist
        WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                     "%s: stream doesn't exist", __FUNCTION__);
        return 0;
    }
    IncomingVideoStream* incomingStream =
            static_cast<IncomingVideoStream*> (item->GetItem());
    if (incomingStream == NULL)
    {
        // This should never happen
        assert(false);
        _streamRenderMap.Erase(item);
        return 0;
    }
    return incomingStream->GetLastRenderedFrame(frame);
}

int32_t ModuleVideoRenderImpl::SetExpectedRenderDelay(
    uint32_t stream_id, int32_t delay_ms) {
  CriticalSectionScoped cs(&_moduleCrit);

  if (!_ptrRenderer) {
    WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                 "%s: No renderer", __FUNCTION__);
    return false;
  }

  MapItem *item = _streamRenderMap.Find(stream_id);
  if (item == NULL) {
    // This stream doesn't exist
    WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                 "%s(%u, %d): stream doesn't exist", __FUNCTION__, stream_id,
                 delay_ms);
    return -1;
  }

  IncomingVideoStream* incoming_stream =
      static_cast<IncomingVideoStream*> (item->GetItem());
  if (incoming_stream == NULL) {
      // This should never happen
      assert(false);
      _streamRenderMap.Erase(item);
      return 0;
  }

  return incoming_stream->SetExpectedRenderDelay(delay_ms);
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

    MapItem *item = _streamRenderMap.Find(streamId);
    if (item == NULL)
    {
        // This stream doesn't exist
        WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                     "%s: stream doesn't exist", __FUNCTION__);
        return -1;
    }
    IncomingVideoStream* incomingStream =
            static_cast<IncomingVideoStream*> (item->GetItem());
    if (incomingStream == NULL)
    {
        // This should never happen
        assert(false);
        _streamRenderMap.Erase(item);
        return 0;
    }
    return incomingStream->SetStartImage(videoFrame);

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

    MapItem *item = _streamRenderMap.Find(streamId);
    if (item == NULL)
    {
        // This stream doesn't exist
        WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                     "%s: stream doesn't exist", __FUNCTION__);
        return -1;
    }
    IncomingVideoStream* incomingStream =
            static_cast<IncomingVideoStream*> (item->GetItem());
    if (incomingStream == NULL)
    {
        // This should never happen
        assert(false);
        _streamRenderMap.Erase(item);
        return 0;
    }
    return incomingStream->SetTimeoutImage(videoFrame, timeout);
}

int32_t ModuleVideoRenderImpl::MirrorRenderStream(const int renderId,
                                                  const bool enable,
                                                  const bool mirrorXAxis,
                                                  const bool mirrorYAxis)
{
    CriticalSectionScoped cs(&_moduleCrit);

    if (!_ptrRenderer)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                     "%s: No renderer", __FUNCTION__);
        return -1;
    }

    MapItem *item = _streamRenderMap.Find(renderId);
    if (item == NULL)
    {
        // This stream doesn't exist
        WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                     "%s: stream doesn't exist", __FUNCTION__);
        return 0;
    }
    IncomingVideoStream* incomingStream =
            static_cast<IncomingVideoStream*> (item->GetItem());
    if (incomingStream == NULL)
    {
        // This should never happen
        assert(false);
        _streamRenderMap.Erase(item);
        return 0;
    }

    return incomingStream->EnableMirroring(enable, mirrorXAxis, mirrorYAxis);
}

} //namespace webrtc
