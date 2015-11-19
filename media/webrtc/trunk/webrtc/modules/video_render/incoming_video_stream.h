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

namespace webrtc {
class CriticalSectionWrapper;
class EventWrapper;
class ThreadWrapper;
class VideoRenderCallback;
class VideoRenderFrames;

class IncomingVideoStream : public VideoRenderCallback {
 public:
  IncomingVideoStream(const int32_t module_id,
                      const uint32_t stream_id);
  ~IncomingVideoStream();

  int32_t ChangeModuleId(const int32_t id);

  // Get callback to deliver frames to the module.
  VideoRenderCallback* ModuleCallback();
  virtual int32_t RenderFrame(const uint32_t stream_id,
                              const I420VideoFrame& video_frame);

  // Set callback to the platform dependent code.
  int32_t SetRenderCallback(VideoRenderCallback* render_callback);

  // Callback for file recording, snapshot, ...
  int32_t SetExternalCallback(VideoRenderCallback* render_object);

  // Start/Stop.
  int32_t Start();
  int32_t Stop();

  // Clear all buffers.
  int32_t Reset();

  // Properties.
  uint32_t StreamId() const;
  uint32_t IncomingRate() const;

  int32_t SetStartImage(const I420VideoFrame& video_frame);

  int32_t SetTimeoutImage(const I420VideoFrame& video_frame,
                          const uint32_t timeout);

  int32_t SetExpectedRenderDelay(int32_t delay_ms);

 protected:
  static bool IncomingVideoStreamThreadFun(void* obj);
  bool IncomingVideoStreamProcess();

 private:
  enum { KEventStartupTimeMS = 10 };
  enum { KEventMaxWaitTimeMs = 100 };
  enum { KFrameRatePeriodMs = 1000 };

  int32_t module_id_;
  uint32_t stream_id_;
  // Critsects in allowed to enter order.
  CriticalSectionWrapper& stream_critsect_;
  CriticalSectionWrapper& thread_critsect_;
  CriticalSectionWrapper& buffer_critsect_;
  rtc::scoped_ptr<ThreadWrapper> incoming_render_thread_;
  EventWrapper& deliver_buffer_event_;
  bool running_;

  VideoRenderCallback* external_callback_;
  VideoRenderCallback* render_callback_;
  VideoRenderFrames& render_buffers_;

  RawVideoType callbackVideoType_;
  uint32_t callbackWidth_;
  uint32_t callbackHeight_;

  uint32_t incoming_rate_;
  int64_t last_rate_calculation_time_ms_;
  uint16_t num_frames_since_last_calculation_;
  int64_t last_render_time_ms_;
  I420VideoFrame temp_frame_;
  I420VideoFrame start_image_;
  I420VideoFrame timeout_image_;
  uint32_t timeout_time_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_INCOMING_VIDEO_STREAM_H_
