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
#if defined(CARBON_RENDERING)

#include <AGL/agl.h>
#include "webrtc/modules/video_render/mac/video_render_agl.h"
#include "webrtc/modules/video_render/mac/video_render_mac_carbon_impl.h"
#include "webrtc/system_wrappers/include/critical_section_wrapper.h"
#include "webrtc/system_wrappers/include/trace.h"

namespace webrtc {

VideoRenderMacCarbonImpl::VideoRenderMacCarbonImpl(const int32_t id,
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

int32_t
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

int32_t
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
VideoRenderMacCarbonImpl::AddIncomingRenderStream(const uint32_t streamId,
        const uint32_t zOrder,
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

int32_t
VideoRenderMacCarbonImpl::DeleteIncomingRenderStream(const uint32_t streamId)
{

    WEBRTC_TRACE(kTraceDebug, kTraceVideoRenderer, _id, "%s:%d", __FUNCTION__, __LINE__);
    CriticalSectionScoped cs(&_renderMacCarbonCritsect);
    _ptrCarbonRender->DeleteAGLChannel(streamId);

    return 0;
}

int32_t
VideoRenderMacCarbonImpl::GetIncomingRenderStreamProperties(const uint32_t streamId,
        uint32_t& zOrder,
        float& left,
        float& top,
        float& right,
        float& bottom) const
{
    return -1;
    return _ptrCarbonRender->GetChannelProperties(streamId, zOrder, left, top, right, bottom);
}

int32_t
VideoRenderMacCarbonImpl::StartRender()
{
    return _ptrCarbonRender->StartRender();
}

int32_t
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

int32_t
VideoRenderMacCarbonImpl::GetGraphicsMemory(uint64_t& totalGraphicsMemory,
        uint64_t& availableGraphicsMemory) const
{
    totalGraphicsMemory = 0;
    availableGraphicsMemory = 0;
    return 0;
}

int32_t
VideoRenderMacCarbonImpl::GetScreenResolution(uint32_t& screenWidth,
        uint32_t& screenHeight) const
{
    CriticalSectionScoped cs(&_renderMacCarbonCritsect);
    //NSScreen* mainScreen = [NSScreen mainScreen];

    //NSRect frame = [mainScreen frame];

    //screenWidth = frame.size.width;
    //screenHeight = frame.size.height;
    return 0;
}

uint32_t
VideoRenderMacCarbonImpl::RenderFrameRate(const uint32_t streamId)
{
    CriticalSectionScoped cs(&_renderMacCarbonCritsect);
    return 0;
}

int32_t
VideoRenderMacCarbonImpl::SetStreamCropping(const uint32_t streamId,
        const float left,
        const float top,
        const float right,
        const float bottom)
{
    return 0;
}

int32_t VideoRenderMacCarbonImpl::ConfigureRenderer(const uint32_t streamId,
                                                    const unsigned int zOrder,
                                                    const float left,
                                                    const float top,
                                                    const float right,
                                                    const float bottom)
{
    return 0;
}

int32_t
VideoRenderMacCarbonImpl::SetTransparentBackground(const bool enable)
{
    return 0;
}

int32_t VideoRenderMacCarbonImpl::SetText(const uint8_t textId,
                                          const uint8_t* text,
                                          const int32_t textLength,
                                          const uint32_t textColorRef,
                                          const uint32_t backgroundColorRef,
                                          const float left,
                                          const float top,
                                          const float right,
                                          const float bottom)
{
    return 0;
}

int32_t VideoRenderMacCarbonImpl::SetBitmap(const void* bitMap,
                                            const uint8_t pictureId,
                                            const void* colorKey,
                                            const float left,
                                            const float top,
                                            const float right,
                                            const float bottom)
{
    return 0;
}


}  // namespace webrtc

#endif // CARBON_RENDERING
