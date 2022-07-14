/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/timing.h"

#include <algorithm>

#include "api/units/time_delta.h"
#include "rtc_base/experiments/field_trial_parser.h"
#include "rtc_base/time/timestamp_extrapolator.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {
namespace {

// Default pacing that is used for the low-latency renderer path.
constexpr TimeDelta kZeroPlayoutDelayDefaultMinPacing = TimeDelta::Millis(8);
constexpr TimeDelta kLowLatencyRendererMaxPlayoutDelay = TimeDelta::Millis(500);

}  // namespace

VCMTiming::VCMTiming(Clock* clock, const FieldTrialsView& field_trials)
    : clock_(clock),
      ts_extrapolator_(
          std::make_unique<TimestampExtrapolator>(clock_->CurrentTime())),
      codec_timer_(std::make_unique<VCMCodecTimer>()),
      render_delay_(kDefaultRenderDelay),
      min_playout_delay_(TimeDelta::Zero()),
      max_playout_delay_(TimeDelta::Seconds(10)),
      jitter_delay_(TimeDelta::Zero()),
      current_delay_(TimeDelta::Zero()),
      prev_frame_timestamp_(0),
      num_decoded_frames_(0),
      zero_playout_delay_min_pacing_("min_pacing",
                                     kZeroPlayoutDelayDefaultMinPacing),
      last_decode_scheduled_(Timestamp::Zero()) {
  ParseFieldTrial({&zero_playout_delay_min_pacing_},
                  field_trials.Lookup("WebRTC-ZeroPlayoutDelay"));
}

void VCMTiming::Reset() {
  MutexLock lock(&mutex_);
  ts_extrapolator_->Reset(clock_->CurrentTime());
  codec_timer_ = std::make_unique<VCMCodecTimer>();
  render_delay_ = kDefaultRenderDelay;
  min_playout_delay_ = TimeDelta::Zero();
  jitter_delay_ = TimeDelta::Zero();
  current_delay_ = TimeDelta::Zero();
  prev_frame_timestamp_ = 0;
}

void VCMTiming::set_render_delay(TimeDelta render_delay) {
  MutexLock lock(&mutex_);
  render_delay_ = render_delay;
}

void VCMTiming::set_min_playout_delay(TimeDelta min_playout_delay) {
  MutexLock lock(&mutex_);
  min_playout_delay_ = min_playout_delay;
}

void VCMTiming::set_max_playout_delay(TimeDelta max_playout_delay) {
  MutexLock lock(&mutex_);
  max_playout_delay_ = max_playout_delay;
}

void VCMTiming::SetJitterDelay(TimeDelta jitter_delay) {
  MutexLock lock(&mutex_);
  if (jitter_delay != jitter_delay_) {
    jitter_delay_ = jitter_delay;
    // When in initial state, set current delay to minimum delay.
    if (current_delay_.IsZero()) {
      current_delay_ = jitter_delay_;
    }
  }
}

void VCMTiming::UpdateCurrentDelay(uint32_t frame_timestamp) {
  MutexLock lock(&mutex_);
  TimeDelta target_delay = TargetDelayInternal();

  if (current_delay_.IsZero()) {
    // Not initialized, set current delay to target.
    current_delay_ = target_delay;
  } else if (target_delay != current_delay_) {
    TimeDelta delay_diff = target_delay - current_delay_;
    // Never change the delay with more than 100 ms every second. If we're
    // changing the delay in too large steps we will get noticeable freezes. By
    // limiting the change we can increase the delay in smaller steps, which
    // will be experienced as the video is played in slow motion. When lowering
    // the delay the video will be played at a faster pace.
    TimeDelta max_change = TimeDelta::Zero();
    if (frame_timestamp < 0x0000ffff && prev_frame_timestamp_ > 0xffff0000) {
      // wrap
      max_change =
          TimeDelta::Millis(kDelayMaxChangeMsPerS *
                            (frame_timestamp + (static_cast<int64_t>(1) << 32) -
                             prev_frame_timestamp_) /
                            90000);
    } else {
      max_change =
          TimeDelta::Millis(kDelayMaxChangeMsPerS *
                            (frame_timestamp - prev_frame_timestamp_) / 90000);
    }

    if (max_change <= TimeDelta::Zero()) {
      // Any changes less than 1 ms are truncated and will be postponed.
      // Negative change will be due to reordering and should be ignored.
      return;
    }
    delay_diff = std::max(delay_diff, -max_change);
    delay_diff = std::min(delay_diff, max_change);

    current_delay_ = current_delay_ + delay_diff;
  }
  prev_frame_timestamp_ = frame_timestamp;
}

void VCMTiming::UpdateCurrentDelay(Timestamp render_time,
                                   Timestamp actual_decode_time) {
  MutexLock lock(&mutex_);
  TimeDelta target_delay = TargetDelayInternal();
  TimeDelta delayed =
      (actual_decode_time - render_time) + RequiredDecodeTime() + render_delay_;
  if (delayed < TimeDelta::Zero()) {
    return;
  }
  if (current_delay_ + delayed <= target_delay) {
    current_delay_ += delayed;
  } else {
    current_delay_ = target_delay;
  }
}

void VCMTiming::StopDecodeTimer(TimeDelta decode_time, Timestamp now) {
  MutexLock lock(&mutex_);
  codec_timer_->AddTiming(decode_time.ms(), now.ms());
  RTC_DCHECK_GE(decode_time, TimeDelta::Zero());
  ++num_decoded_frames_;
}

void VCMTiming::IncomingTimestamp(uint32_t rtp_timestamp, Timestamp now) {
  MutexLock lock(&mutex_);
  ts_extrapolator_->Update(now, rtp_timestamp);
}

Timestamp VCMTiming::RenderTime(uint32_t frame_timestamp, Timestamp now) const {
  MutexLock lock(&mutex_);
  return RenderTimeInternal(frame_timestamp, now);
}

void VCMTiming::SetLastDecodeScheduledTimestamp(
    Timestamp last_decode_scheduled) {
  MutexLock lock(&mutex_);
  last_decode_scheduled_ = last_decode_scheduled;
}

Timestamp VCMTiming::RenderTimeInternal(uint32_t frame_timestamp,
                                        Timestamp now) const {
  if (min_playout_delay_.IsZero() &&
      (max_playout_delay_.IsZero() ||
       max_playout_delay_ <= kLowLatencyRendererMaxPlayoutDelay)) {
    // Render as soon as possible or with low-latency renderer algorithm.
    return Timestamp::Zero();
  }
  // Note that TimestampExtrapolator::ExtrapolateLocalTime is not a const
  // method; it mutates the object's wraparound state.
  Timestamp estimated_complete_time =
      ts_extrapolator_->ExtrapolateLocalTime(frame_timestamp).value_or(now);

  // Make sure the actual delay stays in the range of `min_playout_delay_`
  // and `max_playout_delay_`.
  TimeDelta actual_delay =
      current_delay_.Clamped(min_playout_delay_, max_playout_delay_);
  return estimated_complete_time + actual_delay;
}

TimeDelta VCMTiming::RequiredDecodeTime() const {
  const int decode_time_ms = codec_timer_->RequiredDecodeTimeMs();
  RTC_DCHECK_GE(decode_time_ms, 0);
  return TimeDelta::Millis(decode_time_ms);
}

TimeDelta VCMTiming::MaxWaitingTime(Timestamp render_time,
                                    Timestamp now,
                                    bool too_many_frames_queued) const {
  MutexLock lock(&mutex_);

  if (render_time.IsZero() && zero_playout_delay_min_pacing_->us() > 0 &&
      min_playout_delay_.IsZero() && max_playout_delay_ > TimeDelta::Zero()) {
    // `render_time` == 0 indicates that the frame should be decoded and
    // rendered as soon as possible. However, the decoder can be choked if too
    // many frames are sent at once. Therefore, limit the interframe delay to
    // |zero_playout_delay_min_pacing_| unless too many frames are queued in
    // which case the frames are sent to the decoder at once.
    if (too_many_frames_queued) {
      return TimeDelta::Zero();
    }
    Timestamp earliest_next_decode_start_time =
        last_decode_scheduled_ +
        static_cast<const webrtc::TimeDelta>(zero_playout_delay_min_pacing_);
    TimeDelta max_wait_time = now >= earliest_next_decode_start_time
                                  ? TimeDelta::Zero()
                                  : earliest_next_decode_start_time - now;
    return max_wait_time;
  }
  return render_time - now - RequiredDecodeTime() - render_delay_;
}

TimeDelta VCMTiming::TargetVideoDelay() const {
  MutexLock lock(&mutex_);
  return TargetDelayInternal();
}

TimeDelta VCMTiming::TargetDelayInternal() const {
  return std::max(min_playout_delay_,
                  jitter_delay_ + RequiredDecodeTime() + render_delay_);
}

VCMTiming::VideoDelayTimings VCMTiming::GetTimings() const {
  MutexLock lock(&mutex_);
  return VideoDelayTimings{.max_decode_duration = RequiredDecodeTime(),
                           .current_delay = current_delay_,
                           .target_delay = TargetDelayInternal(),
                           .jitter_buffer_delay = jitter_delay_,
                           .min_playout_delay = min_playout_delay_,
                           .max_playout_delay = max_playout_delay_,
                           .render_delay = render_delay_,
                           .num_decoded_frames = num_decoded_frames_};
}

void VCMTiming::SetTimingFrameInfo(const TimingFrameInfo& info) {
  MutexLock lock(&mutex_);
  timing_frame_info_.emplace(info);
}

absl::optional<TimingFrameInfo> VCMTiming::GetTimingFrameInfo() {
  MutexLock lock(&mutex_);
  return timing_frame_info_;
}

void VCMTiming::SetMaxCompositionDelayInFrames(
    absl::optional<int> max_composition_delay_in_frames) {
  MutexLock lock(&mutex_);
  max_composition_delay_in_frames_ = max_composition_delay_in_frames;
}

absl::optional<int> VCMTiming::MaxCompositionDelayInFrames() const {
  MutexLock lock(&mutex_);
  return max_composition_delay_in_frames_;
}

}  // namespace webrtc
