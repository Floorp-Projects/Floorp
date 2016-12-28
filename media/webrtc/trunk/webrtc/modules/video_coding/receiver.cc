/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/receiver.h"

#include <assert.h>

#include <cstdlib>
#include <utility>
#include <vector>

#include "webrtc/base/logging.h"
#include "webrtc/base/trace_event.h"
#include "webrtc/modules/video_coding/encoded_frame.h"
#include "webrtc/modules/video_coding/internal_defines.h"
#include "webrtc/modules/video_coding/media_opt_util.h"
#include "webrtc/system_wrappers/include/clock.h"

namespace webrtc {

enum { kMaxReceiverDelayMs = 10000 };

VCMReceiver::VCMReceiver(VCMTiming* timing,
                         Clock* clock,
                         EventFactory* event_factory)
    : VCMReceiver(timing,
                  clock,
                  rtc::scoped_ptr<EventWrapper>(event_factory->CreateEvent()),
                  rtc::scoped_ptr<EventWrapper>(event_factory->CreateEvent())) {
}

VCMReceiver::VCMReceiver(VCMTiming* timing,
                         Clock* clock,
                         rtc::scoped_ptr<EventWrapper> receiver_event,
                         rtc::scoped_ptr<EventWrapper> jitter_buffer_event)
    : crit_sect_(CriticalSectionWrapper::CreateCriticalSection()),
      clock_(clock),
      jitter_buffer_(clock_, std::move(jitter_buffer_event)),
      timing_(timing),
      render_wait_event_(std::move(receiver_event)),
      receiveState_(kReceiveStateInitial),
      max_video_delay_ms_(kMaxVideoDelayMs) {
  Reset();
}

VCMReceiver::~VCMReceiver() {
  render_wait_event_->Set();
  delete crit_sect_;
}

void VCMReceiver::Reset() {
  CriticalSectionScoped cs(crit_sect_);
  if (!jitter_buffer_.Running()) {
    jitter_buffer_.Start();
  } else {
    jitter_buffer_.Flush();
  }
  receiveState_ = kReceiveStateInitial;
}

void VCMReceiver::UpdateRtt(int64_t rtt) {
  jitter_buffer_.UpdateRtt(rtt);
}

int32_t VCMReceiver::InsertPacket(const VCMPacket& packet,
                                  uint16_t frame_width,
                                  uint16_t frame_height) {
  // Insert the packet into the jitter buffer. The packet can either be empty or
  // contain media at this point.
  bool retransmitted = false;
  const VCMFrameBufferEnum ret =
      jitter_buffer_.InsertPacket(packet, &retransmitted);
  if (ret == kOldPacket) {
    return VCM_OK;
  } else if (ret == kFlushIndicator) {
    return VCM_FLUSH_INDICATOR;
  } else if (ret < 0) {
    return VCM_JITTER_BUFFER_ERROR;
  }
  if (ret == kCompleteSession && !retransmitted) {
    // We don't want to include timestamps which have suffered from
    // retransmission here, since we compensate with extra retransmission
    // delay within the jitter estimate.
    timing_->IncomingTimestamp(packet.timestamp, clock_->TimeInMilliseconds());
  }
  return VCM_OK;
}

void VCMReceiver::TriggerDecoderShutdown() {
  jitter_buffer_.Stop();
  render_wait_event_->Set();
}

VCMEncodedFrame* VCMReceiver::FrameForDecoding(uint16_t max_wait_time_ms,
                                               int64_t* next_render_time_ms,
                                               bool prefer_late_decoding) {
  const int64_t start_time_ms = clock_->TimeInMilliseconds();
  uint32_t frame_timestamp = 0;
  // Exhaust wait time to get a complete frame for decoding.
  bool found_frame =
      jitter_buffer_.NextCompleteTimestamp(max_wait_time_ms, &frame_timestamp);

  if (!found_frame)
    found_frame = jitter_buffer_.NextMaybeIncompleteTimestamp(&frame_timestamp);

  if (!found_frame)
    return NULL;

  // We have a frame - Set timing and render timestamp.
  timing_->SetJitterDelay(jitter_buffer_.EstimatedJitterMs());
  const int64_t now_ms = clock_->TimeInMilliseconds();
  timing_->UpdateCurrentDelay(frame_timestamp);
  *next_render_time_ms = timing_->RenderTimeMs(frame_timestamp, now_ms);
  // Check render timing.
  bool timing_error = false;
  // Assume that render timing errors are due to changes in the video stream.
  if (*next_render_time_ms < 0) {
    timing_error = true;
  } else if (std::abs(*next_render_time_ms - now_ms) > max_video_delay_ms_) {
    int frame_delay = std::abs(*next_render_time_ms - now_ms);
    LOG(LS_WARNING) << "A frame about to be decoded is out of the configured "
                    << "delay bounds (" << frame_delay << " > "
                    << max_video_delay_ms_
                    << "). Resetting the video jitter buffer.";
    timing_error = true;
  } else if (static_cast<int>(timing_->TargetVideoDelay()) >
             max_video_delay_ms_) {
    LOG(LS_WARNING) << "The video target delay has grown larger than "
                    << max_video_delay_ms_ << " ms. Resetting jitter buffer.";
    timing_error = true;
  }

  if (timing_error) {
    // Timing error => reset timing and flush the jitter buffer.
    jitter_buffer_.Flush();
    timing_->Reset();
    return NULL;
  }

  if (prefer_late_decoding) {
    // Decode frame as close as possible to the render timestamp.
    const int32_t available_wait_time =
        max_wait_time_ms -
        static_cast<int32_t>(clock_->TimeInMilliseconds() - start_time_ms);
    uint16_t new_max_wait_time =
        static_cast<uint16_t>(VCM_MAX(available_wait_time, 0));
    uint32_t wait_time_ms = timing_->MaxWaitingTime(
        *next_render_time_ms, clock_->TimeInMilliseconds());
    if (new_max_wait_time < wait_time_ms) {
      // We're not allowed to wait until the frame is supposed to be rendered,
      // waiting as long as we're allowed to avoid busy looping, and then return
      // NULL. Next call to this function might return the frame.
      render_wait_event_->Wait(new_max_wait_time);
      return NULL;
    }
    // Wait until it's time to render.
    render_wait_event_->Wait(wait_time_ms);
  }

  // Extract the frame from the jitter buffer and set the render time.
  VCMEncodedFrame* frame = jitter_buffer_.ExtractAndSetDecode(frame_timestamp);
  if (frame == NULL) {
    return NULL;
  }
  frame->SetRenderTime(*next_render_time_ms);
  TRACE_EVENT_ASYNC_STEP1("webrtc", "Video", frame->TimeStamp(), "SetRenderTS",
                          "render_time", *next_render_time_ms);
  UpdateReceiveState(*frame);
  if (!frame->Complete()) {
    // Update stats for incomplete frames.
    bool retransmitted = false;
    const int64_t last_packet_time_ms =
        jitter_buffer_.LastPacketTime(frame, &retransmitted);
    if (last_packet_time_ms >= 0 && !retransmitted) {
      // We don't want to include timestamps which have suffered from
      // retransmission here, since we compensate with extra retransmission
      // delay within the jitter estimate.
      timing_->IncomingTimestamp(frame_timestamp, last_packet_time_ms);
    }
  }
  return frame;
}

void VCMReceiver::ReleaseFrame(VCMEncodedFrame* frame) {
  jitter_buffer_.ReleaseFrame(frame);
}

void VCMReceiver::ReceiveStatistics(uint32_t* bitrate, uint32_t* framerate) {
  assert(bitrate);
  assert(framerate);
  jitter_buffer_.IncomingRateStatistics(framerate, bitrate);
}

uint32_t VCMReceiver::DiscardedPackets() const {
  return jitter_buffer_.num_discarded_packets();
}

void VCMReceiver::SetNackMode(VCMNackMode nackMode,
                              int64_t low_rtt_nack_threshold_ms,
                              int64_t high_rtt_nack_threshold_ms) {
  CriticalSectionScoped cs(crit_sect_);
  // Default to always having NACK enabled in hybrid mode.
  jitter_buffer_.SetNackMode(nackMode, low_rtt_nack_threshold_ms,
                             high_rtt_nack_threshold_ms);
}

void VCMReceiver::SetNackSettings(size_t max_nack_list_size,
                                  int max_packet_age_to_nack,
                                  int max_incomplete_time_ms) {
  jitter_buffer_.SetNackSettings(max_nack_list_size, max_packet_age_to_nack,
                                 max_incomplete_time_ms);
}

VCMNackMode VCMReceiver::NackMode() const {
  CriticalSectionScoped cs(crit_sect_);
  return jitter_buffer_.nack_mode();
}

std::vector<uint16_t> VCMReceiver::NackList(bool* request_key_frame) {
  return jitter_buffer_.GetNackList(request_key_frame);
}

VideoReceiveState VCMReceiver::ReceiveState() const {
  CriticalSectionScoped cs(crit_sect_);
  return receiveState_;
}

void VCMReceiver::SetDecodeErrorMode(VCMDecodeErrorMode decode_error_mode) {
  jitter_buffer_.SetDecodeErrorMode(decode_error_mode);
}

VCMDecodeErrorMode VCMReceiver::DecodeErrorMode() const {
  return jitter_buffer_.decode_error_mode();
}

int VCMReceiver::SetMinReceiverDelay(int desired_delay_ms) {
  CriticalSectionScoped cs(crit_sect_);
  if (desired_delay_ms < 0 || desired_delay_ms > kMaxReceiverDelayMs) {
    return -1;
  }
  max_video_delay_ms_ = desired_delay_ms + kMaxVideoDelayMs;
  // Initializing timing to the desired delay.
  timing_->set_min_playout_delay(desired_delay_ms);
  return 0;
}

int VCMReceiver::RenderBufferSizeMs() {
  uint32_t timestamp_start = 0u;
  uint32_t timestamp_end = 0u;
  // Render timestamps are computed just prior to decoding. Therefore this is
  // only an estimate based on frames' timestamps and current timing state.
  jitter_buffer_.RenderBufferSize(&timestamp_start, &timestamp_end);
  if (timestamp_start == timestamp_end) {
    return 0;
  }
  // Update timing.
  const int64_t now_ms = clock_->TimeInMilliseconds();
  timing_->SetJitterDelay(jitter_buffer_.EstimatedJitterMs());
  // Get render timestamps.
  uint32_t render_start = timing_->RenderTimeMs(timestamp_start, now_ms);
  uint32_t render_end = timing_->RenderTimeMs(timestamp_end, now_ms);
  return render_end - render_start;
}

void VCMReceiver::UpdateReceiveState(const VCMEncodedFrame& frame) {
  if (frame.Complete() && frame.FrameType() == kVideoFrameKey) {
    receiveState_ = kReceiveStateNormal;
    return;
  }
  if (frame.MissingFrame() || !frame.Complete()) {
    // State is corrupted
    receiveState_ = kReceiveStateWaitingKey;
  }
  // state continues
}

void VCMReceiver::RegisterStatsCallback(
    VCMReceiveStatisticsCallback* callback) {
  jitter_buffer_.RegisterStatsCallback(callback);
}

}  // namespace webrtc
