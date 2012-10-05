/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_VIDEO_RENDER_IMPL_H_
#define WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_VIDEO_RENDER_IMPL_H_

#include "engine_configurations.h"
#include "video_render.h"
#include "map_wrapper.h"

//#include "video_render_defines.h"

namespace webrtc {
class CriticalSectionWrapper;
class IncomingVideoStream;
class IVideoRender;
class MapWrapper;

// Class definitions
class ModuleVideoRenderImpl: public VideoRender
{
public:
    /*
     *   VideoRenderer constructor/destructor
     */
    ModuleVideoRenderImpl(const WebRtc_Word32 id,
                          const VideoRenderType videoRenderType,
                          void* window, const bool fullscreen);

    virtual ~ModuleVideoRenderImpl();

    /*
     *   Change the unique identifier of this object
     */
    virtual WebRtc_Word32 ChangeUniqueId(const WebRtc_Word32 id);

    virtual WebRtc_Word32 TimeUntilNextProcess();
    virtual WebRtc_Word32 Process();

    /*
     *   Returns the render window
     */
    virtual void* Window();

    /*
     *   Change render window
     */
    virtual WebRtc_Word32 ChangeWindow(void* window);

    /*
     *   Returns module id
     */
    WebRtc_Word32 Id();

    /**************************************************************************
     *
     *   Incoming Streams
     *
     ***************************************************************************/

    /*
     *   Add incoming render stream
     */
    virtual VideoRenderCallback
            * AddIncomingRenderStream(const WebRtc_UWord32 streamId,
                                      const WebRtc_UWord32 zOrder,
                                      const float left, const float top,
                                      const float right, const float bottom);
    /*
     *   Delete incoming render stream
     */
    virtual WebRtc_Word32
            DeleteIncomingRenderStream(const WebRtc_UWord32 streamId);

    /*
     *   Add incoming render callback, used for external rendering
     */
    virtual WebRtc_Word32
            AddExternalRenderCallback(const WebRtc_UWord32 streamId,
                                      VideoRenderCallback* renderObject);

    /*
     *   Get the porperties for an incoming render stream
     */
    virtual WebRtc_Word32
            GetIncomingRenderStreamProperties(const WebRtc_UWord32 streamId,
                                              WebRtc_UWord32& zOrder,
                                              float& left, float& top,
                                              float& right, float& bottom) const;
    /*
     *   Incoming frame rate for the specified stream.
     */
    virtual WebRtc_UWord32 GetIncomingFrameRate(const WebRtc_UWord32 streamId);

    /*
     *   Returns the number of incoming streams added to this render module
     */
    virtual WebRtc_UWord32 GetNumIncomingRenderStreams() const;

    /*
     *   Returns true if this render module has the streamId added, false otherwise.
     */
    virtual bool HasIncomingRenderStream(const WebRtc_UWord32 streamId) const;

    /*
     *
     */
    virtual WebRtc_Word32
            RegisterRawFrameCallback(const WebRtc_UWord32 streamId,
                                     VideoRenderCallback* callbackObj);

    virtual WebRtc_Word32 GetLastRenderedFrame(const WebRtc_UWord32 streamId,
                                               VideoFrame &frame) const;

    /**************************************************************************
     *
     *   Start/Stop
     *
     ***************************************************************************/

    /*
     *   Starts rendering the specified stream
     */
    virtual WebRtc_Word32 StartRender(const WebRtc_UWord32 streamId);

    /*
     *   Stops the renderer
     */
    virtual WebRtc_Word32 StopRender(const WebRtc_UWord32 streamId);

    /*
     *   Sets the renderer in start state, no streams removed.
     */
    virtual WebRtc_Word32 ResetRender();

    /**************************************************************************
     *
     *   Properties
     *
     ***************************************************************************/

    /*
     *   Returns the prefered render video type
     */
    virtual RawVideoType PreferredVideoType() const;

    /*
     *   Returns true if the renderer is in fullscreen mode, otherwise false.
     */
    virtual bool IsFullScreen();

    /*
     *   Gets screen resolution in pixels
     */
    virtual WebRtc_Word32
            GetScreenResolution(WebRtc_UWord32& screenWidth,
                                WebRtc_UWord32& screenHeight) const;

    /*
     *   Get the actual render rate for this stream. I.e rendered frame rate,
     *   not frames delivered to the renderer.
     */
    virtual WebRtc_UWord32 RenderFrameRate(const WebRtc_UWord32 streamId);

    /*
     *   Set cropping of incoming stream
     */
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

    virtual WebRtc_Word32 FullScreenRender(void* window, const bool enable);

    virtual WebRtc_Word32 SetBitmap(const void* bitMap,
                                    const WebRtc_UWord8 pictureId,
                                    const void* colorKey, const float left,
                                    const float top, const float right,
                                    const float bottom);

    virtual WebRtc_Word32 SetText(const WebRtc_UWord8 textId,
                                  const WebRtc_UWord8* text,
                                  const WebRtc_Word32 textLength,
                                  const WebRtc_UWord32 textColorRef,
                                  const WebRtc_UWord32 backgroundColorRef,
                                  const float left, const float top,
                                  const float right, const float bottom);

    virtual WebRtc_Word32 SetStartImage(const WebRtc_UWord32 streamId,
                                        const VideoFrame& videoFrame);

    virtual WebRtc_Word32 SetTimeoutImage(const WebRtc_UWord32 streamId,
                                          const VideoFrame& videoFrame,
                                          const WebRtc_UWord32 timeout);

    virtual WebRtc_Word32 MirrorRenderStream(const int renderId,
                                             const bool enable,
                                             const bool mirrorXAxis,
                                             const bool mirrorYAxis);

private:
    WebRtc_Word32 _id;
    CriticalSectionWrapper& _moduleCrit;
    void* _ptrWindow;
    bool _fullScreen;

    IVideoRender* _ptrRenderer;
    MapWrapper& _streamRenderMap;
};

} //namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_VIDEO_RENDER_IMPL_H_
