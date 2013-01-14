/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_I_VIDEO_RENDER_H_
#define WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_I_VIDEO_RENDER_H_

#include "video_render.h"

namespace webrtc {

// Class definitions
class IVideoRender
{
public:
    /*
     *   Constructor/destructor
     */

    virtual ~IVideoRender()
    {
    };

    virtual WebRtc_Word32 Init() = 0;

    virtual WebRtc_Word32 ChangeUniqueId(const WebRtc_Word32 id) = 0;

    virtual WebRtc_Word32 ChangeWindow(void* window) = 0;

    /**************************************************************************
     *
     *   Incoming Streams
     *
     ***************************************************************************/

    virtual VideoRenderCallback
            * AddIncomingRenderStream(const WebRtc_UWord32 streamId,
                                      const WebRtc_UWord32 zOrder,
                                      const float left,
                                      const float top,
                                      const float right,
                                      const float bottom) = 0;

    virtual WebRtc_Word32
            DeleteIncomingRenderStream(const WebRtc_UWord32 streamId) = 0;

    virtual WebRtc_Word32
            GetIncomingRenderStreamProperties(const WebRtc_UWord32 streamId,
                                              WebRtc_UWord32& zOrder,
                                              float& left,
                                              float& top,
                                              float& right,
                                              float& bottom) const = 0;
    // Implemented in common code?
    //virtual WebRtc_UWord32 GetNumIncomingRenderStreams() const = 0;
    //virtual bool HasIncomingRenderStream(const WebRtc_UWord16 stramId) const = 0;


    /**************************************************************************
     *
     *   Start/Stop
     *
     ***************************************************************************/

    virtual WebRtc_Word32 StartRender() = 0;

    virtual WebRtc_Word32 StopRender() = 0;

    /**************************************************************************
     *
     *   Properties
     *
     ***************************************************************************/
    virtual VideoRenderType RenderType() = 0;

    virtual RawVideoType PerferedVideoType() = 0;

    virtual bool FullScreen() = 0;

    // TODO: This should be treated in platform specific code only
    virtual WebRtc_Word32
            GetGraphicsMemory(WebRtc_UWord64& totalGraphicsMemory,
                              WebRtc_UWord64& availableGraphicsMemory) const = 0;

    virtual WebRtc_Word32
            GetScreenResolution(WebRtc_UWord32& screenWidth,
                                WebRtc_UWord32& screenHeight) const = 0;

    virtual WebRtc_UWord32 RenderFrameRate(const WebRtc_UWord32 streamId) = 0;

    virtual WebRtc_Word32 SetStreamCropping(const WebRtc_UWord32 streamId,
                                            const float left,
                                            const float top,
                                            const float right,
                                            const float bottom) = 0;

    virtual WebRtc_Word32 ConfigureRenderer(const WebRtc_UWord32 streamId,
                                            const unsigned int zOrder,
                                            const float left,
                                            const float top,
                                            const float right,
                                            const float bottom) = 0;

    virtual WebRtc_Word32 SetTransparentBackground(const bool enable) = 0;

    virtual WebRtc_Word32 SetText(const WebRtc_UWord8 textId,
                                  const WebRtc_UWord8* text,
                                  const WebRtc_Word32 textLength,
                                  const WebRtc_UWord32 textColorRef,
                                  const WebRtc_UWord32 backgroundColorRef,
                                  const float left,
                                  const float top,
                                  const float rigth,
                                  const float bottom) = 0;

    virtual WebRtc_Word32 SetBitmap(const void* bitMap,
                                    const WebRtc_UWord8 pictureId,
                                    const void* colorKey,
                                    const float left,
                                    const float top,
                                    const float right,
                                    const float bottom) = 0;

};
} //namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_I_VIDEO_RENDER_H_
