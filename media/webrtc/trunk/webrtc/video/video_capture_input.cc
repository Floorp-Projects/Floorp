/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video/video_capture_input.h"

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/trace_event.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/modules/utility/include/process_thread.h"
#include "webrtc/modules/video_capture/video_capture_factory.h"
#include "webrtc/modules/video_processing/include/video_processing.h"
#include "webrtc/modules/video_render/video_render_defines.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/system_wrappers/include/critical_section_wrapper.h"
#include "webrtc/system_wrappers/include/tick_util.h"
#include "webrtc/video/overuse_frame_detector.h"
#include "webrtc/video/send_statistics_proxy.h"
#include "webrtc/video/vie_encoder.h"

namespace webrtc {

namespace internal {
VideoCaptureInput::VideoCaptureInput(
    ProcessThread* module_process_thread,
    VideoCaptureCallback* frame_callback,
    VideoRenderer* local_renderer,
    SendStatisticsProxy* stats_proxy,
    CpuOveruseObserver* overuse_observer,
    EncodingTimeObserver* encoding_time_observer)
    : capture_cs_(CriticalSectionWrapper::CreateCriticalSection()),
      module_process_thread_(module_process_thread),
      frame_callback_(frame_callback),
      local_renderer_(local_renderer),
      stats_proxy_(stats_proxy),
      incoming_frame_cs_(CriticalSectionWrapper::CreateCriticalSection()),
      encoder_thread_(EncoderThreadFunction, this, "EncoderThread"),
      capture_event_(false, false),
      stop_(0),
      last_captured_timestamp_(0),
      delta_ntp_internal_ms_(
          Clock::GetRealTimeClock()->CurrentNtpInMilliseconds() -
          TickTime::MillisecondTimestamp()),
      overuse_detector_(new OveruseFrameDetector(Clock::GetRealTimeClock(),
                                                 CpuOveruseOptions(),
                                                 overuse_observer,
                                                 stats_proxy)),
      encoding_time_observer_(encoding_time_observer) {
  encoder_thread_.Start();
  encoder_thread_.SetPriority(rtc::kHighPriority);
  module_process_thread_->RegisterModule(overuse_detector_.get());
}

VideoCaptureInput::~VideoCaptureInput() {
  module_process_thread_->DeRegisterModule(overuse_detector_.get());

  // Stop the thread.
  rtc::AtomicOps::ReleaseStore(&stop_, 1);
  capture_event_.Set();
  encoder_thread_.Stop();
}

void VideoCaptureInput::IncomingCapturedFrame(const VideoFrame& video_frame) {
  // TODO(pbos): Remove local rendering, it should be handled by the client code
  // if required.
  if (local_renderer_)
    local_renderer_->RenderFrame(video_frame, 0);

  stats_proxy_->OnIncomingFrame(video_frame.width(), video_frame.height());

  VideoFrame incoming_frame = video_frame;

  if (incoming_frame.ntp_time_ms() != 0) {
    // If a NTP time stamp is set, this is the time stamp we will use.
    incoming_frame.set_render_time_ms(incoming_frame.ntp_time_ms() -
                                      delta_ntp_internal_ms_);
  } else {  // NTP time stamp not set.
    int64_t render_time = incoming_frame.render_time_ms() != 0
                              ? incoming_frame.render_time_ms()
                              : TickTime::MillisecondTimestamp();

    incoming_frame.set_render_time_ms(render_time);
    incoming_frame.set_ntp_time_ms(render_time + delta_ntp_internal_ms_);
  }

  // Convert NTP time, in ms, to RTP timestamp.
  const int kMsToRtpTimestamp = 90;
  incoming_frame.set_timestamp(
      kMsToRtpTimestamp * static_cast<uint32_t>(incoming_frame.ntp_time_ms()));

  CriticalSectionScoped cs(capture_cs_.get());
  if (incoming_frame.ntp_time_ms() <= last_captured_timestamp_) {
    // We don't allow the same capture time for two frames, drop this one.
    LOG(LS_WARNING) << "Same/old NTP timestamp ("
                    << incoming_frame.ntp_time_ms()
                    << " <= " << last_captured_timestamp_
                    << ") for incoming frame. Dropping.";
    return;
  }

  captured_frame_.ShallowCopy(incoming_frame);
  last_captured_timestamp_ = incoming_frame.ntp_time_ms();

  overuse_detector_->FrameCaptured(captured_frame_.width(),
                                   captured_frame_.height(),
                                   captured_frame_.render_time_ms());

  TRACE_EVENT_ASYNC_BEGIN1("webrtc", "Video", video_frame.render_time_ms(),
                           "render_time", video_frame.render_time_ms());

  capture_event_.Set();
}

bool VideoCaptureInput::EncoderThreadFunction(void* obj) {
  return static_cast<VideoCaptureInput*>(obj)->EncoderProcess();
}

bool VideoCaptureInput::EncoderProcess() {
  static const int kThreadWaitTimeMs = 100;
  int64_t capture_time = -1;
  if (capture_event_.Wait(kThreadWaitTimeMs)) {
    if (rtc::AtomicOps::AcquireLoad(&stop_))
      return false;

    int64_t encode_start_time = -1;
    VideoFrame deliver_frame;
    {
      CriticalSectionScoped cs(capture_cs_.get());
      if (!captured_frame_.IsZeroSize()) {
        deliver_frame = captured_frame_;
        captured_frame_.Reset();
      }
    }
    if (!deliver_frame.IsZeroSize()) {
      capture_time = deliver_frame.render_time_ms();
      encode_start_time = Clock::GetRealTimeClock()->TimeInMilliseconds();
      frame_callback_->DeliverFrame(deliver_frame);
    }
    // Update the overuse detector with the duration.
    if (encode_start_time != -1) {
      int encode_time_ms = static_cast<int>(
          Clock::GetRealTimeClock()->TimeInMilliseconds() - encode_start_time);
      stats_proxy_->OnEncodedFrame(encode_time_ms);
      if (encoding_time_observer_) {
        encoding_time_observer_->OnReportEncodedTime(
            deliver_frame.ntp_time_ms(), encode_time_ms);
      }
    }
  }
  // We're done!
  if (capture_time != -1) {
    overuse_detector_->FrameSent(capture_time);
  }
  return true;
}

}  // namespace internal
}  // namespace webrtc
