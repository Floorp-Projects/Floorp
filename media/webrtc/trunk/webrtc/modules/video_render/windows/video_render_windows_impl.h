/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_WINDOWS_VIDEO_RENDER_WINDOWS_IMPL_H_
#define WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_WINDOWS_VIDEO_RENDER_WINDOWS_IMPL_H_

#include <Winerror.h>
#include <dxdiag.h>

#include "webrtc/modules/video_render/i_video_render.h"
#include "i_video_render_win.h"

namespace webrtc {
class CriticalSectionWrapper;

#define EXPAND(x)            x, sizeof(x)/sizeof(TCHAR)

#pragma comment(lib, "dxguid.lib")

enum VideoRenderWinMethod {
  kVideoRenderWinD3D9 = 0,
};

// Class definitions
class VideoRenderWindowsImpl: IVideoRender
{
public:
    /*
     *   Constructor/destructor
     */

    VideoRenderWindowsImpl(const WebRtc_Word32 id,
                           const VideoRenderType videoRenderType,
                           void* window, const bool fullscreen);

    virtual ~VideoRenderWindowsImpl();

    virtual WebRtc_Word32 Init();

    virtual WebRtc_Word32 ChangeUniqueId(const WebRtc_Word32 id);

    virtual WebRtc_Word32 ChangeWindow(void* window);

    /**************************************************************************
     *
     *   Incoming Streams
     *
     ***************************************************************************/

    virtual VideoRenderCallback
            * AddIncomingRenderStream(const WebRtc_UWord32 streamId,
                                      const WebRtc_UWord32 zOrder,
                                      const float left, const float top,
                                      const float right, const float bottom);

    virtual WebRtc_Word32
            DeleteIncomingRenderStream(const WebRtc_UWord32 streamId);

    virtual WebRtc_Word32
            GetIncomingRenderStreamProperties(const WebRtc_UWord32 streamId,
                                              WebRtc_UWord32& zOrder,
                                              float& left, float& top,
                                              float& right, float& bottom) const;

    /**************************************************************************
     *
     *   Start/Stop
     *
     ***************************************************************************/

    virtual WebRtc_Word32 StartRender();

    virtual WebRtc_Word32 StopRender();

    /**************************************************************************
     *
     *   Properties
     *
     ***************************************************************************/

    virtual VideoRenderType RenderType();

    virtual RawVideoType PerferedVideoType();

    virtual bool FullScreen();

    virtual WebRtc_Word32
            GetGraphicsMemory(WebRtc_UWord64& totalGraphicsMemory,
                              WebRtc_UWord64& availableGraphicsMemory) const;

    virtual WebRtc_Word32
            GetScreenResolution(WebRtc_UWord32& screenWidth,
                                WebRtc_UWord32& screenHeight) const;

    virtual WebRtc_UWord32 RenderFrameRate(const WebRtc_UWord32 streamId);

    virtual WebRtc_Word32 SetStreamCropping(const WebRtc_UWord32 streamId,
                                            const float left, const float top,
                                            const float right,
                                            const float bottom);

    virtual WebRtc_Word32 ConfigureRenderer(const WebRtc_UWord32 streamId,
                                            const unsigned int zOrder,
                                            const float left, const float top,
                                            const float right,
                                            const float bottom);

    virtual WebRtc_Word32 SetTransparentBackground(const bool enable);

    virtual WebRtc_Word32 SetText(const WebRtc_UWord8 textId,
                                  const WebRtc_UWord8* text,
                                  const WebRtc_Word32 textLength,
                                  const WebRtc_UWord32 textColorRef,
                                  const WebRtc_UWord32 backgroundColorRef,
                                  const float left, const float top,
                                  const float right, const float bottom);

    virtual WebRtc_Word32 SetBitmap(const void* bitMap,
                                    const WebRtc_UWord8 pictureId,
                                    const void* colorKey, const float left,
                                    const float top, const float right,
                                    const float bottom);

private:
    WebRtc_Word32 _id;
    CriticalSectionWrapper& _renderWindowsCritsect;

    void* _prtWindow;
    bool _fullscreen;

    VideoRenderWinMethod _renderMethod;
    IVideoRenderWin* _ptrRendererWin;
};

} //namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_WINDOWS_VIDEO_RENDER_WINDOWS_IMPL_H_
