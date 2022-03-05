/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/saturation_protector.h"

#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/numerics/safe_compare.h"
#include "rtc_base/numerics/safe_minmax.h"

namespace webrtc {
namespace {

constexpr float kMinLevelDbfs = -90.f;

// Min/max margins are based on speech crest-factor.
constexpr float kMinMarginDb = 12.f;
constexpr float kMaxMarginDb = 25.f;

}  // namespace

void SaturationProtector::RingBuffer::Reset() {
  next_ = 0;
  size_ = 0;
}

void SaturationProtector::RingBuffer::PushBack(float v) {
  RTC_DCHECK_GE(next_, 0);
  RTC_DCHECK_GE(size_, 0);
  RTC_DCHECK_LT(next_, buffer_.size());
  RTC_DCHECK_LE(size_, buffer_.size());
  buffer_[next_++] = v;
  if (rtc::SafeEq(next_, buffer_.size())) {
    next_ = 0;
  }
  if (rtc::SafeLt(size_, buffer_.size())) {
    size_++;
  }
}

absl::optional<float> SaturationProtector::RingBuffer::Front() const {
  if (size_ == 0) {
    return absl::nullopt;
  }
  RTC_DCHECK_LT(next_, buffer_.size());
  return buffer_[rtc::SafeEq(size_, buffer_.size()) ? next_ : 0];
}

SaturationProtector::PeakEnveloper::PeakEnveloper()
    : speech_time_in_estimate_ms_(0),
      current_superframe_peak_dbfs_(kMinLevelDbfs) {}

void SaturationProtector::PeakEnveloper::Reset() {
  speech_time_in_estimate_ms_ = 0;
  current_superframe_peak_dbfs_ = kMinLevelDbfs;
  peak_delay_buffer_.Reset();
}

void SaturationProtector::PeakEnveloper::Process(float frame_peak_dbfs) {
  // Get the max peak over `kPeakEnveloperSuperFrameLengthMs` ms.
  current_superframe_peak_dbfs_ =
      std::max(current_superframe_peak_dbfs_, frame_peak_dbfs);
  speech_time_in_estimate_ms_ += kFrameDurationMs;
  if (speech_time_in_estimate_ms_ > kPeakEnveloperSuperFrameLengthMs) {
    peak_delay_buffer_.PushBack(current_superframe_peak_dbfs_);
    // Reset.
    speech_time_in_estimate_ms_ = 0;
    current_superframe_peak_dbfs_ = kMinLevelDbfs;
  }
}

float SaturationProtector::PeakEnveloper::Query() const {
  return peak_delay_buffer_.Front().value_or(current_superframe_peak_dbfs_);
}

SaturationProtector::SaturationProtector(ApmDataDumper* apm_data_dumper)
    : SaturationProtector(apm_data_dumper, GetExtraSaturationMarginOffsetDb()) {
}

SaturationProtector::SaturationProtector(ApmDataDumper* apm_data_dumper,
                                         float extra_saturation_margin_db)
    : apm_data_dumper_(apm_data_dumper),
      extra_saturation_margin_db_(extra_saturation_margin_db),
      last_margin_(GetInitialSaturationMarginDb()) {}

void SaturationProtector::UpdateMargin(
    const VadWithLevel::LevelAndProbability& vad_data,
    float last_speech_level_estimate) {
  peak_enveloper_.Process(vad_data.speech_peak_dbfs);
  const float delayed_peak_dbfs = peak_enveloper_.Query();
  const float difference_db = delayed_peak_dbfs - last_speech_level_estimate;

  if (last_margin_ < difference_db) {
    last_margin_ = last_margin_ * kSaturationProtectorAttackConstant +
                   difference_db * (1.f - kSaturationProtectorAttackConstant);
  } else {
    last_margin_ = last_margin_ * kSaturationProtectorDecayConstant +
                   difference_db * (1.f - kSaturationProtectorDecayConstant);
  }

  last_margin_ =
      rtc::SafeClamp<float>(last_margin_, kMinMarginDb, kMaxMarginDb);
}

float SaturationProtector::LastMargin() const {
  return last_margin_ + extra_saturation_margin_db_;
}

void SaturationProtector::Reset() {
  peak_enveloper_.Reset();
}

void SaturationProtector::DebugDumpEstimate() const {
  if (apm_data_dumper_) {
    apm_data_dumper_->DumpRaw(
        "agc2_adaptive_saturation_protector_delayed_peak_dbfs",
        peak_enveloper_.Query());
    apm_data_dumper_->DumpRaw("agc2_adaptive_saturation_margin_db",
                              last_margin_);
  }
}

}  // namespace webrtc
