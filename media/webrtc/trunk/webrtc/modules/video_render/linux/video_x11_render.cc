/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_x11_render.h"
#include "video_x11_channel.h"

#include "critical_section_wrapper.h"
#include "trace.h"

namespace webrtc {

VideoX11Render::VideoX11Render(Window window) :
    _window(window),
            _critSect(*CriticalSectionWrapper::CreateCriticalSection())
{
}

VideoX11Render::~VideoX11Render()
{
    delete &_critSect;
}

WebRtc_Word32 VideoX11Render::Init()
{
    CriticalSectionScoped cs(&_critSect);

    _streamIdToX11ChannelMap.clear();

    return 0;
}

WebRtc_Word32 VideoX11Render::ChangeWindow(Window window)
{
    CriticalSectionScoped cs(&_critSect);
    VideoX11Channel* renderChannel = NULL;

    std::map<int, VideoX11Channel*>::iterator iter =
            _streamIdToX11ChannelMap.begin();

    while (iter != _streamIdToX11ChannelMap.end())
    {
        renderChannel = iter->second;
        if (renderChannel)
        {
            renderChannel->ChangeWindow(window);
        }
        iter++;
    }

    _window = window;

    return 0;
}

VideoX11Channel* VideoX11Render::CreateX11RenderChannel(
                                                                WebRtc_Word32 streamId,
                                                                WebRtc_Word32 zOrder,
                                                                const float left,
                                                                const float top,
                                                                const float right,
                                                                const float bottom)
{
    CriticalSectionScoped cs(&_critSect);
    VideoX11Channel* renderChannel = NULL;

    std::map<int, VideoX11Channel*>::iterator iter =
            _streamIdToX11ChannelMap.find(streamId);

    if (iter == _streamIdToX11ChannelMap.end())
    {
        renderChannel = new VideoX11Channel(streamId);
        if (!renderChannel)
        {
            WEBRTC_TRACE(
                         kTraceError,
                         kTraceVideoRenderer,
                         -1,
                         "Failed to create VideoX11Channel for streamId : %d",
                         streamId);
            return NULL;
        }
        renderChannel->Init(_window, left, top, right, bottom);
        _streamIdToX11ChannelMap[streamId] = renderChannel;
    }
    else
    {
        WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, -1,
                     "Render Channel already exists for streamId: %d", streamId);
        renderChannel = iter->second;
    }

    return renderChannel;
}

WebRtc_Word32 VideoX11Render::DeleteX11RenderChannel(WebRtc_Word32 streamId)
{
    CriticalSectionScoped cs(&_critSect);

    std::map<int, VideoX11Channel*>::iterator iter =
            _streamIdToX11ChannelMap.find(streamId);
    if (iter != _streamIdToX11ChannelMap.end())
    {
        VideoX11Channel *renderChannel = iter->second;
        if (renderChannel)
        {
            renderChannel->ReleaseWindow();
            delete renderChannel;
            renderChannel = NULL;
        }
        _streamIdToX11ChannelMap.erase(iter);
    }

    WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, -1,
                 "No VideoX11Channel object exists for stream id: %d",
                 streamId);
    return -1;
}

WebRtc_Word32 VideoX11Render::GetIncomingStreamProperties(
                                                              WebRtc_Word32 streamId,
                                                              WebRtc_UWord32& zOrder,
                                                              float& left,
                                                              float& top,
                                                              float& right,
                                                              float& bottom)
{
    CriticalSectionScoped cs(&_critSect);

    std::map<int, VideoX11Channel*>::iterator iter =
            _streamIdToX11ChannelMap.find(streamId);
    if (iter != _streamIdToX11ChannelMap.end())
    {
        VideoX11Channel *renderChannel = iter->second;
        if (renderChannel)
        {
            renderChannel->GetStreamProperties(zOrder, left, top, right, bottom);
        }
    }

    WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, -1,
                 "No VideoX11Channel object exists for stream id: %d",
                 streamId);
    return -1;
}

} //namespace webrtc

