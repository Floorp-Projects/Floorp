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

#include <map>

#include "webrtc/engine_configurations.h"
#include "webrtc/modules/video_render/video_render.h"

namespace webrtc {
class CriticalSectionWrapper;
class IncomingVideoStream;
class IVideoRender;

// Class definitions
class ModuleVideoRenderImpl: public VideoRender
{
public:
    /*
     *   VideoRenderer constructor/destructor
     */
    ModuleVideoRenderImpl(const int32_t id,
                          const VideoRenderType videoRenderType,
                          void* window, const bool fullscreen);

    virtual ~ModuleVideoRenderImpl();

    virtual int64_t TimeUntilNextProcess();
    virtual int32_t Process();

    /*
     *   Returns the render window
     */
    virtual void* Window();

    /*
     *   Change render window
     */
    virtual int32_t ChangeWindow(void* window);

    /*
     *   Returns module id
     */
    int32_t Id();

    /**************************************************************************
     *
     *   Incoming Streams
     *
     ***************************************************************************/

    /*
     *   Add incoming render stream
     */
    virtual VideoRenderCallback
            * AddIncomingRenderStream(const uint32_t streamId,
                                      const uint32_t zOrder,
                                      const float left, const float top,
                                      const float right, const float bottom);
    /*
     *   Delete incoming render stream
     */
    virtual int32_t
            DeleteIncomingRenderStream(const uint32_t streamId);

    /*
     *   Add incoming render callback, used for external rendering
     */
    virtual int32_t
            AddExternalRenderCallback(const uint32_t streamId,
                                      VideoRenderCallback* renderObject);

    /*
     *   Get the porperties for an incoming render stream
     */
    virtual int32_t
            GetIncomingRenderStreamProperties(const uint32_t streamId,
                                              uint32_t& zOrder,
                                              float& left, float& top,
                                              float& right, float& bottom) const;
    /*
     *   Incoming frame rate for the specified stream.
     */
    virtual uint32_t GetIncomingFrameRate(const uint32_t streamId);

    /*
     *   Returns the number of incoming streams added to this render module
     */
    virtual uint32_t GetNumIncomingRenderStreams() const;

    /*
     *   Returns true if this render module has the streamId added, false otherwise.
     */
    virtual bool HasIncomingRenderStream(const uint32_t streamId) const;

    /*
     *
     */
    virtual int32_t
            RegisterRawFrameCallback(const uint32_t streamId,
                                     VideoRenderCallback* callbackObj);

    virtual int32_t SetExpectedRenderDelay(uint32_t stream_id,
                                           int32_t delay_ms);

    /**************************************************************************
     *
     *   Start/Stop
     *
     ***************************************************************************/

    /*
     *   Starts rendering the specified stream
     */
    virtual int32_t StartRender(const uint32_t streamId);

    /*
     *   Stops the renderer
     */
    virtual int32_t StopRender(const uint32_t streamId);

    /*
     *   Sets the renderer in start state, no streams removed.
     */
    virtual int32_t ResetRender();

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
    virtual int32_t
            GetScreenResolution(uint32_t& screenWidth,
                                uint32_t& screenHeight) const;

    /*
     *   Get the actual render rate for this stream. I.e rendered frame rate,
     *   not frames delivered to the renderer.
     */
    virtual uint32_t RenderFrameRate(const uint32_t streamId);

    /*
     *   Set cropping of incoming stream
     */
    virtual int32_t SetStreamCropping(const uint32_t streamId,
                                      const float left, const float top,
                                      const float right, const float bottom);

    virtual int32_t ConfigureRenderer(const uint32_t streamId,
                                      const unsigned int zOrder,
                                      const float left, const float top,
                                      const float right, const float bottom);

    virtual int32_t SetTransparentBackground(const bool enable);

    virtual int32_t FullScreenRender(void* window, const bool enable);

    virtual int32_t SetBitmap(const void* bitMap,
                              const uint8_t pictureId,
                              const void* colorKey,
                              const float left, const float top,
                              const float right, const float bottom);

    virtual int32_t SetText(const uint8_t textId,
                            const uint8_t* text,
                            const int32_t textLength,
                            const uint32_t textColorRef,
                            const uint32_t backgroundColorRef,
                            const float left, const float top,
                            const float right, const float bottom);

    virtual int32_t SetStartImage(const uint32_t streamId,
                                  const VideoFrame& videoFrame);

    virtual int32_t SetTimeoutImage(const uint32_t streamId,
                                    const VideoFrame& videoFrame,
                                    const uint32_t timeout);

private:
    int32_t _id;
    CriticalSectionWrapper& _moduleCrit;
    void* _ptrWindow;
    bool _fullScreen;

    IVideoRender* _ptrRenderer;
    typedef std::map<uint32_t, IncomingVideoStream*> IncomingVideoStreamMap;
    IncomingVideoStreamMap _streamRenderMap;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_VIDEO_RENDER_IMPL_H_
