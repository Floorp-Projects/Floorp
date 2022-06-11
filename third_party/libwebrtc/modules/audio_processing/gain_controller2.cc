/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/gain_controller2.h"

#include <memory>
#include <utility>

#include "common_audio/include/audio_util.h"
#include "modules/audio_processing/audio_buffer.h"
#include "modules/audio_processing/include/audio_frame_view.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/atomic_ops.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/strings/string_builder.h"

namespace webrtc {
namespace {

using Agc2Config = AudioProcessing::Config::GainController2;

constexpr int kUnspecifiedAnalogLevel = -1;
constexpr int kLogLimiterStatsPeriodMs = 30'000;
constexpr int kFrameLengthMs = 10;
constexpr int kLogLimiterStatsPeriodNumFrames =
    kLogLimiterStatsPeriodMs / kFrameLengthMs;

// Creates an adaptive digital gain controller if enabled.
std::unique_ptr<AdaptiveAgc> CreateAdaptiveDigitalController(
    const Agc2Config::AdaptiveDigital& config,
    int sample_rate_hz,
    int num_channels,
    ApmDataDumper* data_dumper) {
  if (config.enabled) {
    // TODO(bugs.webrtc.org/7494): Also init with sample rate and num channels.
    auto controller = std::make_unique<AdaptiveAgc>(data_dumper, config);
    // TODO(bugs.webrtc.org/7494): Remove once passed to the ctor.
    controller->Initialize(sample_rate_hz, num_channels);
    return controller;
  }
  return nullptr;
}

}  // namespace

int GainController2::instance_count_ = 0;

GainController2::GainController2(const Agc2Config& config,
                                 int sample_rate_hz,
                                 int num_channels)
    : data_dumper_(rtc::AtomicOps::Increment(&instance_count_)),
      fixed_gain_applier_(/*hard_clip_samples=*/false,
                          /*initial_gain_factor=*/0.0f),
      adaptive_digital_controller_(
          CreateAdaptiveDigitalController(config.adaptive_digital,
                                          sample_rate_hz,
                                          num_channels,
                                          &data_dumper_)),
      limiter_(sample_rate_hz, &data_dumper_, /*histogram_name_prefix=*/"Agc2"),
      calls_since_last_limiter_log_(0),
      analog_level_(kUnspecifiedAnalogLevel) {
  RTC_DCHECK(Validate(config));
  data_dumper_.InitiateNewSetOfRecordings();
  // TODO(bugs.webrtc.org/7494): Set gain when `fixed_gain_applier_` is init'd.
  fixed_gain_applier_.SetGainFactor(DbToRatio(config.fixed_digital.gain_db));
}

GainController2::~GainController2() = default;

void GainController2::Initialize(int sample_rate_hz, int num_channels) {
  RTC_DCHECK(sample_rate_hz == AudioProcessing::kSampleRate8kHz ||
             sample_rate_hz == AudioProcessing::kSampleRate16kHz ||
             sample_rate_hz == AudioProcessing::kSampleRate32kHz ||
             sample_rate_hz == AudioProcessing::kSampleRate48kHz);
  // TODO(bugs.webrtc.org/7494): Initialize `fixed_gain_applier_`.
  limiter_.SetSampleRate(sample_rate_hz);
  if (adaptive_digital_controller_) {
    adaptive_digital_controller_->Initialize(sample_rate_hz, num_channels);
  }
  data_dumper_.InitiateNewSetOfRecordings();
  calls_since_last_limiter_log_ = 0;
  analog_level_ = kUnspecifiedAnalogLevel;
}

void GainController2::SetFixedGainDb(float gain_db) {
  const float gain_factor = DbToRatio(gain_db);
  if (fixed_gain_applier_.GetGainFactor() != gain_factor) {
    // Reset the limiter to quickly react on abrupt level changes caused by
    // large changes of the fixed gain.
    limiter_.Reset();
  }
  fixed_gain_applier_.SetGainFactor(gain_factor);
}

void GainController2::Process(AudioBuffer* audio) {
  data_dumper_.DumpRaw("agc2_notified_analog_level", analog_level_);
  AudioFrameView<float> float_frame(audio->channels(), audio->num_channels(),
                                    audio->num_frames());
  fixed_gain_applier_.ApplyGain(float_frame);
  if (adaptive_digital_controller_) {
    adaptive_digital_controller_->Process(float_frame,
                                          limiter_.LastAudioLevel());
  }
  limiter_.Process(float_frame);

  // Periodically log limiter stats.
  if (++calls_since_last_limiter_log_ == kLogLimiterStatsPeriodNumFrames) {
    calls_since_last_limiter_log_ = 0;
    InterpolatedGainCurve::Stats stats = limiter_.GetGainCurveStats();
    RTC_LOG(LS_INFO) << "AGC2 limiter stats"
                     << " | identity: " << stats.look_ups_identity_region
                     << " | knee: " << stats.look_ups_knee_region
                     << " | limiter: " << stats.look_ups_limiter_region
                     << " | saturation: " << stats.look_ups_saturation_region;
  }
}

void GainController2::NotifyAnalogLevel(int level) {
  if (analog_level_ != level && adaptive_digital_controller_) {
    adaptive_digital_controller_->HandleInputGainChange();
  }
  analog_level_ = level;
}

bool GainController2::Validate(
    const AudioProcessing::Config::GainController2& config) {
  const auto& fixed = config.fixed_digital;
  const auto& adaptive = config.adaptive_digital;
  return fixed.gain_db >= 0.0f && fixed.gain_db < 50.f &&
         adaptive.headroom_db >= 0.0f && adaptive.max_gain_db > 0.0f &&
         adaptive.initial_gain_db >= 0.0f &&
         adaptive.max_gain_change_db_per_second > 0.0f &&
         adaptive.max_output_noise_level_dbfs <= 0.0f;
}

}  // namespace webrtc
