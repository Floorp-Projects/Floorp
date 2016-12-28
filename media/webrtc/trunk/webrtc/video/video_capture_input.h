/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_VIDEO_CAPTURE_INPUT_H_
#define WEBRTC_VIDEO_VIDEO_CAPTURE_INPUT_H_

#include <vector>

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/event.h"
#include "webrtc/base/platform_thread.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/common_types.h"
#include "webrtc/engine_configurations.h"
#include "webrtc/modules/video_capture/video_capture.h"
#include "webrtc/modules/video_coding/include/video_codec_interface.h"
#include "webrtc/modules/video_coding/include/video_coding.h"
#include "webrtc/modules/video_processing/include/video_processing.h"
#include "webrtc/system_wrappers/include/critical_section_wrapper.h"
#include "webrtc/typedefs.h"
#include "webrtc/video_send_stream.h"

namespace webrtc {

class Config;
class CpuOveruseMetricsObserver;
class CpuOveruseObserver;
class CriticalSectionWrapper;
class OveruseFrameDetector;
class ProcessThread;
class RegistrableCpuOveruseMetricsObserver;
class SendStatisticsProxy;
class VideoRenderer;

class VideoCaptureCallback {
 public:
  virtual ~VideoCaptureCallback() {}

  virtual void DeliverFrame(VideoFrame video_frame) = 0;
};

namespace internal {
class VideoCaptureInput : public webrtc::VideoCaptureInput {
 public:
  VideoCaptureInput(ProcessThread* module_process_thread,
                    VideoCaptureCallback* frame_callback,
                    VideoRenderer* local_renderer,
                    SendStatisticsProxy* send_stats_proxy,
                    CpuOveruseObserver* overuse_observer,
                    EncodingTimeObserver* encoding_time_observer);
  ~VideoCaptureInput();

  void IncomingCapturedFrame(const VideoFrame& video_frame) override;

 private:
  // Thread functions for deliver captured frames to receivers.
  static bool EncoderThreadFunction(void* obj);
  bool EncoderProcess();

  rtc::scoped_ptr<CriticalSectionWrapper> capture_cs_;
  ProcessThread* const module_process_thread_;

  VideoCaptureCallback* const frame_callback_;
  VideoRenderer* const local_renderer_;
  SendStatisticsProxy* const stats_proxy_;

  // Frame used in IncomingFrameI420.
  rtc::scoped_ptr<CriticalSectionWrapper> incoming_frame_cs_;
  VideoFrame incoming_frame_;

  rtc::PlatformThread encoder_thread_;
  rtc::Event capture_event_;

  volatile int stop_;

  VideoFrame captured_frame_ GUARDED_BY(capture_cs_.get());
  // Used to make sure incoming time stamp is increasing for every frame.
  int64_t last_captured_timestamp_;
  // Delta used for translating between NTP and internal timestamps.
  const int64_t delta_ntp_internal_ms_;

  rtc::scoped_ptr<OveruseFrameDetector> overuse_detector_;
  EncodingTimeObserver* const encoding_time_observer_;
};

}  // namespace internal
}  // namespace webrtc

#endif  // WEBRTC_VIDEO_VIDEO_CAPTURE_INPUT_H_
