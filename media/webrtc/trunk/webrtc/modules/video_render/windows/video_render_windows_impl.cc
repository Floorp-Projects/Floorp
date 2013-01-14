/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "engine_configurations.h"
#include "video_render_windows_impl.h"

#include "critical_section_wrapper.h"
#include "trace.h"
#ifdef DIRECT3D9_RENDERING
#include "video_render_direct3d9.h"
#endif

#include <tchar.h>

namespace webrtc {

VideoRenderWindowsImpl::VideoRenderWindowsImpl(const WebRtc_Word32 id,
    const VideoRenderType videoRenderType, void* window, const bool fullscreen)
    : _id(id),
      _renderWindowsCritsect(*CriticalSectionWrapper::CreateCriticalSection()),
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

WebRtc_Word32 VideoRenderWindowsImpl::Init()
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

WebRtc_Word32 VideoRenderWindowsImpl::ChangeUniqueId(const WebRtc_Word32 id)
{
    CriticalSectionScoped cs(&_renderWindowsCritsect);
    _id = id;
    return 0;
}

WebRtc_Word32 VideoRenderWindowsImpl::ChangeWindow(void* window)
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
VideoRenderWindowsImpl::AddIncomingRenderStream(const WebRtc_UWord32 streamId,
                                                const WebRtc_UWord32 zOrder,
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

WebRtc_Word32 VideoRenderWindowsImpl::DeleteIncomingRenderStream(
                                                                 const WebRtc_UWord32 streamId)
{
    CriticalSectionScoped cs(&_renderWindowsCritsect);
    WebRtc_Word32 error = -1;
    if (!_ptrRendererWin)
    {
    }
    else
    {
        error = _ptrRendererWin->DeleteChannel(streamId);
    }
    return error;
}

WebRtc_Word32 VideoRenderWindowsImpl::GetIncomingRenderStreamProperties(
                                                                        const WebRtc_UWord32 streamId,
                                                                        WebRtc_UWord32& zOrder,
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

    WebRtc_Word32 error = -1;
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

WebRtc_Word32 VideoRenderWindowsImpl::StartRender()
{
    CriticalSectionScoped cs(&_renderWindowsCritsect);
    WebRtc_Word32 error = -1;
    if (!_ptrRendererWin)
    {
    }
    else
    {
        error = _ptrRendererWin->StartRender();
    }
    return error;
}

WebRtc_Word32 VideoRenderWindowsImpl::StopRender()
{
    CriticalSectionScoped cs(&_renderWindowsCritsect);
    WebRtc_Word32 error = -1;
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

WebRtc_Word32 VideoRenderWindowsImpl::GetGraphicsMemory(
                                                        WebRtc_UWord64& totalGraphicsMemory,
                                                        WebRtc_UWord64& availableGraphicsMemory) const
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

WebRtc_Word32 VideoRenderWindowsImpl::GetScreenResolution(
                                                          WebRtc_UWord32& screenWidth,
                                                          WebRtc_UWord32& screenHeight) const
{
    CriticalSectionScoped cs(&_renderWindowsCritsect);
    screenWidth = 0;
    screenHeight = 0;
    return 0;
}

WebRtc_UWord32 VideoRenderWindowsImpl::RenderFrameRate(
                                                       const WebRtc_UWord32 streamId)
{
    CriticalSectionScoped cs(&_renderWindowsCritsect);
    return 0;
}

WebRtc_Word32 VideoRenderWindowsImpl::SetStreamCropping(
                                                        const WebRtc_UWord32 streamId,
                                                        const float left,
                                                        const float top,
                                                        const float right,
                                                        const float bottom)
{
    CriticalSectionScoped cs(&_renderWindowsCritsect);
    WebRtc_Word32 error = -1;
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

WebRtc_Word32 VideoRenderWindowsImpl::ConfigureRenderer(
                                                        const WebRtc_UWord32 streamId,
                                                        const unsigned int zOrder,
                                                        const float left,
                                                        const float top,
                                                        const float right,
                                                        const float bottom)
{
    CriticalSectionScoped cs(&_renderWindowsCritsect);
    WebRtc_Word32 error = -1;
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

WebRtc_Word32 VideoRenderWindowsImpl::SetTransparentBackground(
                                                               const bool enable)
{
    CriticalSectionScoped cs(&_renderWindowsCritsect);
    WebRtc_Word32 error = -1;
    if (!_ptrRendererWin)
    {
    }
    else
    {
        error = _ptrRendererWin->SetTransparentBackground(enable);
    }
    return error;
}

WebRtc_Word32 VideoRenderWindowsImpl::SetText(
                                              const WebRtc_UWord8 textId,
                                              const WebRtc_UWord8* text,
                                              const WebRtc_Word32 textLength,
                                              const WebRtc_UWord32 textColorRef,
                                              const WebRtc_UWord32 backgroundColorRef,
                                              const float left,
                                              const float top,
                                              const float right,
                                              const float bottom)
{
    CriticalSectionScoped cs(&_renderWindowsCritsect);
    WebRtc_Word32 error = -1;
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

WebRtc_Word32 VideoRenderWindowsImpl::SetBitmap(const void* bitMap,
                                                const WebRtc_UWord8 pictureId,
                                                const void* colorKey,
                                                const float left,
                                                const float top,
                                                const float right,
                                                const float bottom)
{
    CriticalSectionScoped cs(&_renderWindowsCritsect);
    WebRtc_Word32 error = -1;
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

} //namespace webrtc

