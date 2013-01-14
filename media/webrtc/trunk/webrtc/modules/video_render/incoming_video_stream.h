/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_INCOMING_VIDEO_STREAM_H_
#define WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_INCOMING_VIDEO_STREAM_H_

#include "webrtc/modules/video_render/include/video_render.h"
#include "system_wrappers/interface/map_wrapper.h"

namespace webrtc {
class CriticalSectionWrapper;
class EventWrapper;
class ThreadWrapper;
class VideoRenderCallback;
class VideoRenderFrames;

struct VideoMirroring {
  VideoMirroring() : mirror_x_axis(false), mirror_y_axis(false) {}
  bool mirror_x_axis;
  bool mirror_y_axis;
};

class IncomingVideoStream : public VideoRenderCallback {
 public:
  IncomingVideoStream(const WebRtc_Word32 module_id,
                      const WebRtc_UWord32 stream_id);
  ~IncomingVideoStream();

  WebRtc_Word32 ChangeModuleId(const WebRtc_Word32 id);

  // Get callback to deliver frames to the module.
  VideoRenderCallback* ModuleCallback();
  virtual WebRtc_Word32 RenderFrame(const WebRtc_UWord32 stream_id,
                                    I420VideoFrame& video_frame);

  // Set callback to the platform dependent code.
  WebRtc_Word32 SetRenderCallback(VideoRenderCallback* render_callback);

  // Callback for file recording, snapshot, ...
  WebRtc_Word32 SetExternalCallback(VideoRenderCallback* render_object);

  // Start/Stop.
  WebRtc_Word32 Start();
  WebRtc_Word32 Stop();

  // Clear all buffers.
  WebRtc_Word32 Reset();

  // Properties.
  WebRtc_UWord32 StreamId() const;
  WebRtc_UWord32 IncomingRate() const;

  WebRtc_Word32 GetLastRenderedFrame(I420VideoFrame& video_frame) const;

  WebRtc_Word32 SetStartImage(const I420VideoFrame& video_frame);

  WebRtc_Word32 SetTimeoutImage(const I420VideoFrame& video_frame,
                                const WebRtc_UWord32 timeout);

  WebRtc_Word32 EnableMirroring(const bool enable,
                                const bool mirror_xaxis,
                                const bool mirror_yaxis);

  WebRtc_Word32 SetExpectedRenderDelay(WebRtc_Word32 delay_ms);

 protected:
  static bool IncomingVideoStreamThreadFun(void* obj);
  bool IncomingVideoStreamProcess();

 private:
  enum { KEventStartupTimeMS = 10 };
  enum { KEventMaxWaitTimeMs = 100 };
  enum { KFrameRatePeriodMs = 1000 };

  WebRtc_Word32 module_id_;
  WebRtc_UWord32 stream_id_;
  // Critsects in allowed to enter order.
  CriticalSectionWrapper& stream_critsect_;
  CriticalSectionWrapper& thread_critsect_;
  CriticalSectionWrapper& buffer_critsect_;
  ThreadWrapper* incoming_render_thread_;
  EventWrapper& deliver_buffer_event_;
  bool running_;

  VideoRenderCallback* external_callback_;
  VideoRenderCallback* render_callback_;
  VideoRenderFrames& render_buffers_;

  RawVideoType callbackVideoType_;
  WebRtc_UWord32 callbackWidth_;
  WebRtc_UWord32 callbackHeight_;

  WebRtc_UWord32 incoming_rate_;
  WebRtc_Word64 last_rate_calculation_time_ms_;
  WebRtc_UWord16 num_frames_since_last_calculation_;
  I420VideoFrame last_rendered_frame_;
  I420VideoFrame temp_frame_;
  I420VideoFrame start_image_;
  I420VideoFrame timeout_image_;
  WebRtc_UWord32 timeout_time_;

  bool mirror_frames_enabled_;
  VideoMirroring mirroring_;
  I420VideoFrame transformed_video_frame_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_INCOMING_VIDEO_STREAM_H_
