/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/engine_configurations.h"
#include "webrtc/modules/video_render/windows/video_render_windows_impl.h"

#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/trace.h"
#ifdef DIRECT3D9_RENDERING
#include "webrtc/modules/video_render/windows/video_render_direct3d9.h"
#endif

#include <tchar.h>

namespace webrtc {

VideoRenderWindowsImpl::VideoRenderWindowsImpl(const int32_t id,
    const VideoRenderType videoRenderType, void* window, const bool fullscreen)
    : _renderWindowsCritsect(*CriticalSectionWrapper::CreateCriticalSection()),
      _prtWindow(window),
      _fullscreen(fullscreen),
      _renderMethod(kVideoRenderWinD3D9),
      _ptrRendererWin(NULL) {
}

VideoRenderWindowsImpl::~VideoRenderWindowsImpl()
{
    delete &_renderWindowsCritsect;
    if (_ptrRendererWin)
    {
        delete _ptrRendererWin;
        _ptrRendererWin = NULL;
    }
}

int32_t VideoRenderWindowsImpl::Init()
{
    // Create the win renderer
    switch (_renderMethod)
    {
        case kVideoRenderWinD3D9:
        {
#ifdef DIRECT3D9_RENDERING
            VideoRenderDirect3D9* ptrRenderer;
            ptrRenderer = new VideoRenderDirect3D9(NULL, (HWND) _prtWindow, _fullscreen);
            if (ptrRenderer == NULL)
            {
                break;
            }
            _ptrRendererWin = reinterpret_cast<IVideoRenderWin*>(ptrRenderer);
#else
            return NULL;
#endif  //DIRECT3D9_RENDERING
        }
            break;
        default:
            break;
    }

    //Init renderer
    if (_ptrRendererWin)
        return _ptrRendererWin->Init();
    else
        return -1;
}

int32_t VideoRenderWindowsImpl::ChangeWindow(void* window)
{
    CriticalSectionScoped cs(&_renderWindowsCritsect);
    if (!_ptrRendererWin)
    {
        return -1;
    }
    else
    {
        return _ptrRendererWin->ChangeWindow(window);
    }
}

VideoRenderCallback*
VideoRenderWindowsImpl::AddIncomingRenderStream(const uint32_t streamId,
                                                const uint32_t zOrder,
                                                const float left,
                                                const float top,
                                                const float right,
                                                const float bottom)
{
    CriticalSectionScoped cs(&_renderWindowsCritsect);
    VideoRenderCallback* renderCallback = NULL;

    if (!_ptrRendererWin)
    {
    }
    else
    {
        renderCallback = _ptrRendererWin->CreateChannel(streamId, zOrder, left,
                                                        top, right, bottom);
    }

    return renderCallback;
}

int32_t VideoRenderWindowsImpl::DeleteIncomingRenderStream(
                                                                 const uint32_t streamId)
{
    CriticalSectionScoped cs(&_renderWindowsCritsect);
    int32_t error = -1;
    if (!_ptrRendererWin)
    {
    }
    else
    {
        error = _ptrRendererWin->DeleteChannel(streamId);
    }
    return error;
}

int32_t VideoRenderWindowsImpl::GetIncomingRenderStreamProperties(
                                                                        const uint32_t streamId,
                                                                        uint32_t& zOrder,
                                                                        float& left,
                                                                        float& top,
                                                                        float& right,
                                                                        float& bottom) const
{
    CriticalSectionScoped cs(&_renderWindowsCritsect);
    zOrder = 0;
    left = 0;
    top = 0;
    right = 0;
    bottom = 0;

    int32_t error = -1;
    if (!_ptrRendererWin)
    {
    }
    else
    {
        error = _ptrRendererWin->GetStreamSettings(streamId, 0, zOrder, left,
                                                   top, right, bottom);
    }
    return error;
}

int32_t VideoRenderWindowsImpl::StartRender()
{
    CriticalSectionScoped cs(&_renderWindowsCritsect);
    int32_t error = -1;
    if (!_ptrRendererWin)
    {
    }
    else
    {
        error = _ptrRendererWin->StartRender();
    }
    return error;
}

int32_t VideoRenderWindowsImpl::StopRender()
{
    CriticalSectionScoped cs(&_renderWindowsCritsect);
    int32_t error = -1;
    if (!_ptrRendererWin)
    {
    }
    else
    {
        error = _ptrRendererWin->StopRender();
    }
    return error;
}

VideoRenderType VideoRenderWindowsImpl::RenderType()
{
    return kRenderWindows;
}

RawVideoType VideoRenderWindowsImpl::PerferedVideoType()
{
    return kVideoI420;
}

bool VideoRenderWindowsImpl::FullScreen()
{
    CriticalSectionScoped cs(&_renderWindowsCritsect);
    bool fullscreen = false;
    if (!_ptrRendererWin)
    {
    }
    else
    {
        fullscreen = _ptrRendererWin->IsFullScreen();
    }
    return fullscreen;
}

int32_t VideoRenderWindowsImpl::GetGraphicsMemory(
                                                        uint64_t& totalGraphicsMemory,
                                                        uint64_t& availableGraphicsMemory) const
{
    if (_ptrRendererWin)
    {
        return _ptrRendererWin->GetGraphicsMemory(totalGraphicsMemory,
                                                  availableGraphicsMemory);
    }

    totalGraphicsMemory = 0;
    availableGraphicsMemory = 0;
    return -1;
}

int32_t VideoRenderWindowsImpl::GetScreenResolution(
                                                          uint32_t& screenWidth,
                                                          uint32_t& screenHeight) const
{
    CriticalSectionScoped cs(&_renderWindowsCritsect);
    screenWidth = 0;
    screenHeight = 0;
    return 0;
}

uint32_t VideoRenderWindowsImpl::RenderFrameRate(
                                                       const uint32_t streamId)
{
    CriticalSectionScoped cs(&_renderWindowsCritsect);
    return 0;
}

int32_t VideoRenderWindowsImpl::SetStreamCropping(
                                                        const uint32_t streamId,
                                                        const float left,
                                                        const float top,
                                                        const float right,
                                                        const float bottom)
{
    CriticalSectionScoped cs(&_renderWindowsCritsect);
    int32_t error = -1;
    if (!_ptrRendererWin)
    {
    }
    else
    {
        error = _ptrRendererWin->SetCropping(streamId, 0, left, top, right,
                                             bottom);
    }
    return error;
}

int32_t VideoRenderWindowsImpl::ConfigureRenderer(
                                                        const uint32_t streamId,
                                                        const unsigned int zOrder,
                                                        const float left,
                                                        const float top,
                                                        const float right,
                                                        const float bottom)
{
    CriticalSectionScoped cs(&_renderWindowsCritsect);
    int32_t error = -1;
    if (!_ptrRendererWin)
    {
    }
    else
    {
        error = _ptrRendererWin->ConfigureRenderer(streamId, 0, zOrder, left,
                                                   top, right, bottom);
    }

    return error;
}

int32_t VideoRenderWindowsImpl::SetTransparentBackground(
                                                               const bool enable)
{
    CriticalSectionScoped cs(&_renderWindowsCritsect);
    int32_t error = -1;
    if (!_ptrRendererWin)
    {
    }
    else
    {
        error = _ptrRendererWin->SetTransparentBackground(enable);
    }
    return error;
}

int32_t VideoRenderWindowsImpl::SetText(
                                              const uint8_t textId,
                                              const uint8_t* text,
                                              const int32_t textLength,
                                              const uint32_t textColorRef,
                                              const uint32_t backgroundColorRef,
                                              const float left,
                                              const float top,
                                              const float right,
                                              const float bottom)
{
    CriticalSectionScoped cs(&_renderWindowsCritsect);
    int32_t error = -1;
    if (!_ptrRendererWin)
    {
    }
    else
    {
        error = _ptrRendererWin->SetText(textId, text, textLength,
                                         textColorRef, backgroundColorRef,
                                         left, top, right, bottom);
    }
    return error;
}

int32_t VideoRenderWindowsImpl::SetBitmap(const void* bitMap,
                                          const uint8_t pictureId,
                                          const void* colorKey,
                                          const float left, const float top,
                                          const float right, const float bottom)
{
    CriticalSectionScoped cs(&_renderWindowsCritsect);
    int32_t error = -1;
    if (!_ptrRendererWin)
    {
    }
    else
    {
        error = _ptrRendererWin->SetBitmap(bitMap, pictureId, colorKey, left,
                                           top, right, bottom);
    }
    return error;
}

}  // namespace webrtc
