/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_render_linux_impl.h"

#include "critical_section_wrapper.h"
#include "trace.h"
#include "video_x11_render.h"

#include <X11/Xlib.h>

namespace webrtc {

VideoRenderLinuxImpl::VideoRenderLinuxImpl(
                                           const WebRtc_Word32 id,
                                           const VideoRenderType videoRenderType,
                                           void* window, const bool fullscreen) :
            _id(id),
            _renderLinuxCritsect(
                                 *CriticalSectionWrapper::CreateCriticalSection()),
            _ptrWindow(window), _fullscreen(fullscreen), _ptrX11Render(NULL),
            _renderType(videoRenderType)
{
}

VideoRenderLinuxImpl::~VideoRenderLinuxImpl()
{
    if (_ptrX11Render)
        delete _ptrX11Render;

    delete &_renderLinuxCritsect;
}

WebRtc_Word32 VideoRenderLinuxImpl::Init()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, _id, "%s",
                 __FUNCTION__);

    CriticalSectionScoped cs(&_renderLinuxCritsect);
    _ptrX11Render = new VideoX11Render((Window) _ptrWindow);
    if (!_ptrX11Render)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                     "%s",
                     "Failed to create instance of VideoX11Render object");
        return -1;
    }
    int retVal = _ptrX11Render->Init();
    if (retVal == -1)
    {
        return -1;
    }

    return 0;

}

WebRtc_Word32 VideoRenderLinuxImpl::ChangeUniqueId(const WebRtc_Word32 id)
{
    CriticalSectionScoped cs(&_renderLinuxCritsect);

    _id = id;
    return 0;
}

WebRtc_Word32 VideoRenderLinuxImpl::ChangeWindow(void* window)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, _id, "%s",
                 __FUNCTION__);

    CriticalSectionScoped cs(&_renderLinuxCritsect);
    _ptrWindow = window;

    if (_ptrX11Render)
    {
        return _ptrX11Render->ChangeWindow((Window) window);
    }

    return -1;
}

VideoRenderCallback* VideoRenderLinuxImpl::AddIncomingRenderStream(
                                                                       const WebRtc_UWord32 streamId,
                                                                       const WebRtc_UWord32 zOrder,
                                                                       const float left,
                                                                       const float top,
                                                                       const float right,
                                                                       const float bottom)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, _id, "%s",
                 __FUNCTION__);
    CriticalSectionScoped cs(&_renderLinuxCritsect);

    VideoRenderCallback* renderCallback = NULL;
    if (_ptrX11Render)
    {
        VideoX11Channel* renderChannel =
                _ptrX11Render->CreateX11RenderChannel(streamId, zOrder, left,
                                                      top, right, bottom);
        if (!renderChannel)
        {
            WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                         "Render channel creation failed for stream id: %d",
                         streamId);
            return NULL;
        }
        renderCallback = (VideoRenderCallback *) renderChannel;
    }
    else
    {
        WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                     "_ptrX11Render is NULL");
        return NULL;
    }
    return renderCallback;
}

WebRtc_Word32 VideoRenderLinuxImpl::DeleteIncomingRenderStream(
                                                               const WebRtc_UWord32 streamId)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, _id, "%s",
                 __FUNCTION__);
    CriticalSectionScoped cs(&_renderLinuxCritsect);

    if (_ptrX11Render)
    {
        return _ptrX11Render->DeleteX11RenderChannel(streamId);
    }
    return -1;
}

WebRtc_Word32 VideoRenderLinuxImpl::GetIncomingRenderStreamProperties(
                                                                      const WebRtc_UWord32 streamId,
                                                                      WebRtc_UWord32& zOrder,
                                                                      float& left,
                                                                      float& top,
                                                                      float& right,
                                                                      float& bottom) const
{
    WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, _id, "%s",
                 __FUNCTION__);
    CriticalSectionScoped cs(&_renderLinuxCritsect);

    if (_ptrX11Render)
    {
        return _ptrX11Render->GetIncomingStreamProperties(streamId, zOrder,
                                                          left, top, right,
                                                          bottom);
    }
    return -1;
}

WebRtc_Word32 VideoRenderLinuxImpl::StartRender()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, _id, "%s",
                 __FUNCTION__);
    return 0;
}

WebRtc_Word32 VideoRenderLinuxImpl::StopRender()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, _id, "%s",
                 __FUNCTION__);
    return 0;
}

VideoRenderType VideoRenderLinuxImpl::RenderType()
{
    return kRenderX11;
}

RawVideoType VideoRenderLinuxImpl::PerferedVideoType()
{
    return kVideoI420;
}

bool VideoRenderLinuxImpl::FullScreen()
{
    return false;
}

WebRtc_Word32 VideoRenderLinuxImpl::GetGraphicsMemory(
                                                      WebRtc_UWord64& /*totalGraphicsMemory*/,
                                                      WebRtc_UWord64& /*availableGraphicsMemory*/) const
{
    WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                 "%s - not supported on Linux", __FUNCTION__);
    return -1;
}

WebRtc_Word32 VideoRenderLinuxImpl::GetScreenResolution(
                                                        WebRtc_UWord32& /*screenWidth*/,
                                                        WebRtc_UWord32& /*screenHeight*/) const
{
    return -1;
}

WebRtc_UWord32 VideoRenderLinuxImpl::RenderFrameRate(const WebRtc_UWord32 /*streamId*/)
{
    return -1;
}

WebRtc_Word32 VideoRenderLinuxImpl::SetStreamCropping(
                                                      const WebRtc_UWord32 /*streamId*/,
                                                      const float /*left*/,
                                                      const float /*top*/,
                                                      const float /*right*/,
                                                      const float /*bottom*/)
{
    WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                 "%s - not supported on Linux", __FUNCTION__);
    return -1;
}

WebRtc_Word32 VideoRenderLinuxImpl::SetTransparentBackground(const bool /*enable*/)
{
    WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                 "%s - not supported on Linux", __FUNCTION__);
    return -1;
}

WebRtc_Word32 VideoRenderLinuxImpl::ConfigureRenderer(
                                                      const WebRtc_UWord32 streamId,
                                                      const unsigned int zOrder,
                                                      const float left,
                                                      const float top,
                                                      const float right,
                                                      const float bottom)
{
    WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                 "%s - not supported on Linux", __FUNCTION__);
    return -1;
}

WebRtc_Word32 VideoRenderLinuxImpl::SetText(
                                            const WebRtc_UWord8 textId,
                                            const WebRtc_UWord8* text,
                                            const WebRtc_Word32 textLength,
                                            const WebRtc_UWord32 textColorRef,
                                            const WebRtc_UWord32 backgroundColorRef,
                                            const float left, const float top,
                                            const float rigth,
                                            const float bottom)
{
    WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                 "%s - not supported on Linux", __FUNCTION__);
    return -1;
}

WebRtc_Word32 VideoRenderLinuxImpl::SetBitmap(const void* bitMap,
                                              const WebRtc_UWord8 pictureId,
                                              const void* colorKey,
                                              const float left,
                                              const float top,
                                              const float right,
                                              const float bottom)
{
    WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                 "%s - not supported on Linux", __FUNCTION__);
    return -1;
}

} //namespace webrtc

