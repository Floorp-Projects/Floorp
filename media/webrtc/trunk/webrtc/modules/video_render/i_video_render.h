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

    virtual int32_t Init() = 0;

    virtual int32_t ChangeUniqueId(const int32_t id) = 0;

    virtual int32_t ChangeWindow(void* window) = 0;

    /**************************************************************************
     *
     *   Incoming Streams
     *
     ***************************************************************************/

    virtual VideoRenderCallback
            * AddIncomingRenderStream(const uint32_t streamId,
                                      const uint32_t zOrder,
                                      const float left,
                                      const float top,
                                      const float right,
                                      const float bottom) = 0;

    virtual int32_t
            DeleteIncomingRenderStream(const uint32_t streamId) = 0;

    virtual int32_t
            GetIncomingRenderStreamProperties(const uint32_t streamId,
                                              uint32_t& zOrder,
                                              float& left,
                                              float& top,
                                              float& right,
                                              float& bottom) const = 0;
    // Implemented in common code?
    //virtual uint32_t GetNumIncomingRenderStreams() const = 0;
    //virtual bool HasIncomingRenderStream(const uint16_t stramId) const = 0;


    /**************************************************************************
     *
     *   Start/Stop
     *
     ***************************************************************************/

    virtual int32_t StartRender() = 0;

    virtual int32_t StopRender() = 0;

    /**************************************************************************
     *
     *   Properties
     *
     ***************************************************************************/
    virtual VideoRenderType RenderType() = 0;

    virtual RawVideoType PerferedVideoType() = 0;

    virtual bool FullScreen() = 0;

    // TODO: This should be treated in platform specific code only
    virtual int32_t
            GetGraphicsMemory(uint64_t& totalGraphicsMemory,
                              uint64_t& availableGraphicsMemory) const = 0;

    virtual int32_t
            GetScreenResolution(uint32_t& screenWidth,
                                uint32_t& screenHeight) const = 0;

    virtual uint32_t RenderFrameRate(const uint32_t streamId) = 0;

    virtual int32_t SetStreamCropping(const uint32_t streamId,
                                      const float left,
                                      const float top,
                                      const float right,
                                      const float bottom) = 0;

    virtual int32_t ConfigureRenderer(const uint32_t streamId,
                                      const unsigned int zOrder,
                                      const float left,
                                      const float top,
                                      const float right,
                                      const float bottom) = 0;

    virtual int32_t SetTransparentBackground(const bool enable) = 0;

    virtual int32_t SetText(const uint8_t textId,
                            const uint8_t* text,
                            const int32_t textLength,
                            const uint32_t textColorRef,
                            const uint32_t backgroundColorRef,
                            const float left,
                            const float top,
                            const float rigth,
                            const float bottom) = 0;

    virtual int32_t SetBitmap(const void* bitMap,
                              const uint8_t pictureId,
                              const void* colorKey,
                              const float left,
                              const float top,
                              const float right,
                              const float bottom) = 0;

};
} //namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_I_VIDEO_RENDER_H_
