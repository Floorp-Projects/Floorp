/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This sub-API supports the following functionalities:
//  - Specify render destinations for incoming video streams, capture devices
//    and files.
//  - Configuring render streams.

#ifndef WEBRTC_VIDEO_ENGINE_INCLUDE_VIE_RENDER_H_
#define WEBRTC_VIDEO_ENGINE_INCLUDE_VIE_RENDER_H_

#include "webrtc/common_types.h"

namespace webrtc {

class VideoEngine;
class VideoRender;
class VideoRenderCallback;

// This class declares an abstract interface to be used for external renderers.
// The user implemented derived class is registered using AddRenderer().
class ExternalRenderer {
 public:
  // This method will be called when the stream to be rendered changes in
  // resolution or number of streams mixed in the image.
  virtual int FrameSizeChange(unsigned int width,
                              unsigned int height,
                              unsigned int number_of_streams) = 0;

  // This method is called when a new frame should be rendered.
  virtual int DeliverFrame(unsigned char* buffer,
                           int buffer_size,
                           // RTP timestamp in 90kHz.
                           uint32_t time_stamp,
                           // Wallclock render time in miliseconds
                           int64_t render_time,
                           // Handle of the underlying video frame,
                           void* handle) = 0;

  // Returns true if the renderer supports textures. DeliverFrame can be called
  // with NULL |buffer| and non-NULL |handle|.
  virtual bool IsTextureSupported() = 0;

 protected:
  virtual ~ExternalRenderer() {}
};

class ViERender {
 public:
  // Factory for the ViERender sub‐API and increases an internal reference
  // counter if successful. Returns NULL if the API is not supported or if
  // construction fails.
  static ViERender* GetInterface(VideoEngine* video_engine);

  // Releases the ViERender sub-API and decreases an internal reference
  // counter. Returns the new reference count. This value should be zero
  // for all sub-API:s before the VideoEngine object can be safely deleted.
  virtual int Release() = 0;

  // Registers render module.
  virtual int RegisterVideoRenderModule(VideoRender& render_module) = 0;

  // Deregisters render module.
  virtual int DeRegisterVideoRenderModule(VideoRender& render_module) = 0;

  // Sets the render destination for a given render ID.
  virtual int AddRenderer(const int render_id,
                          void* window,
                          const unsigned int z_order,
                          const float left,
                          const float top,
                          const float right,
                          const float bottom) = 0;

  // Removes the renderer for a stream.
  virtual int RemoveRenderer(const int render_id) = 0;

  // Starts rendering a render stream.
  virtual int StartRender(const int render_id) = 0;

  // Stops rendering a render stream.
  virtual int StopRender(const int render_id) = 0;

  // Set expected render time needed by graphics card or external renderer, i.e.
  // the number of ms a frame will be sent to rendering before the actual render
  // time.
  virtual int SetExpectedRenderDelay(int render_id, int render_delay) = 0;

  // Configures an already added render stream.
  virtual int ConfigureRender(int render_id,
                              const unsigned int z_order,
                              const float left,
                              const float top,
                              const float right,
                              const float bottom) = 0;

  // This function mirrors the rendered stream left and right or up and down.
  virtual int MirrorRenderStream(const int render_id,
                                 const bool enable,
                                 const bool mirror_xaxis,
                                 const bool mirror_yaxis) = 0;

  // External render.
  virtual int AddRenderer(const int render_id,
                          RawVideoType video_input_format,
                          ExternalRenderer* renderer) = 0;

  // Propagating VideoRenderCallback down to the VideoRender module for new API.
  // Contains default-implementation not to break code mocking this interface.
  // (Ugly, but temporary.)
  virtual int AddRenderCallback(int render_id, VideoRenderCallback* callback) {
    return 0;
  }

 protected:
  ViERender() {}
  virtual ~ViERender() {}
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_INCLUDE_VIE_RENDER_H_
