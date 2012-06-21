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
#if defined(CARBON_RENDERING)

#include "video_render_mac_carbon_impl.h"
#include "critical_section_wrapper.h"
#include "video_render_agl.h"
#include "trace.h"
#include <AGL/agl.h>

namespace webrtc {

VideoRenderMacCarbonImpl::VideoRenderMacCarbonImpl(const WebRtc_Word32 id,
        const VideoRenderType videoRenderType,
        void* window,
        const bool fullscreen) :
_id(id),
_renderMacCarbonCritsect(*CriticalSectionWrapper::CreateCriticalSection()),
_fullScreen(fullscreen),
_ptrWindow(window)
{

    WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, _id, "Constructor %s:%d", __FUNCTION__, __LINE__);

}

VideoRenderMacCarbonImpl::~VideoRenderMacCarbonImpl()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, _id, "Destructor %s:%d", __FUNCTION__, __LINE__);
    delete &_renderMacCarbonCritsect;
}

WebRtc_Word32
VideoRenderMacCarbonImpl::Init()
{
    CriticalSectionScoped cs(&_renderMacCarbonCritsect);
    WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, _id, "%s:%d", __FUNCTION__, __LINE__);

    if (!_ptrWindow)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVideoRenderer, _id, "Constructor %s:%d", __FUNCTION__, __LINE__);
        return -1;
    }

    // We don't know if the user passed us a WindowRef or a HIViewRef, so test.
    bool referenceIsValid = false;

    // Check if it's a valid WindowRef
    //WEBRTC_TRACE(kTraceDebug, kTraceVideoRenderer, _id, "%s:%d _ptrWindowRef before WindowRef cast: %x", __FUNCTION__, __LINE__, _ptrWindowRef);
    WindowRef* windowRef = static_cast<WindowRef*>(_ptrWindow);
    //WEBRTC_TRACE(kTraceDebug, kTraceVideoRenderer, _id, "%s:%d _ptrWindowRef after cast: %x", __FUNCTION__, __LINE__, _ptrWindowRef);
    if (IsValidWindowPtr(*windowRef))
    {
        _ptrCarbonRender = new VideoRenderAGL(*windowRef, _fullScreen, _id);
        referenceIsValid = true;
        WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, _id, "%s:%d Successfully initialized CarbonRenderer with WindowRef:%x", __FUNCTION__, __LINE__, *windowRef);
    }
    else
    {
        HIViewRef* hiviewRef = static_cast<HIViewRef*>(_ptrWindow);
        if (HIViewIsValid(*hiviewRef))
        {
            _ptrCarbonRender = new VideoRenderAGL(*hiviewRef, _fullScreen, _id);
            referenceIsValid = true;
            WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, _id, "%s:%d Successfully initialized CarbonRenderer with HIViewRef:%x", __FUNCTION__, __LINE__, hiviewRef);
        }
    }

    if(!referenceIsValid)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id, "%s:%d Invalid WindowRef/HIViewRef Returning -1", __FUNCTION__, __LINE__);
        return -1;
    }

    if(!_ptrCarbonRender)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id, "%s:%d Failed to create an instance of VideoRenderAGL. Returning -1", __FUNCTION__, __LINE__);
    }

    int retVal = _ptrCarbonRender->Init();
    if (retVal == -1)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id, "%s:%d Failed to init CarbonRenderer", __FUNCTION__, __LINE__);
        return -1;
    }

    return 0;
}

WebRtc_Word32
VideoRenderMacCarbonImpl::ChangeUniqueId(const WebRtc_Word32 id)
{
    return -1;

    CriticalSectionScoped cs(&_renderMacCarbonCritsect);
    WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, _id, "%s", __FUNCTION__);
    _id = id;

    if(_ptrCarbonRender)
    {
        _ptrCarbonRender->ChangeUniqueID(_id);
    }

    return 0;
}

WebRtc_Word32
VideoRenderMacCarbonImpl::ChangeWindow(void* window)
{
    return -1;
    CriticalSectionScoped cs(&_renderMacCarbonCritsect);
    WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, _id, "%s changing ID to ", __FUNCTION__, window);

    if (window == NULL)
    {
        return -1;
    }
    _ptrWindow = window;


    _ptrWindow = window;

    return 0;
}

VideoRenderCallback*
VideoRenderMacCarbonImpl::AddIncomingRenderStream(const WebRtc_UWord32 streamId,
        const WebRtc_UWord32 zOrder,
        const float left,
        const float top,
        const float right,
        const float bottom)
{

    CriticalSectionScoped cs(&_renderMacCarbonCritsect);
    WEBRTC_TRACE(kTraceDebug, kTraceVideoRenderer, _id, "%s", __FUNCTION__);
    VideoChannelAGL* AGLChannel = NULL;

    if(!_ptrWindow)
    {
    }

    if(!AGLChannel)
    {
        AGLChannel = _ptrCocoaRender->CreateNSGLChannel(streamId, zOrder, left, top, right, bottom);
    }

    return AGLChannel;

}

WebRtc_Word32
VideoRenderMacCarbonImpl::DeleteIncomingRenderStream(const WebRtc_UWord32 streamId)
{

    WEBRTC_TRACE(kTraceDebug, kTraceVideoRenderer, _id, "%s:%d", __FUNCTION__, __LINE__);
    CriticalSectionScoped cs(&_renderMacCarbonCritsect);
    _ptrCarbonRender->DeleteAGLChannel(streamId);

    return 0;
}

WebRtc_Word32
VideoRenderMacCarbonImpl::GetIncomingRenderStreamProperties(const WebRtc_UWord32 streamId,
        WebRtc_UWord32& zOrder,
        float& left,
        float& top,
        float& right,
        float& bottom) const
{
    return -1;
    return _ptrCarbonRender->GetChannelProperties(streamId, zOrder, left, top, right, bottom);
}

WebRtc_Word32
VideoRenderMacCarbonImpl::StartRender()
{
    return _ptrCarbonRender->StartRender();
}

WebRtc_Word32
VideoRenderMacCarbonImpl::StopRender()
{
    return _ptrCarbonRender->StopRender();
}

VideoRenderType
VideoRenderMacCarbonImpl::RenderType()
{
    return kRenderCarbon;
}

RawVideoType
VideoRenderMacCarbonImpl::PerferedVideoType()
{
    return kVideoI420;
}

bool
VideoRenderMacCarbonImpl::FullScreen()
{
    return false;
}

WebRtc_Word32
VideoRenderMacCarbonImpl::GetGraphicsMemory(WebRtc_UWord64& totalGraphicsMemory,
        WebRtc_UWord64& availableGraphicsMemory) const
{
    totalGraphicsMemory = 0;
    availableGraphicsMemory = 0;
    return 0;
}

WebRtc_Word32
VideoRenderMacCarbonImpl::GetScreenResolution(WebRtc_UWord32& screenWidth,
        WebRtc_UWord32& screenHeight) const
{
    CriticalSectionScoped cs(&_renderMacCarbonCritsect);
    //NSScreen* mainScreen = [NSScreen mainScreen];

    //NSRect frame = [mainScreen frame];

    //screenWidth = frame.size.width;
    //screenHeight = frame.size.height;
    return 0;
}

WebRtc_UWord32
VideoRenderMacCarbonImpl::RenderFrameRate(const WebRtc_UWord32 streamId)
{
    CriticalSectionScoped cs(&_renderMacCarbonCritsect);
    return 0;
}

WebRtc_Word32
VideoRenderMacCarbonImpl::SetStreamCropping(const WebRtc_UWord32 streamId,
        const float left,
        const float top,
        const float right,
        const float bottom)
{
    return 0;
}

WebRtc_Word32 VideoRenderMacCarbonImpl::ConfigureRenderer(const WebRtc_UWord32 streamId,
        const unsigned int zOrder,
        const float left,
        const float top,
        const float right,
        const float bottom)
{
    return 0;
}

WebRtc_Word32
VideoRenderMacCarbonImpl::SetTransparentBackground(const bool enable)
{
    return 0;
}

WebRtc_Word32 VideoRenderMacCarbonImpl::SetText(const WebRtc_UWord8 textId,
        const WebRtc_UWord8* text,
        const WebRtc_Word32 textLength,
        const WebRtc_UWord32 textColorRef,
        const WebRtc_UWord32 backgroundColorRef,
        const float left,
        const float top,
        const float right,
        const float bottom)
{
    return 0;
}

WebRtc_Word32 VideoRenderMacCarbonImpl::SetBitmap(const void* bitMap,
        const WebRtc_UWord8 pictureId,
        const void* colorKey,
        const float left,
        const float top,
        const float right,
        const float bottom)
{
    return 0;
}


} //namespace webrtc

#endif // CARBON_RENDERING
