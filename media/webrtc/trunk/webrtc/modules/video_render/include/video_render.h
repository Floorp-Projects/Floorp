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

#include "webrtc/modules/interface/module.h"
#include "webrtc/modules/video_render/include/video_render_defines.h"

namespace webrtc {

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
                                          const int32_t id,
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
    virtual int32_t ChangeUniqueId(const int32_t id) OVERRIDE = 0;

    virtual int32_t TimeUntilNextProcess() OVERRIDE = 0;
    virtual int32_t Process() OVERRIDE = 0;

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
    virtual int32_t ChangeWindow(void* window) = 0;

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
            * AddIncomingRenderStream(const uint32_t streamId,
                                      const uint32_t zOrder,
                                      const float left, const float top,
                                      const float right, const float bottom) = 0;
    /*
     *   Delete incoming render stream
     *
     *   streamID    - id of the stream to add
     */
    virtual int32_t
            DeleteIncomingRenderStream(const uint32_t streamId) = 0;

    /*
     *   Add incoming render callback, used for external rendering
     *
     *   streamID     - id of the stream the callback is used for
     *   renderObject - the VideoRenderCallback to use for this stream, NULL to remove
     *
     *   Return      - callback class to use for delivering new frames to render.
     */
    virtual int32_t
            AddExternalRenderCallback(const uint32_t streamId,
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
    virtual int32_t
            GetIncomingRenderStreamProperties(const uint32_t streamId,
                                              uint32_t& zOrder,
                                              float& left, float& top,
                                              float& right, float& bottom) const = 0;
    /*
     *   The incoming frame rate to the module, not the rate rendered in the window.
     */
    virtual uint32_t
            GetIncomingFrameRate(const uint32_t streamId) = 0;

    /*
     *   Returns the number of incoming streams added to this render module
     */
    virtual uint32_t GetNumIncomingRenderStreams() const = 0;

    /*
     *   Returns true if this render module has the streamId added, false otherwise.
     */
    virtual bool
            HasIncomingRenderStream(const uint32_t streamId) const = 0;

    /*
     *   Registers a callback to get raw images in the same time as sent
     *   to the renderer. To be used for external rendering.
     */
    virtual int32_t
            RegisterRawFrameCallback(const uint32_t streamId,
                                     VideoRenderCallback* callbackObj) = 0;

    /*
     * This method is usefull to get last rendered frame for the stream specified
     */
    virtual int32_t
            GetLastRenderedFrame(const uint32_t streamId,
                                 I420VideoFrame &frame) const = 0;

    /**************************************************************************
     *
     *   Start/Stop
     *
     ***************************************************************************/

    /*
     *   Starts rendering the specified stream
     */
    virtual int32_t StartRender(const uint32_t streamId) = 0;

    /*
     *   Stops the renderer
     */
    virtual int32_t StopRender(const uint32_t streamId) = 0;

    /*
     *   Resets the renderer
     *   No streams are removed. The state should be as after AddStream was called.
     */
    virtual int32_t ResetRender() = 0;

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
    virtual int32_t
            GetScreenResolution(uint32_t& screenWidth,
                                uint32_t& screenHeight) const = 0;

    /*
     *   Get the actual render rate for this stream. I.e rendered frame rate,
     *   not frames delivered to the renderer.
     */
    virtual uint32_t RenderFrameRate(const uint32_t streamId) = 0;

    /*
     *   Set cropping of incoming stream
     */
    virtual int32_t SetStreamCropping(const uint32_t streamId,
                                      const float left,
                                      const float top,
                                      const float right,
                                      const float bottom) = 0;

    /*
     * re-configure renderer
     */

    // Set the expected time needed by the graphics card or external renderer,
    // i.e. frames will be released for rendering |delay_ms| before set render
    // time in the video frame.
    virtual int32_t SetExpectedRenderDelay(uint32_t stream_id,
                                           int32_t delay_ms) = 0;

    virtual int32_t ConfigureRenderer(const uint32_t streamId,
                                      const unsigned int zOrder,
                                      const float left,
                                      const float top,
                                      const float right,
                                      const float bottom) = 0;

    virtual int32_t SetTransparentBackground(const bool enable) = 0;

    virtual int32_t FullScreenRender(void* window, const bool enable) = 0;

    virtual int32_t SetBitmap(const void* bitMap,
                              const uint8_t pictureId,
                              const void* colorKey,
                              const float left, const float top,
                              const float right, const float bottom) = 0;

    virtual int32_t SetText(const uint8_t textId,
                            const uint8_t* text,
                            const int32_t textLength,
                            const uint32_t textColorRef,
                            const uint32_t backgroundColorRef,
                            const float left, const float top,
                            const float right, const float bottom) = 0;

    /*
     * Set a start image. The image is rendered before the first image has been delivered
     */
    virtual int32_t
            SetStartImage(const uint32_t streamId,
                          const I420VideoFrame& videoFrame) = 0;

    /*
     * Set a timout image. The image is rendered if no videoframe has been delivered
     */
    virtual int32_t SetTimeoutImage(const uint32_t streamId,
                                    const I420VideoFrame& videoFrame,
                                    const uint32_t timeout)= 0;

    virtual int32_t MirrorRenderStream(const int renderId,
                                       const bool enable,
                                       const bool mirrorXAxis,
                                       const bool mirrorYAxis) = 0;
};
}  // namespace webrtc
#endif  // WEBRTC_MODULES_VIDEO_RENDER_MAIN_INTERFACE_VIDEO_RENDER_H_
