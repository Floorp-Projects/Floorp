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
#if defined(COCOA_RENDERING)

#import "cocoa_render_view.h"

#include "video_render_mac_cocoa_impl.h"
#include "critical_section_wrapper.h"
#include "video_render_nsopengl.h"
#include "trace.h"

namespace webrtc {

VideoRenderMacCocoaImpl::VideoRenderMacCocoaImpl(const int32_t id,
        const VideoRenderType videoRenderType,
        void* window,
        const bool fullscreen) :
_id(id),
_renderMacCocoaCritsect(*CriticalSectionWrapper::CreateCriticalSection()),
_fullScreen(fullscreen),
_ptrWindow(window)
{

    WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, _id, "Constructor %s:%d", __FUNCTION__, __LINE__);
}

VideoRenderMacCocoaImpl::~VideoRenderMacCocoaImpl()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, _id, "Destructor %s:%d", __FUNCTION__, __LINE__);
    delete &_renderMacCocoaCritsect;
    if (_ptrCocoaRender)
    {
        delete _ptrCocoaRender;
        _ptrCocoaRender = NULL;
    }
}

int32_t
VideoRenderMacCocoaImpl::Init()
{

    CriticalSectionScoped cs(&_renderMacCocoaCritsect);
    WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, _id, "%s:%d", __FUNCTION__, __LINE__);

    // cast ptrWindow from void* to CocoaRenderer. Void* was once NSOpenGLView, and CocoaRenderer is NSOpenGLView.
    _ptrCocoaRender = new VideoRenderNSOpenGL((CocoaRenderView*)_ptrWindow, _fullScreen, _id);
    if (!_ptrWindow)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVideoRenderer, _id, "Constructor %s:%d", __FUNCTION__, __LINE__);
        return -1;
    }
    int retVal = _ptrCocoaRender->Init();
    if (retVal == -1)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, _id, "Failed to init %s:%d", __FUNCTION__, __LINE__);
        return -1;
    }

    return 0;
}

int32_t
VideoRenderMacCocoaImpl::ChangeUniqueId(const int32_t id)
{
    CriticalSectionScoped cs(&_renderMacCocoaCritsect);
    WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, _id, "%s", __FUNCTION__);
    _id = id;

    if(_ptrCocoaRender)
    {
        _ptrCocoaRender->ChangeUniqueID(_id);
    }

    return 0;
}

int32_t
VideoRenderMacCocoaImpl::ChangeWindow(void* window)
{

    CriticalSectionScoped cs(&_renderMacCocoaCritsect);
    WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, _id, "%s changing ID to ", __FUNCTION__, window);

    if (window == NULL)
    {
        return -1;
    }
    _ptrWindow = window;


    _ptrWindow = window;
    _ptrCocoaRender->ChangeWindow((CocoaRenderView*)_ptrWindow);

    return 0;
}

VideoRenderCallback*
VideoRenderMacCocoaImpl::AddIncomingRenderStream(const uint32_t streamId,
        const uint32_t zOrder,
        const float left,
        const float top,
        const float right,
        const float bottom)
{
    CriticalSectionScoped cs(&_renderMacCocoaCritsect);
    WEBRTC_TRACE(kTraceDebug, kTraceVideoRenderer, _id, "%s", __FUNCTION__);
    VideoChannelNSOpenGL* nsOpenGLChannel = NULL;

    if(!_ptrWindow)
    {
    }

    if(!nsOpenGLChannel)
    {
        nsOpenGLChannel = _ptrCocoaRender->CreateNSGLChannel(streamId, zOrder, left, top, right, bottom);
    }

    return nsOpenGLChannel;

}

int32_t
VideoRenderMacCocoaImpl::DeleteIncomingRenderStream(const uint32_t streamId)
{
    WEBRTC_TRACE(kTraceDebug, kTraceVideoRenderer, _id, "Constructor %s:%d", __FUNCTION__, __LINE__);
    CriticalSectionScoped cs(&_renderMacCocoaCritsect);
    _ptrCocoaRender->DeleteNSGLChannel(streamId);

    return 0;
}

int32_t
VideoRenderMacCocoaImpl::GetIncomingRenderStreamProperties(const uint32_t streamId,
        uint32_t& zOrder,
        float& left,
        float& top,
        float& right,
        float& bottom) const
{
    return _ptrCocoaRender->GetChannelProperties(streamId, zOrder, left, top, right, bottom);
}

int32_t
VideoRenderMacCocoaImpl::StartRender()
{
    return _ptrCocoaRender->StartRender();
}

int32_t
VideoRenderMacCocoaImpl::StopRender()
{
    return _ptrCocoaRender->StopRender();
}

VideoRenderType
VideoRenderMacCocoaImpl::RenderType()
{
    return kRenderCocoa;
}

RawVideoType
VideoRenderMacCocoaImpl::PerferedVideoType()
{
    return kVideoI420;
}

bool
VideoRenderMacCocoaImpl::FullScreen()
{
    return false;
}

int32_t
VideoRenderMacCocoaImpl::GetGraphicsMemory(uint64_t& totalGraphicsMemory,
        uint64_t& availableGraphicsMemory) const
{
    totalGraphicsMemory = 0;
    availableGraphicsMemory = 0;
    return 0;
}

int32_t
VideoRenderMacCocoaImpl::GetScreenResolution(uint32_t& screenWidth,
        uint32_t& screenHeight) const
{
    CriticalSectionScoped cs(&_renderMacCocoaCritsect);
    NSScreen* mainScreen = [NSScreen mainScreen];

    NSRect frame = [mainScreen frame];

    screenWidth = frame.size.width;
    screenHeight = frame.size.height;
    return 0;
}

uint32_t
VideoRenderMacCocoaImpl::RenderFrameRate(const uint32_t streamId)
{
    CriticalSectionScoped cs(&_renderMacCocoaCritsect);
    return 0;
}

int32_t
VideoRenderMacCocoaImpl::SetStreamCropping(const uint32_t streamId,
        const float left,
        const float top,
        const float right,
        const float bottom)
{
    return 0;
}

int32_t VideoRenderMacCocoaImpl::ConfigureRenderer(const uint32_t streamId,
                                                   const unsigned int zOrder,
                                                   const float left,
                                                   const float top,
                                                   const float right,
                                                   const float bottom)
{
    return 0;
}

int32_t
VideoRenderMacCocoaImpl::SetTransparentBackground(const bool enable)
{
    return 0;
}

int32_t VideoRenderMacCocoaImpl::SetText(const uint8_t textId,
                                         const uint8_t* text,
                                         const int32_t textLength,
                                         const uint32_t textColorRef,
                                         const uint32_t backgroundColorRef,
                                         const float left,
                                         const float top,
                                         const float right,
                                         const float bottom)
{
    return _ptrCocoaRender->SetText(textId, text, textLength, textColorRef, backgroundColorRef, left, top, right, bottom);
}

int32_t VideoRenderMacCocoaImpl::SetBitmap(const void* bitMap,
                                           const uint8_t pictureId,
                                           const void* colorKey,
                                           const float left,
                                           const float top,
                                           const float right,
                                           const float bottom)
{
    return 0;
}

int32_t VideoRenderMacCocoaImpl::FullScreenRender(void* window, const bool enable)
{
    return -1;
}

}  // namespace webrtc

#endif // COCOA_RENDERING
