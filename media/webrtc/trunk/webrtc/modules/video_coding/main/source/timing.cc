/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/main/source/timing.h"


#include "webrtc/modules/video_coding/main/source/internal_defines.h"
#include "webrtc/modules/video_coding/main/source/jitter_buffer_common.h"
#include "webrtc/modules/video_coding/main/source/timestamp_extrapolator.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/trace.h"



namespace webrtc {

VCMTiming::VCMTiming(Clock* clock,
                     int32_t vcm_id,
                     int32_t timing_id,
                     VCMTiming* master_timing)
    : crit_sect_(CriticalSectionWrapper::CreateCriticalSection()),
      vcm_id_(vcm_id),
      clock_(clock),
      timing_id_(timing_id),
      master_(false),
      ts_extrapolator_(),
      codec_timer_(),
      render_delay_ms_(kDefaultRenderDelayMs),
      min_playout_delay_ms_(0),
      jitter_delay_ms_(0),
      current_delay_ms_(0),
      prev_frame_timestamp_(0) {
  if (master_timing == NULL) {
    master_ = true;
    ts_extrapolator_ = new VCMTimestampExtrapolator(clock_, vcm_id, timing_id);
  } else {
    ts_extrapolator_ = master_timing->ts_extrapolator_;
  }
}

VCMTiming::~VCMTiming() {
  if (master_) {
    delete ts_extrapolator_;
  }
  delete crit_sect_;
}

void VCMTiming::Reset() {
  CriticalSectionScoped cs(crit_sect_);
  ts_extrapolator_->Reset();
  codec_timer_.Reset();
  render_delay_ms_ = kDefaultRenderDelayMs;
  min_playout_delay_ms_ = 0;
  jitter_delay_ms_ = 0;
  current_delay_ms_ = 0;
  prev_frame_timestamp_ = 0;
}

void VCMTiming::ResetDecodeTime() {
  codec_timer_.Reset();
}

void VCMTiming::set_render_delay(uint32_t render_delay_ms) {
  CriticalSectionScoped cs(crit_sect_);
  render_delay_ms_ = render_delay_ms;
}

void VCMTiming::set_min_playout_delay(uint32_t min_playout_delay_ms) {
  CriticalSectionScoped cs(crit_sect_);
  min_playout_delay_ms_ = min_playout_delay_ms;
}

void VCMTiming::SetJitterDelay(uint32_t jitter_delay_ms) {
  CriticalSectionScoped cs(crit_sect_);
  if (jitter_delay_ms != jitter_delay_ms_) {
    if (master_) {
      WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding,
          VCMId(vcm_id_, timing_id_),
          "Desired jitter buffer level: %u ms", jitter_delay_ms);
    }
    jitter_delay_ms_ = jitter_delay_ms;
    // When in initial state, set current delay to minimum delay.
    if (current_delay_ms_ == 0) {
      current_delay_ms_ = jitter_delay_ms_;
    }
  }
}

void VCMTiming::UpdateCurrentDelay(uint32_t frame_timestamp) {
  CriticalSectionScoped cs(crit_sect_);
  uint32_t target_delay_ms = TargetDelayInternal();

  if (current_delay_ms_ == 0) {
    // Not initialized, set current delay to target.
    current_delay_ms_ = target_delay_ms;
  } else if (target_delay_ms != current_delay_ms_) {
    int64_t delay_diff_ms = static_cast<int64_t>(target_delay_ms) -
        current_delay_ms_;
    // Never change the delay with more than 100 ms every second. If we're
    // changing the delay in too large steps we will get noticeable freezes. By
    // limiting the change we can increase the delay in smaller steps, which
    // will be experienced as the video is played in slow motion. When lowering
    // the delay the video will be played at a faster pace.
    int64_t max_change_ms = 0;
    if (frame_timestamp < 0x0000ffff && prev_frame_timestamp_ > 0xffff0000) {
      // wrap
      max_change_ms = kDelayMaxChangeMsPerS * (frame_timestamp +
          (static_cast<int64_t>(1) << 32) - prev_frame_timestamp_) / 90000;
    } else {
      max_change_ms = kDelayMaxChangeMsPerS *
          (frame_timestamp - prev_frame_timestamp_) / 90000;
    }
    if (max_change_ms <= 0) {
      // Any changes less than 1 ms are truncated and
      // will be postponed. Negative change will be due
      // to reordering and should be ignored.
      return;
    }
    delay_diff_ms = std::max(delay_diff_ms, -max_change_ms);
    delay_diff_ms = std::min(delay_diff_ms, max_change_ms);

    current_delay_ms_ = current_delay_ms_ + static_cast<int32_t>(delay_diff_ms);
  }
  prev_frame_timestamp_ = frame_timestamp;
}

void VCMTiming::UpdateCurrentDelay(int64_t render_time_ms,
                                   int64_t actual_decode_time_ms) {
  CriticalSectionScoped cs(crit_sect_);
  uint32_t target_delay_ms = TargetDelayInternal();
  int64_t delayed_ms = actual_decode_time_ms -
      (render_time_ms - MaxDecodeTimeMs() - render_delay_ms_);
  if (delayed_ms < 0) {
    return;
  }
  if (current_delay_ms_ + delayed_ms <= target_delay_ms) {
    current_delay_ms_ += static_cast<uint32_t>(delayed_ms);
  } else {
    current_delay_ms_ = target_delay_ms;
  }
}

int32_t VCMTiming::StopDecodeTimer(uint32_t time_stamp,
                                   int64_t start_time_ms,
                                   int64_t now_ms) {
  CriticalSectionScoped cs(crit_sect_);
  const int32_t max_dec_time = MaxDecodeTimeMs();
  int32_t time_diff_ms = codec_timer_.StopTimer(start_time_ms, now_ms);
  if (time_diff_ms < 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCoding, VCMId(vcm_id_,
        timing_id_), "Codec timer error: %d", time_diff_ms);
    assert(false);
  }

  if (master_) {
    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding, VCMId(vcm_id_,
        timing_id_),
        "Frame decoded: time_stamp=%u dec_time=%d max_dec_time=%u, at %u",
        time_stamp, time_diff_ms, max_dec_time, MaskWord64ToUWord32(now_ms));
  }
  return 0;
}

void VCMTiming::IncomingTimestamp(uint32_t time_stamp, int64_t now_ms) {
  CriticalSectionScoped cs(crit_sect_);
  ts_extrapolator_->Update(now_ms, time_stamp, master_);
}

int64_t VCMTiming::RenderTimeMs(uint32_t frame_timestamp, int64_t now_ms)
    const {
  CriticalSectionScoped cs(crit_sect_);
  const int64_t render_time_ms = RenderTimeMsInternal(frame_timestamp, now_ms);
  if (master_) {
    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding, VCMId(vcm_id_,
        timing_id_), "Render frame %u at %u. Render delay %u",
        "jitter delay %u, max decode time %u, playout delay %u",
        frame_timestamp, MaskWord64ToUWord32(render_time_ms), render_delay_ms_,
        jitter_delay_ms_, MaxDecodeTimeMs(), min_playout_delay_ms_);
  }
  return render_time_ms;
}

int64_t VCMTiming::RenderTimeMsInternal(uint32_t frame_timestamp,
                                        int64_t now_ms) const {
  int64_t estimated_complete_time_ms =
    ts_extrapolator_->ExtrapolateLocalTime(frame_timestamp);
  if (master_) {
    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding,
        VCMId(vcm_id_, timing_id_), "ExtrapolateLocalTime(%u)=%u ms",
        frame_timestamp, MaskWord64ToUWord32(estimated_complete_time_ms));
  }
  if (estimated_complete_time_ms == -1) {
    estimated_complete_time_ms = now_ms;
  }

  // Make sure that we have at least the playout delay.
  uint32_t actual_delay = std::max(current_delay_ms_, min_playout_delay_ms_);
  return estimated_complete_time_ms + actual_delay;
}

// Must be called from inside a critical section.
int32_t VCMTiming::MaxDecodeTimeMs(FrameType frame_type /*= kVideoFrameDelta*/)
    const {
  const int32_t decode_time_ms = codec_timer_.RequiredDecodeTimeMs(frame_type);
  if (decode_time_ms < 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCoding, VCMId(vcm_id_,
        timing_id_), "Negative maximum decode time: %d", decode_time_ms);
        return -1;
  }
  return decode_time_ms;
}

uint32_t VCMTiming::MaxWaitingTime(int64_t render_time_ms, int64_t now_ms)
    const {
  CriticalSectionScoped cs(crit_sect_);

  const int64_t max_wait_time_ms = render_time_ms - now_ms -
      MaxDecodeTimeMs() - render_delay_ms_;

  if (max_wait_time_ms < 0) {
    return 0;
  }
  return static_cast<uint32_t>(max_wait_time_ms);
}

bool VCMTiming::EnoughTimeToDecode(uint32_t available_processing_time_ms)
    const {
  CriticalSectionScoped cs(crit_sect_);
  int32_t max_decode_time_ms = MaxDecodeTimeMs();
  if (max_decode_time_ms < 0) {
    // Haven't decoded any frames yet, try decoding one to get an estimate
    // of the decode time.
    return true;
  } else if (max_decode_time_ms == 0) {
    // Decode time is less than 1, set to 1 for now since
    // we don't have any better precision. Count ticks later?
    max_decode_time_ms = 1;
  }
  return static_cast<int32_t>(available_processing_time_ms) -
      max_decode_time_ms > 0;
}

uint32_t VCMTiming::TargetVideoDelay() const {
  CriticalSectionScoped cs(crit_sect_);
  return TargetDelayInternal();
}

uint32_t VCMTiming::TargetDelayInternal() const {
  return std::max(min_playout_delay_ms_,
      jitter_delay_ms_ + MaxDecodeTimeMs() + render_delay_ms_);
}

}  // namespace webrtc
