/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_WINDOWS_I_VIDEO_RENDER_WIN_H_
#define WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_WINDOWS_I_VIDEO_RENDER_WIN_H_

#include "video_render.h"

namespace webrtc {

// Class definitions
class IVideoRenderWin
{
public:
    /**************************************************************************
     *
     *   Constructor/destructor
     *
     ***************************************************************************/
    virtual ~IVideoRenderWin()
    {
    };

    virtual int32_t Init() = 0;

    /**************************************************************************
     *
     *   Incoming Streams
     *
     ***************************************************************************/

    virtual VideoRenderCallback
            * CreateChannel(const uint32_t streamId,
                            const uint32_t zOrder,
                            const float left,
                            const float top,
                            const float right,
                            const float bottom) = 0;

    virtual int32_t DeleteChannel(const uint32_t streamId) = 0;

    virtual int32_t GetStreamSettings(const uint32_t channel,
                                      const uint16_t streamId,
                                      uint32_t& zOrder,
                                      float& left, float& top,
                                      float& right, float& bottom) = 0;

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

    virtual bool IsFullScreen() = 0;

    virtual int32_t SetCropping(const uint32_t channel,
                                const uint16_t streamId,
                                const float left, const float top,
                                const float right, const float bottom) = 0;

    virtual int32_t ConfigureRenderer(const uint32_t channel,
                                      const uint16_t streamId,
                                      const unsigned int zOrder,
                                      const float left,
                                      const float top,
                                      const float right,
                                      const float bottom) = 0;

    virtual int32_t SetTransparentBackground(const bool enable) = 0;

    virtual int32_t SetText(const uint8_t textId,
                            const uint8_t* text,
                            const int32_t textLength,
                            const uint32_t colorText,
                            const uint32_t colorBg,
                            const float left, const float top,
                            const float rigth, const float bottom) = 0;

    virtual int32_t SetBitmap(const void* bitMap,
                              const uint8_t pictureId,
                              const void* colorKey,
                              const float left, const float top,
                              const float right, const float bottom) = 0;

    virtual int32_t ChangeWindow(void* window) = 0;

    virtual int32_t GetGraphicsMemory(uint64_t& totalMemory,
                                      uint64_t& availableMemory) = 0;

};

} //namespace webrtc
#endif  // WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_WINDOWS_I_VIDEO_RENDER_WIN_H_
