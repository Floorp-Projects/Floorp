/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_COMMON_VIDEO_INCLUDE_INCOMING_VIDEO_STREAM_H_
#define WEBRTC_COMMON_VIDEO_INCLUDE_INCOMING_VIDEO_STREAM_H_

#include "webrtc/base/platform_thread.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/common_video/video_render_frames.h"

namespace webrtc {
class CriticalSectionWrapper;
class EventTimerWrapper;

class VideoRenderCallback {
 public:
  virtual int32_t RenderFrame(const uint32_t streamId,
                              const VideoFrame& videoFrame) = 0;

 protected:
  virtual ~VideoRenderCallback() {}
};

class IncomingVideoStream : public VideoRenderCallback {
 public:
  IncomingVideoStream(uint32_t stream_id, bool disable_prerenderer_smoothing);
  ~IncomingVideoStream();

  // Get callback to deliver frames to the module.
  VideoRenderCallback* ModuleCallback();
  virtual int32_t RenderFrame(const uint32_t stream_id,
                              const VideoFrame& video_frame);

  // Set callback to the platform dependent code.
  void SetRenderCallback(VideoRenderCallback* render_callback);

  // Callback for file recording, snapshot, ...
  void SetExternalCallback(VideoRenderCallback* render_object);

  // Start/Stop.
  int32_t Start();
  int32_t Stop();

  // Clear all buffers.
  int32_t Reset();

  // Properties.
  uint32_t StreamId() const;
  uint32_t IncomingRate() const;

  int32_t SetStartImage(const VideoFrame& video_frame);

  int32_t SetTimeoutImage(const VideoFrame& video_frame,
                          const uint32_t timeout);

  int32_t SetExpectedRenderDelay(int32_t delay_ms);

 protected:
  static bool IncomingVideoStreamThreadFun(void* obj);
  bool IncomingVideoStreamProcess();

 private:
  enum { kEventStartupTimeMs = 10 };
  enum { kEventMaxWaitTimeMs = 100 };
  enum { kFrameRatePeriodMs = 1000 };

  void DeliverFrame(const VideoFrame& video_frame);

  uint32_t const stream_id_;
  const bool disable_prerenderer_smoothing_;
  // Critsects in allowed to enter order.
  const rtc::scoped_ptr<CriticalSectionWrapper> stream_critsect_;
  const rtc::scoped_ptr<CriticalSectionWrapper> thread_critsect_;
  const rtc::scoped_ptr<CriticalSectionWrapper> buffer_critsect_;
  // TODO(pbos): Make plain member and stop resetting this thread, just
  // start/stoping it is enough.
  rtc::scoped_ptr<rtc::PlatformThread> incoming_render_thread_
      GUARDED_BY(thread_critsect_);
  rtc::scoped_ptr<EventTimerWrapper> deliver_buffer_event_;

  bool running_ GUARDED_BY(stream_critsect_);
  VideoRenderCallback* external_callback_ GUARDED_BY(thread_critsect_);
  VideoRenderCallback* render_callback_ GUARDED_BY(thread_critsect_);
  const rtc::scoped_ptr<VideoRenderFrames> render_buffers_
      GUARDED_BY(buffer_critsect_);

  uint32_t incoming_rate_ GUARDED_BY(stream_critsect_);
  int64_t last_rate_calculation_time_ms_ GUARDED_BY(stream_critsect_);
  uint16_t num_frames_since_last_calculation_ GUARDED_BY(stream_critsect_);
  int64_t last_render_time_ms_ GUARDED_BY(thread_critsect_);
  VideoFrame temp_frame_ GUARDED_BY(thread_critsect_);
  VideoFrame start_image_ GUARDED_BY(thread_critsect_);
  VideoFrame timeout_image_ GUARDED_BY(thread_critsect_);
  uint32_t timeout_time_ GUARDED_BY(thread_critsect_);
};

}  // namespace webrtc

#endif  // WEBRTC_COMMON_VIDEO_INCLUDE_INCOMING_VIDEO_STREAM_H_
