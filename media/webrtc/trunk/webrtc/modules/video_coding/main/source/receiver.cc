/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/main/source/receiver.h"

#include <assert.h>

#include "webrtc/modules/video_coding/main/interface/video_coding.h"
#include "webrtc/modules/video_coding/main/source/encoded_frame.h"
#include "webrtc/modules/video_coding/main/source/internal_defines.h"
#include "webrtc/modules/video_coding/main/source/media_opt_util.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/trace.h"
#include "webrtc/system_wrappers/interface/trace_event.h"

namespace webrtc {

enum { kMaxReceiverDelayMs = 10000 };

VCMReceiver::VCMReceiver(VCMTiming* timing,
                         Clock* clock,
                         EventFactory* event_factory,
                         int32_t vcm_id,
                         int32_t receiver_id,
                         bool master)
    : crit_sect_(CriticalSectionWrapper::CreateCriticalSection()),
      vcm_id_(vcm_id),
      clock_(clock),
      receiver_id_(receiver_id),
      master_(master),
      jitter_buffer_(clock_, event_factory, vcm_id, receiver_id, master),
      timing_(timing),
      render_wait_event_(event_factory->CreateEvent()),
      state_(kPassive),
      max_video_delay_ms_(kMaxVideoDelayMs) {}

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
  render_wait_event_->Reset();
  if (master_) {
    state_ = kReceiving;
  } else {
    state_ = kPassive;
  }
}

int32_t VCMReceiver::Initialize() {
  CriticalSectionScoped cs(crit_sect_);
  Reset();
  if (!master_) {
    SetNackMode(kNoNack, -1, -1);
  }
  return VCM_OK;
}

void VCMReceiver::UpdateRtt(uint32_t rtt) {
  jitter_buffer_.UpdateRtt(rtt);
}

int32_t VCMReceiver::InsertPacket(const VCMPacket& packet,
                                  uint16_t frame_width,
                                  uint16_t frame_height) {
  // Find an empty frame.
  VCMEncodedFrame* buffer = NULL;
  const int32_t error = jitter_buffer_.GetFrame(packet, buffer);
  if (error == VCM_OLD_PACKET_ERROR) {
    return VCM_OK;
  } else if (error != VCM_OK) {
    return error;
  }
  assert(buffer);
  {
    CriticalSectionScoped cs(crit_sect_);

    if (frame_width && frame_height) {
      buffer->SetEncodedSize(static_cast<uint32_t>(frame_width),
                             static_cast<uint32_t>(frame_height));
    }

    if (master_) {
      // Only trace the primary receiver to make it possible to parse and plot
      // the trace file.
      WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding,
                   VCMId(vcm_id_, receiver_id_),
                   "Packet seq_no %u of frame %u at %u",
                   packet.seqNum, packet.timestamp,
                   MaskWord64ToUWord32(clock_->TimeInMilliseconds()));
    }

    const int64_t now_ms = clock_->TimeInMilliseconds();

    int64_t render_time_ms = timing_->RenderTimeMs(packet.timestamp, now_ms);

    if (render_time_ms < 0) {
      // Render time error. Assume that this is due to some change in the
      // incoming video stream and reset the JB and the timing.
      jitter_buffer_.Flush();
      timing_->Reset(clock_->TimeInMilliseconds());
      return VCM_FLUSH_INDICATOR;
    } else if (render_time_ms < now_ms - max_video_delay_ms_) {
      WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideoCoding,
                   VCMId(vcm_id_, receiver_id_),
                   "This frame should have been rendered more than %u ms ago."
                   "Flushing jitter buffer and resetting timing.",
                   max_video_delay_ms_);
      jitter_buffer_.Flush();
      timing_->Reset(clock_->TimeInMilliseconds());
      return VCM_FLUSH_INDICATOR;
    } else if (static_cast<int>(timing_->TargetVideoDelay()) >
               max_video_delay_ms_) {
      WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideoCoding,
                   VCMId(vcm_id_, receiver_id_),
                   "More than %u ms target delay. Flushing jitter buffer and"
                   "resetting timing.", max_video_delay_ms_);
      jitter_buffer_.Flush();
      timing_->Reset(clock_->TimeInMilliseconds());
      return VCM_FLUSH_INDICATOR;
    }

    // First packet received belonging to this frame.
    if (buffer->Length() == 0) {
      const int64_t now_ms = clock_->TimeInMilliseconds();
      if (master_) {
        // Only trace the primary receiver to make it possible to parse and plot
        // the trace file.
        WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding,
                     VCMId(vcm_id_, receiver_id_),
                     "First packet of frame %u at %u", packet.timestamp,
                     MaskWord64ToUWord32(now_ms));
      }
      render_time_ms = timing_->RenderTimeMs(packet.timestamp, now_ms);
      if (render_time_ms >= 0) {
        buffer->SetRenderTime(render_time_ms);
      } else {
        buffer->SetRenderTime(now_ms);
      }
    }

    // Insert packet into the jitter buffer both media and empty packets.
    const VCMFrameBufferEnum
    ret = jitter_buffer_.InsertPacket(buffer, packet);
    if (ret == kFlushIndicator) {
      return VCM_FLUSH_INDICATOR;
    } else if (ret < 0) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCoding,
                   VCMId(vcm_id_, receiver_id_),
                   "Error inserting packet seq_no=%u, time_stamp=%u",
                   packet.seqNum, packet.timestamp);
      return VCM_JITTER_BUFFER_ERROR;
    }
  }
  return VCM_OK;
}

VCMEncodedFrame* VCMReceiver::FrameForDecoding(
    uint16_t max_wait_time_ms,
    int64_t& next_render_time_ms,
    bool render_timing,
    VCMReceiver* dual_receiver) {
  TRACE_EVENT0("webrtc", "Recv::FrameForDecoding");
  // No need to enter the critical section here since the jitter buffer
  // is thread-safe.
  FrameType incoming_frame_type = kVideoFrameDelta;
  next_render_time_ms = -1;
  const int64_t start_time_ms = clock_->TimeInMilliseconds();
  int64_t ret = jitter_buffer_.NextTimestamp(max_wait_time_ms,
                                             &incoming_frame_type,
                                             &next_render_time_ms);
  if (ret < 0) {
    // No timestamp in jitter buffer at the moment.
    return NULL;
  }
  const uint32_t time_stamp = static_cast<uint32_t>(ret);

  // Update the timing.
  timing_->SetRequiredDelay(jitter_buffer_.EstimatedJitterMs());
  timing_->UpdateCurrentDelay(time_stamp);

  const int32_t temp_wait_time = max_wait_time_ms -
      static_cast<int32_t>(clock_->TimeInMilliseconds() - start_time_ms);
  uint16_t new_max_wait_time = static_cast<uint16_t>(VCM_MAX(temp_wait_time,
                                                             0));

  VCMEncodedFrame* frame = NULL;

  if (render_timing) {
    frame = FrameForDecoding(new_max_wait_time, next_render_time_ms,
                             dual_receiver);
  } else {
    frame = FrameForRendering(new_max_wait_time, next_render_time_ms,
                              dual_receiver);
  }

  if (frame != NULL) {
    bool retransmitted = false;
    const int64_t last_packet_time_ms =
      jitter_buffer_.LastPacketTime(frame, &retransmitted);
    if (last_packet_time_ms >= 0 && !retransmitted) {
      // We don't want to include timestamps which have suffered from
      // retransmission here, since we compensate with extra retransmission
      // delay within the jitter estimate.
      timing_->IncomingTimestamp(time_stamp, last_packet_time_ms);
    }
    if (dual_receiver != NULL) {
      dual_receiver->UpdateState(*frame);
    }
  }
  return frame;
}

VCMEncodedFrame* VCMReceiver::FrameForDecoding(
    uint16_t max_wait_time_ms,
    int64_t next_render_time_ms,
    VCMReceiver* dual_receiver) {
  TRACE_EVENT1("webrtc", "FrameForDecoding",
               "max_wait", max_wait_time_ms);
  // How long can we wait until we must decode the next frame.
  uint32_t wait_time_ms = timing_->MaxWaitingTime(
      next_render_time_ms, clock_->TimeInMilliseconds());

  // Try to get a complete frame from the jitter buffer.
  VCMEncodedFrame* frame = jitter_buffer_.GetCompleteFrameForDecoding(0);

  if (frame == NULL && max_wait_time_ms == 0 && wait_time_ms > 0) {
    // If we're not allowed to wait for frames to get complete we must
    // calculate if it's time to decode, and if it's not we will just return
    // for now.
    return NULL;
  }

  if (frame == NULL && VCM_MIN(wait_time_ms, max_wait_time_ms) == 0) {
    // No time to wait for a complete frame, check if we have an incomplete.
    const bool dual_receiver_enabled_and_passive = (dual_receiver != NULL &&
        dual_receiver->State() == kPassive &&
        dual_receiver->NackMode() == kNack);
    if (dual_receiver_enabled_and_passive &&
        !jitter_buffer_.CompleteSequenceWithNextFrame()) {
      // Jitter buffer state might get corrupt with this frame.
      dual_receiver->CopyJitterBufferStateFromReceiver(*this);
    }
    frame = jitter_buffer_.MaybeGetIncompleteFrameForDecoding();
  }
  if (frame == NULL) {
    // Wait for a complete frame.
    frame = jitter_buffer_.GetCompleteFrameForDecoding(max_wait_time_ms);
  }
  if (frame == NULL) {
    // Get an incomplete frame.
    if (timing_->MaxWaitingTime(next_render_time_ms,
                                clock_->TimeInMilliseconds()) > 0) {
      // Still time to wait for a complete frame.
      return NULL;
    }

    // No time left to wait, we must decode this frame now.
    const bool dual_receiver_enabled_and_passive = (dual_receiver != NULL &&
        dual_receiver->State() == kPassive &&
        dual_receiver->NackMode() == kNack);
    if (dual_receiver_enabled_and_passive &&
        !jitter_buffer_.CompleteSequenceWithNextFrame()) {
      // Jitter buffer state might get corrupt with this frame.
      dual_receiver->CopyJitterBufferStateFromReceiver(*this);
    }

    frame = jitter_buffer_.MaybeGetIncompleteFrameForDecoding();
  }
  return frame;
}

VCMEncodedFrame* VCMReceiver::FrameForRendering(uint16_t max_wait_time_ms,
                                                int64_t next_render_time_ms,
                                                VCMReceiver* dual_receiver) {
  TRACE_EVENT0("webrtc", "FrameForRendering");
  // How long MUST we wait until we must decode the next frame. This is
  // different for the case where we have a renderer which can render at a
  // specified time. Here we must wait as long as possible before giving the
  // frame to the decoder, which will render the frame as soon as it has been
  // decoded.
  uint32_t wait_time_ms = timing_->MaxWaitingTime(
      next_render_time_ms, clock_->TimeInMilliseconds());
  if (max_wait_time_ms < wait_time_ms) {
    // If we're not allowed to wait until the frame is supposed to be rendered,
    // waiting as long as we're allowed to avoid busy looping, and then return
    // NULL. Next call to this function might return the frame.
    render_wait_event_->Wait(max_wait_time_ms);
    return NULL;
  }
  // Wait until it's time to render.
  render_wait_event_->Wait(wait_time_ms);

  // Get a complete frame if possible.
  // Note: This might cause us to wait more than a total of |max_wait_time_ms|.
  // This is necessary to avoid a possible busy loop if no complete frame
  // has been received.
  VCMEncodedFrame* frame = jitter_buffer_.GetCompleteFrameForDecoding(
      max_wait_time_ms);

  if (frame == NULL) {
    // Get an incomplete frame.
    const bool dual_receiver_enabled_and_passive = (dual_receiver != NULL &&
        dual_receiver->State() == kPassive &&
        dual_receiver->NackMode() == kNack);
    if (dual_receiver_enabled_and_passive &&
        !jitter_buffer_.CompleteSequenceWithNextFrame()) {
      // Jitter buffer state might get corrupt with this frame.
      dual_receiver->CopyJitterBufferStateFromReceiver(*this);
    }

    frame = jitter_buffer_.MaybeGetIncompleteFrameForDecoding();
  }
  return frame;
}

void VCMReceiver::ReleaseFrame(VCMEncodedFrame* frame) {
  jitter_buffer_.ReleaseFrame(frame);
}

void VCMReceiver::ReceiveStatistics(uint32_t* bitrate,
                                    uint32_t* framerate) {
  assert(bitrate);
  assert(framerate);
  jitter_buffer_.IncomingRateStatistics(framerate, bitrate);
}

void VCMReceiver::ReceivedFrameCount(VCMFrameCount* frame_count) const {
  assert(frame_count);
  jitter_buffer_.FrameStatistics(&frame_count->numDeltaFrames,
                                 &frame_count->numKeyFrames);
}

uint32_t VCMReceiver::DiscardedPackets() const {
  return jitter_buffer_.num_discarded_packets();
}

void VCMReceiver::SetNackMode(VCMNackMode nackMode,
                              int low_rtt_nack_threshold_ms,
                              int high_rtt_nack_threshold_ms) {
  CriticalSectionScoped cs(crit_sect_);
  // Default to always having NACK enabled in hybrid mode.
  jitter_buffer_.SetNackMode(nackMode, low_rtt_nack_threshold_ms,
                             high_rtt_nack_threshold_ms);
  if (!master_) {
    state_ = kPassive;  // The dual decoder defaults to passive.
  }
}

void VCMReceiver::SetNackSettings(size_t max_nack_list_size,
                                  int max_packet_age_to_nack) {
  jitter_buffer_.SetNackSettings(max_nack_list_size,
                                 max_packet_age_to_nack);
}

VCMNackMode VCMReceiver::NackMode() const {
  CriticalSectionScoped cs(crit_sect_);
  return jitter_buffer_.nack_mode();
}

VCMNackStatus VCMReceiver::NackList(uint16_t* nack_list,
                                    uint16_t size,
                                    uint16_t* nack_list_length) {
  bool request_key_frame = false;
  uint16_t* internal_nack_list = jitter_buffer_.GetNackList(
      nack_list_length, &request_key_frame);
  if (request_key_frame) {
    // This combination is used to trigger key frame requests.
    return kNackKeyFrameRequest;
  }
  if (*nack_list_length > size) {
    return kNackNeedMoreMemory;
  }
  if (internal_nack_list != NULL && *nack_list_length > 0) {
    memcpy(nack_list, internal_nack_list, *nack_list_length * sizeof(uint16_t));
  }
  return kNackOk;
}

// Decide whether we should change decoder state. This should be done if the
// dual decoder has caught up with the decoder decoding with packet losses.
bool VCMReceiver::DualDecoderCaughtUp(VCMEncodedFrame* dual_frame,
                                      VCMReceiver& dual_receiver) const {
  if (dual_frame == NULL) {
    return false;
  }
  if (jitter_buffer_.LastDecodedTimestamp() == dual_frame->TimeStamp()) {
    dual_receiver.UpdateState(kWaitForPrimaryDecode);
    return true;
  }
  return false;
}

void VCMReceiver::CopyJitterBufferStateFromReceiver(
    const VCMReceiver& receiver) {
  jitter_buffer_.CopyFrom(receiver.jitter_buffer_);
}

VCMReceiverState VCMReceiver::State() const {
  CriticalSectionScoped cs(crit_sect_);
  return state_;
}

void VCMReceiver::SetDecodeWithErrors(bool enable){
  CriticalSectionScoped cs(crit_sect_);
  jitter_buffer_.DecodeWithErrors(enable);
}

bool VCMReceiver::DecodeWithErrors() const {
  CriticalSectionScoped cs(crit_sect_);
  return jitter_buffer_.decode_with_errors();
}

int VCMReceiver::SetMinReceiverDelay(int desired_delay_ms) {
  CriticalSectionScoped cs(crit_sect_);
  if (desired_delay_ms < 0 || desired_delay_ms > kMaxReceiverDelayMs) {
    return -1;
  }
  jitter_buffer_.SetMaxJitterEstimate(desired_delay_ms);
  max_video_delay_ms_ = desired_delay_ms + kMaxVideoDelayMs;
  timing_->SetMaxVideoDelay(max_video_delay_ms_);
  // Initializing timing to the desired delay.
  timing_->SetRequiredDelay(desired_delay_ms);
  return 0;
}

int VCMReceiver::RenderBufferSizeMs() {
  return jitter_buffer_.RenderBufferSizeMs();
}

void VCMReceiver::UpdateState(VCMReceiverState new_state) {
  CriticalSectionScoped cs(crit_sect_);
  assert(!(state_ == kPassive && new_state == kWaitForPrimaryDecode));
  state_ = new_state;
}

void VCMReceiver::UpdateState(const VCMEncodedFrame& frame) {
  if (jitter_buffer_.nack_mode() == kNoNack) {
    // Dual decoder mode has not been enabled.
    return;
  }
  // Update the dual receiver state.
  if (frame.Complete() && frame.FrameType() == kVideoFrameKey) {
    UpdateState(kPassive);
  }
  if (State() == kWaitForPrimaryDecode &&
      frame.Complete() && !frame.MissingFrame()) {
    UpdateState(kPassive);
  }
  if (frame.MissingFrame() || !frame.Complete()) {
    // State was corrupted, enable dual receiver.
    UpdateState(kReceiving);
  }
}
}  // namespace webrtc
