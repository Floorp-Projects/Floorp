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

    virtual WebRtc_Word32 Init() = 0;

    /**************************************************************************
     *
     *   Incoming Streams
     *
     ***************************************************************************/

    virtual VideoRenderCallback
            * CreateChannel(const WebRtc_UWord32 streamId,
                            const WebRtc_UWord32 zOrder,
                            const float left,
                            const float top,
                            const float right,
                            const float bottom) = 0;

    virtual WebRtc_Word32 DeleteChannel(const WebRtc_UWord32 streamId) = 0;

    virtual WebRtc_Word32 GetStreamSettings(const WebRtc_UWord32 channel,
                                            const WebRtc_UWord16 streamId,
                                            WebRtc_UWord32& zOrder,
                                            float& left,
                                            float& top,
                                            float& right,
                                            float& bottom) = 0;

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

    virtual bool IsFullScreen() = 0;

    virtual WebRtc_Word32 SetCropping(const WebRtc_UWord32 channel,
                                      const WebRtc_UWord16 streamId,
                                      const float left,
                                      const float top,
                                      const float right,
                                      const float bottom) = 0;

    virtual WebRtc_Word32 ConfigureRenderer(const WebRtc_UWord32 channel,
                                            const WebRtc_UWord16 streamId,
                                            const unsigned int zOrder,
                                            const float left,
                                            const float top,
                                            const float right,
                                            const float bottom) = 0;

    virtual WebRtc_Word32 SetTransparentBackground(const bool enable) = 0;

    virtual WebRtc_Word32 SetText(const WebRtc_UWord8 textId,
                                  const WebRtc_UWord8* text,
                                  const WebRtc_Word32 textLength,
                                  const WebRtc_UWord32 colorText,
                                  const WebRtc_UWord32 colorBg,
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

    virtual WebRtc_Word32 ChangeWindow(void* window) = 0;

    virtual WebRtc_Word32 GetGraphicsMemory(WebRtc_UWord64& totalMemory,
                                            WebRtc_UWord64& availableMemory) = 0;

};

} //namespace webrtc
#endif  // WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_WINDOWS_I_VIDEO_RENDER_WIN_H_
