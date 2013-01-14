/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_RENDER_MAIN_INTERFACE_VIDEO_RENDER_H_
#define WEBRTC_MODULES_VIDEO_RENDER_MAIN_INTERFACE_VIDEO_RENDER_H_

/*
 * video_render.h
 *
 * This header file together with module.h and module_common_types.h
 * contains all of the APIs that are needed for using the video render
 * module class.
 *
 */

#include "modules/interface/module.h"
#include "webrtc/modules/video_render/include/video_render_defines.h"

namespace webrtc {

#if defined(WEBRTC_ANDROID) && !defined(WEBRTC_CHROMIUM_BUILD)
WebRtc_Word32 SetRenderAndroidVM(void* javaVM);
#endif

// Class definitions
class VideoRender: public Module
{
public:
    /*
     *   Create a video render module object
     *
     *   id              - unique identifier of this video render module object
     *   window          - pointer to the window to render to
     *   fullscreen      - true if this is a fullscreen renderer
     *   videoRenderType - type of renderer to create
     */
    static VideoRender
            * CreateVideoRender(
                                          const WebRtc_Word32 id,
                                          void* window,
                                          const bool fullscreen,
                                          const VideoRenderType videoRenderType =
                                                  kRenderDefault);

    /*
     *   Destroy a video render module object
     *
     *   module  - object to destroy
     */
    static void DestroyVideoRender(VideoRender* module);

    /*
     *   Change the unique identifier of this object
     *
     *   id      - new unique identifier of this video render module object
     */
    virtual WebRtc_Word32 ChangeUniqueId(const WebRtc_Word32 id) = 0;

    virtual WebRtc_Word32 TimeUntilNextProcess() = 0;
    virtual WebRtc_Word32 Process() = 0;

    /**************************************************************************
     *
     *   Window functions
     *
     ***************************************************************************/

    /*
     *   Get window for this renderer
     */
    virtual void* Window() = 0;

    /*
     *   Change render window
     *
     *   window      - the new render window, assuming same type as originally created.
     */
    virtual WebRtc_Word32 ChangeWindow(void* window) = 0;

    /**************************************************************************
     *
     *   Incoming Streams
     *
     ***************************************************************************/

    /*
     *   Add incoming render stream
     *
     *   streamID    - id of the stream to add
     *   zOrder      - relative render order for the streams, 0 = on top
     *   left        - position of the stream in the window, [0.0f, 1.0f]
     *   top         - position of the stream in the window, [0.0f, 1.0f]
     *   right       - position of the stream in the window, [0.0f, 1.0f]
     *   bottom      - position of the stream in the window, [0.0f, 1.0f]
     *
     *   Return      - callback class to use for delivering new frames to render.
     */
    virtual VideoRenderCallback
            * AddIncomingRenderStream(const WebRtc_UWord32 streamId,
                                      const WebRtc_UWord32 zOrder,
                                      const float left, const float top,
                                      const float right, const float bottom) = 0;
    /*
     *   Delete incoming render stream
     *
     *   streamID    - id of the stream to add
     */
    virtual WebRtc_Word32
            DeleteIncomingRenderStream(const WebRtc_UWord32 streamId) = 0;

    /*
     *   Add incoming render callback, used for external rendering
     *
     *   streamID     - id of the stream the callback is used for
     *   renderObject - the VideoRenderCallback to use for this stream, NULL to remove
     *
     *   Return      - callback class to use for delivering new frames to render.
     */
    virtual WebRtc_Word32
            AddExternalRenderCallback(const WebRtc_UWord32 streamId,
                                      VideoRenderCallback* renderObject) = 0;

    /*
     *   Get the porperties for an incoming render stream
     *
     *   streamID    - [in] id of the stream to get properties for
     *   zOrder      - [out] relative render order for the streams, 0 = on top
     *   left        - [out] position of the stream in the window, [0.0f, 1.0f]
     *   top         - [out] position of the stream in the window, [0.0f, 1.0f]
     *   right       - [out] position of the stream in the window, [0.0f, 1.0f]
     *   bottom      - [out] position of the stream in the window, [0.0f, 1.0f]
     */
    virtual WebRtc_Word32
            GetIncomingRenderStreamProperties(const WebRtc_UWord32 streamId,
                                              WebRtc_UWord32& zOrder,
                                              float& left, float& top,
                                              float& right, float& bottom) const = 0;
    /*
     *   The incoming frame rate to the module, not the rate rendered in the window.
     */
    virtual WebRtc_UWord32
            GetIncomingFrameRate(const WebRtc_UWord32 streamId) = 0;

    /*
     *   Returns the number of incoming streams added to this render module
     */
    virtual WebRtc_UWord32 GetNumIncomingRenderStreams() const = 0;

    /*
     *   Returns true if this render module has the streamId added, false otherwise.
     */
    virtual bool
            HasIncomingRenderStream(const WebRtc_UWord32 streamId) const = 0;

    /*
     *   Registers a callback to get raw images in the same time as sent
     *   to the renderer. To be used for external rendering.
     */
    virtual WebRtc_Word32
            RegisterRawFrameCallback(const WebRtc_UWord32 streamId,
                                     VideoRenderCallback* callbackObj) = 0;

    /*
     * This method is usefull to get last rendered frame for the stream specified
     */
    virtual WebRtc_Word32
            GetLastRenderedFrame(const WebRtc_UWord32 streamId,
                                 I420VideoFrame &frame) const = 0;

    /**************************************************************************
     *
     *   Start/Stop
     *
     ***************************************************************************/

    /*
     *   Starts rendering the specified stream
     */
    virtual WebRtc_Word32 StartRender(const WebRtc_UWord32 streamId) = 0;

    /*
     *   Stops the renderer
     */
    virtual WebRtc_Word32 StopRender(const WebRtc_UWord32 streamId) = 0;

    /*
     *   Resets the renderer
     *   No streams are removed. The state should be as after AddStream was called.
     */
    virtual WebRtc_Word32 ResetRender() = 0;

    /**************************************************************************
     *
     *   Properties
     *
     ***************************************************************************/

    /*
     *   Returns the preferred render video type
     */
    virtual RawVideoType PreferredVideoType() const = 0;

    /*
     *   Returns true if the renderer is in fullscreen mode, otherwise false.
     */
    virtual bool IsFullScreen() = 0;

    /*
     *   Gets screen resolution in pixels
     */
    virtual WebRtc_Word32
            GetScreenResolution(WebRtc_UWord32& screenWidth,
                                WebRtc_UWord32& screenHeight) const = 0;

    /*
     *   Get the actual render rate for this stream. I.e rendered frame rate,
     *   not frames delivered to the renderer.
     */
    virtual WebRtc_UWord32 RenderFrameRate(const WebRtc_UWord32 streamId) = 0;

    /*
     *   Set cropping of incoming stream
     */
    virtual WebRtc_Word32 SetStreamCropping(const WebRtc_UWord32 streamId,
                                            const float left, const float top,
                                            const float right,
                                            const float bottom) = 0;

    /*
     * re-configure renderer
     */

    // Set the expected time needed by the graphics card or external renderer,
    // i.e. frames will be released for rendering |delay_ms| before set render
    // time in the video frame.
    virtual WebRtc_Word32 SetExpectedRenderDelay(WebRtc_UWord32 stream_id,
                                                 WebRtc_Word32 delay_ms) = 0;

    virtual WebRtc_Word32 ConfigureRenderer(const WebRtc_UWord32 streamId,
                                            const unsigned int zOrder,
                                            const float left, const float top,
                                            const float right,
                                            const float bottom) = 0;

    virtual WebRtc_Word32 SetTransparentBackground(const bool enable) = 0;

    virtual WebRtc_Word32 FullScreenRender(void* window, const bool enable) = 0;

    virtual WebRtc_Word32 SetBitmap(const void* bitMap,
                                    const WebRtc_UWord8 pictureId,
                                    const void* colorKey, const float left,
                                    const float top, const float right,
                                    const float bottom) = 0;

    virtual WebRtc_Word32 SetText(const WebRtc_UWord8 textId,
                                  const WebRtc_UWord8* text,
                                  const WebRtc_Word32 textLength,
                                  const WebRtc_UWord32 textColorRef,
                                  const WebRtc_UWord32 backgroundColorRef,
                                  const float left, const float top,
                                  const float right, const float bottom) = 0;

    /*
     * Set a start image. The image is rendered before the first image has been delivered
     */
    virtual WebRtc_Word32
            SetStartImage(const WebRtc_UWord32 streamId,
                          const I420VideoFrame& videoFrame) = 0;

    /*
     * Set a timout image. The image is rendered if no videoframe has been delivered
     */
    virtual WebRtc_Word32 SetTimeoutImage(const WebRtc_UWord32 streamId,
                                          const I420VideoFrame& videoFrame,
                                          const WebRtc_UWord32 timeout)= 0;

    virtual WebRtc_Word32 MirrorRenderStream(const int renderId,
                                             const bool enable,
                                             const bool mirrorXAxis,
                                             const bool mirrorYAxis) = 0;
};
} //namespace webrtc
#endif  // WEBRTC_MODULES_VIDEO_RENDER_MAIN_INTERFACE_VIDEO_RENDER_H_
