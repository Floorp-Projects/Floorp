/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_render_external_impl.h"

namespace webrtc {

VideoRenderExternalImpl::VideoRenderExternalImpl(
                                                 const WebRtc_Word32 id,
                                                 const VideoRenderType videoRenderType,
                                                 void* window,
                                                 const bool fullscreen) :
    _id(id), _critSect(*CriticalSectionWrapper::CreateCriticalSection()),
            _fullscreen(fullscreen)
{
}

VideoRenderExternalImpl::~VideoRenderExternalImpl()
{
    delete &_critSect;
}

WebRtc_Word32 VideoRenderExternalImpl::Init()
{
    return 0;
}

WebRtc_Word32 VideoRenderExternalImpl::ChangeUniqueId(const WebRtc_Word32 id)
{
    CriticalSectionScoped cs(&_critSect);
    _id = id;
    return 0;
}

WebRtc_Word32 VideoRenderExternalImpl::ChangeWindow(void* window)
{
    CriticalSectionScoped cs(&_critSect);
    return 0;
}

VideoRenderCallback*
VideoRenderExternalImpl::AddIncomingRenderStream(const WebRtc_UWord32 streamId,
                                                 const WebRtc_UWord32 zOrder,
                                                 const float left,
                                                 const float top,
                                                 const float right,
                                                 const float bottom)
{
    CriticalSectionScoped cs(&_critSect);
    return this;
}

WebRtc_Word32 VideoRenderExternalImpl::DeleteIncomingRenderStream(
                                                                  const WebRtc_UWord32 streamId)
{
    CriticalSectionScoped cs(&_critSect);
    return 0;
}

WebRtc_Word32 VideoRenderExternalImpl::GetIncomingRenderStreamProperties(
                                                                         const WebRtc_UWord32 streamId,
                                                                         WebRtc_UWord32& zOrder,
                                                                         float& left,
                                                                         float& top,
                                                                         float& right,
                                                                         float& bottom) const
{
    CriticalSectionScoped cs(&_critSect);

    zOrder = 0;
    left = 0;
    top = 0;
    right = 0;
    bottom = 0;

    return 0;
}

WebRtc_Word32 VideoRenderExternalImpl::StartRender()
{
    CriticalSectionScoped cs(&_critSect);
    return 0;
}

WebRtc_Word32 VideoRenderExternalImpl::StopRender()
{
    CriticalSectionScoped cs(&_critSect);
    return 0;
}

VideoRenderType VideoRenderExternalImpl::RenderType()
{
    return kRenderExternal;
}

RawVideoType VideoRenderExternalImpl::PerferedVideoType()
{
    return kVideoI420;
}

bool VideoRenderExternalImpl::FullScreen()
{
    CriticalSectionScoped cs(&_critSect);
    return _fullscreen;
}

WebRtc_Word32 VideoRenderExternalImpl::GetGraphicsMemory(
                                                         WebRtc_UWord64& totalGraphicsMemory,
                                                         WebRtc_UWord64& availableGraphicsMemory) const
{
    totalGraphicsMemory = 0;
    availableGraphicsMemory = 0;
    return -1;
}

WebRtc_Word32 VideoRenderExternalImpl::GetScreenResolution(
                                                           WebRtc_UWord32& screenWidth,
                                                           WebRtc_UWord32& screenHeight) const
{
    CriticalSectionScoped cs(&_critSect);
    screenWidth = 0;
    screenHeight = 0;
    return 0;
}

WebRtc_UWord32 VideoRenderExternalImpl::RenderFrameRate(
                                                        const WebRtc_UWord32 streamId)
{
    CriticalSectionScoped cs(&_critSect);
    return 0;
}

WebRtc_Word32 VideoRenderExternalImpl::SetStreamCropping(
                                                         const WebRtc_UWord32 streamId,
                                                         const float left,
                                                         const float top,
                                                         const float right,
                                                         const float bottom)
{
    CriticalSectionScoped cs(&_critSect);
    return 0;
}

WebRtc_Word32 VideoRenderExternalImpl::ConfigureRenderer(
                                                         const WebRtc_UWord32 streamId,
                                                         const unsigned int zOrder,
                                                         const float left,
                                                         const float top,
                                                         const float right,
                                                         const float bottom)
{
    CriticalSectionScoped cs(&_critSect);
    return 0;
}

WebRtc_Word32 VideoRenderExternalImpl::SetTransparentBackground(
                                                                const bool enable)
{
    CriticalSectionScoped cs(&_critSect);
    return 0;
}

WebRtc_Word32 VideoRenderExternalImpl::SetText(
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
    CriticalSectionScoped cs(&_critSect);
    return 0;
}

WebRtc_Word32 VideoRenderExternalImpl::SetBitmap(const void* bitMap,
                                                 const WebRtc_UWord8 pictureId,
                                                 const void* colorKey,
                                                 const float left,
                                                 const float top,
                                                 const float right,
                                                 const float bottom)
{
    CriticalSectionScoped cs(&_critSect);
    return 0;
}

// VideoRenderCallback
WebRtc_Word32 VideoRenderExternalImpl::RenderFrame(
                                                   const WebRtc_UWord32 streamId,
                                                   I420VideoFrame& videoFrame)
{
    return 0;
}
} //namespace webrtc

