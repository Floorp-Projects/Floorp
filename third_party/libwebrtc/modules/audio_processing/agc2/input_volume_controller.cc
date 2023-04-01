/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/input_volume_controller.h"

#include <algorithm>
#include <cmath>

#include "api/array_view.h"
#include "modules/audio_processing/agc2/gain_map_internal.h"
#include "modules/audio_processing/include/audio_frame_view.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_minmax.h"
#include "system_wrappers/include/field_trial.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {

namespace {

// Amount of error we tolerate in the microphone level (presumably due to OS
// quantization) before we assume the user has manually adjusted the microphone.
constexpr int kLevelQuantizationSlack = 25;

constexpr int kMaxMicLevel = 255;
static_assert(kGainMapSize > kMaxMicLevel, "gain map too small");

// Prevent very large microphone level changes.
constexpr int kMaxResidualGainChange = 15;

using Agc1ClippingPredictorConfig = AudioProcessing::Config::GainController1::
    AnalogGainController::ClippingPredictor;

// TODO(webrtc:7494): Hardcode clipping predictor parameters and remove this
// function after no longer needed in the ctor.
Agc1ClippingPredictorConfig CreateClippingPredictorConfig(bool enabled) {
  Agc1ClippingPredictorConfig config;
  config.enabled = enabled;

  return config;
}

// Returns the minimum input volume to recommend.
// If the "WebRTC-Audio-Agc2-MinInputVolume" field trial is specified, parses it
// and returns the value specified after "Enabled-" if valid - i.e., in the
// range 0-255. Otherwise returns the default value.
// Example:
// "WebRTC-Audio-Agc2-MinInputVolume/Enabled-80" => returns 80.
int GetMinInputVolume() {
  constexpr int kDefaultMinInputVolume = 12;
  constexpr char kFieldTrial[] = "WebRTC-Audio-Agc2-MinInputVolume";
  if (!webrtc::field_trial::IsEnabled(kFieldTrial)) {
    return kDefaultMinInputVolume;
  }
  std::string field_trial_str = webrtc::field_trial::FindFullName(kFieldTrial);
  int min_input_volume = -1;
  sscanf(field_trial_str.c_str(), "Enabled-%d", &min_input_volume);
  if (min_input_volume >= 0 && min_input_volume <= 255) {
    return min_input_volume;
  }
  RTC_LOG(LS_WARNING) << "Invalid volume for " << kFieldTrial << ", ignored.";
  return kDefaultMinInputVolume;
}

int LevelFromGainError(int gain_error, int level, int min_input_volume) {
  RTC_DCHECK_GE(level, 0);
  RTC_DCHECK_LE(level, kMaxMicLevel);
  if (gain_error == 0) {
    return level;
  }

  int new_level = level;
  if (gain_error > 0) {
    while (kGainMap[new_level] - kGainMap[level] < gain_error &&
           new_level < kMaxMicLevel) {
      ++new_level;
    }
  } else {
    while (kGainMap[new_level] - kGainMap[level] > gain_error &&
           new_level > min_input_volume) {
      --new_level;
    }
  }
  return new_level;
}

// Returns the proportion of samples in the buffer which are at full-scale
// (and presumably clipped).
float ComputeClippedRatio(const float* const* audio,
                          size_t num_channels,
                          size_t samples_per_channel) {
  RTC_DCHECK_GT(samples_per_channel, 0);
  int num_clipped = 0;
  for (size_t ch = 0; ch < num_channels; ++ch) {
    int num_clipped_in_ch = 0;
    for (size_t i = 0; i < samples_per_channel; ++i) {
      RTC_DCHECK(audio[ch]);
      if (audio[ch][i] >= 32767.0f || audio[ch][i] <= -32768.0f) {
        ++num_clipped_in_ch;
      }
    }
    num_clipped = std::max(num_clipped, num_clipped_in_ch);
  }
  return static_cast<float>(num_clipped) / (samples_per_channel);
}

void LogClippingMetrics(int clipping_rate) {
  RTC_LOG(LS_INFO) << "Input clipping rate: " << clipping_rate << "%";
  RTC_HISTOGRAM_COUNTS_LINEAR(/*name=*/"WebRTC.Audio.Agc.InputClippingRate",
                              /*sample=*/clipping_rate, /*min=*/0, /*max=*/100,
                              /*bucket_count=*/50);
}

// Computes the speech level error in dB. The value of `speech_level_dbfs` is
// required to be in the range [-90.0f, 30.0f]. Returns a positive value when
// the speech level is below the target range and a negative value when the
// speech level is above the target range.
int GetSpeechLevelErrorDb(float speech_level_dbfs,
                          int target_range_min_dbfs,
                          int target_range_max_dbfs) {
  constexpr float kMinSpeechLevelDbfs = -90.0f;
  constexpr float kMaxSpeechLevelDbfs = 30.0f;
  RTC_DCHECK_GE(speech_level_dbfs, kMinSpeechLevelDbfs);
  RTC_DCHECK_LE(speech_level_dbfs, kMaxSpeechLevelDbfs);

  // Ensure the speech level is in the range [-90.0f, 30.0f].
  speech_level_dbfs = rtc::SafeClamp<float>(
      speech_level_dbfs, kMinSpeechLevelDbfs, kMaxSpeechLevelDbfs);

  // Compute the speech level distance to the target range
  // [`target_range_min_dbfs`, `target_range_max_dbfs`].
  int rms_error_dbfs = 0;
  if (speech_level_dbfs > target_range_max_dbfs) {
    rms_error_dbfs = std::round(target_range_max_dbfs - speech_level_dbfs);
  } else if (speech_level_dbfs < target_range_min_dbfs) {
    rms_error_dbfs = std::round(target_range_min_dbfs - speech_level_dbfs);
  }

  return rms_error_dbfs;
}

}  // namespace

MonoInputVolumeController::MonoInputVolumeController(
    int clipped_level_min,
    int min_input_volume,
    int update_input_volume_wait_frames,
    float speech_probability_threshold,
    float speech_ratio_threshold)
    : min_input_volume_(min_input_volume),
      max_level_(kMaxMicLevel),
      clipped_level_min_(clipped_level_min),
      update_input_volume_wait_frames_(
          std::max(update_input_volume_wait_frames, 1)),
      speech_probability_threshold_(speech_probability_threshold),
      speech_ratio_threshold_(speech_ratio_threshold) {
  RTC_DCHECK_GE(clipped_level_min_, 0);
  RTC_DCHECK_LE(clipped_level_min_, 255);
  RTC_DCHECK_GE(min_input_volume_, 0);
  RTC_DCHECK_LE(min_input_volume_, 255);
  RTC_DCHECK_GE(update_input_volume_wait_frames_, 0);
  RTC_DCHECK_GE(speech_probability_threshold_, 0.0f);
  RTC_DCHECK_LE(speech_probability_threshold_, 1.0f);
  RTC_DCHECK_GE(speech_ratio_threshold_, 0.0f);
  RTC_DCHECK_LE(speech_ratio_threshold_, 1.0f);
}

MonoInputVolumeController::~MonoInputVolumeController() = default;

void MonoInputVolumeController::Initialize() {
  max_level_ = kMaxMicLevel;
  capture_output_used_ = true;
  check_volume_on_next_process_ = true;
  frames_since_update_input_volume_ = 0;
  speech_frames_since_update_input_volume_ = 0;
  is_first_frame_ = true;
}

// A speeh segment is considered active if at least
// `update_input_volume_wait_frames_` new frames have been processed since the
// previous update and the ratio of non-silence frames (i.e., frames with a
// `speech_probability` higher than `speech_probability_threshold_`) is at least
// `speech_ratio_threshold_`.
void MonoInputVolumeController::Process(absl::optional<int> rms_error_dbfs,
                                        float speech_probability) {
  if (check_volume_on_next_process_) {
    check_volume_on_next_process_ = false;
    // We have to wait until the first process call to check the volume,
    // because Chromium doesn't guarantee it to be valid any earlier.
    CheckVolumeAndReset();
  }

  // Count frames with a high speech probability as speech.
  if (speech_probability >= speech_probability_threshold_) {
    ++speech_frames_since_update_input_volume_;
  }

  // Reset the counters and maybe update the input volume.
  if (++frames_since_update_input_volume_ >= update_input_volume_wait_frames_) {
    const float speech_ratio =
        static_cast<float>(speech_frames_since_update_input_volume_) /
        static_cast<float>(update_input_volume_wait_frames_);

    // Always reset the counters regardless of whether the volume changes or
    // not.
    frames_since_update_input_volume_ = 0;
    speech_frames_since_update_input_volume_ = 0;

    // Update the input volume if allowed.
    if (!is_first_frame_ && speech_ratio >= speech_ratio_threshold_) {
      if (rms_error_dbfs.has_value()) {
        UpdateInputVolume(*rms_error_dbfs);
      }
    }
  }

  is_first_frame_ = false;
}

void MonoInputVolumeController::HandleClipping(int clipped_level_step) {
  RTC_DCHECK_GT(clipped_level_step, 0);
  // Always decrease the maximum level, even if the current level is below
  // threshold.
  SetMaxLevel(std::max(clipped_level_min_, max_level_ - clipped_level_step));
  if (log_to_histograms_) {
    RTC_HISTOGRAM_BOOLEAN("WebRTC.Audio.AgcClippingAdjustmentAllowed",
                          level_ - clipped_level_step >= clipped_level_min_);
  }
  if (level_ > clipped_level_min_) {
    // Don't try to adjust the level if we're already below the limit. As
    // a consequence, if the user has brought the level above the limit, we
    // will still not react until the postproc updates the level.
    SetLevel(std::max(clipped_level_min_, level_ - clipped_level_step));
    frames_since_update_input_volume_ = 0;
    speech_frames_since_update_input_volume_ = 0;
    is_first_frame_ = false;
  }
}

void MonoInputVolumeController::SetLevel(int new_level) {
  int voe_level = recommended_input_volume_;
  if (voe_level == 0) {
    RTC_DLOG(LS_INFO)
        << "[agc] VolumeCallbacks returned level=0, taking no action.";
    return;
  }
  if (voe_level < 0 || voe_level > kMaxMicLevel) {
    RTC_LOG(LS_ERROR) << "VolumeCallbacks returned an invalid level="
                      << voe_level;
    return;
  }

  // Detect manual input volume adjustments by checking if the current level
  // `voe_level` is outside of the `[level_ - kLevelQuantizationSlack, level_ +
  // kLevelQuantizationSlack]` range where `level_` is the last input volume
  // known by this gain controller.
  if (voe_level > level_ + kLevelQuantizationSlack ||
      voe_level < level_ - kLevelQuantizationSlack) {
    RTC_DLOG(LS_INFO) << "[agc] Mic volume was manually adjusted. Updating "
                         "stored level from "
                      << level_ << " to " << voe_level;
    level_ = voe_level;
    // Always allow the user to increase the volume.
    if (level_ > max_level_) {
      SetMaxLevel(level_);
    }
    // Take no action in this case, since we can't be sure when the volume
    // was manually adjusted.
    frames_since_update_input_volume_ = 0;
    speech_frames_since_update_input_volume_ = 0;
    is_first_frame_ = false;
    return;
  }

  new_level = std::min(new_level, max_level_);
  if (new_level == level_) {
    return;
  }

  recommended_input_volume_ = new_level;
  RTC_DLOG(LS_INFO) << "[agc] voe_level=" << voe_level << ", level_=" << level_
                    << ", new_level=" << new_level;
  level_ = new_level;
}

void MonoInputVolumeController::SetMaxLevel(int level) {
  RTC_DCHECK_GE(level, clipped_level_min_);
  max_level_ = level;
  RTC_DLOG(LS_INFO) << "[agc] max_level_=" << max_level_;
}

void MonoInputVolumeController::HandleCaptureOutputUsedChange(
    bool capture_output_used) {
  if (capture_output_used_ == capture_output_used) {
    return;
  }
  capture_output_used_ = capture_output_used;

  if (capture_output_used) {
    // When we start using the output, we should reset things to be safe.
    check_volume_on_next_process_ = true;
  }
}

int MonoInputVolumeController::CheckVolumeAndReset() {
  int level = recommended_input_volume_;
  // Reasons for taking action at startup:
  // 1) A person starting a call is expected to be heard.
  // 2) Independent of interpretation of `level` == 0 we should raise it so the
  // AGC can do its job properly.
  if (level == 0 && !startup_) {
    RTC_DLOG(LS_INFO)
        << "[agc] VolumeCallbacks returned level=0, taking no action.";
    return 0;
  }
  if (level < 0 || level > kMaxMicLevel) {
    RTC_LOG(LS_ERROR) << "[agc] VolumeCallbacks returned an invalid level="
                      << level;
    return -1;
  }
  RTC_DLOG(LS_INFO) << "[agc] Initial GetMicVolume()=" << level;

  if (level < min_input_volume_) {
    level = min_input_volume_;
    RTC_DLOG(LS_INFO) << "[agc] Initial volume too low, raising to " << level;
    recommended_input_volume_ = level;
  }

  level_ = level;
  startup_ = false;
  frames_since_update_input_volume_ = 0;
  speech_frames_since_update_input_volume_ = 0;
  is_first_frame_ = true;

  return 0;
}

void MonoInputVolumeController::UpdateInputVolume(int rms_error_dbfs) {
  const int residual_gain = rtc::SafeClamp(
      rms_error_dbfs, -kMaxResidualGainChange, kMaxResidualGainChange);

  RTC_DLOG(LS_INFO) << "[agc] rms_error_dbfs=" << rms_error_dbfs
                    << ", residual_gain=" << residual_gain;

  if (residual_gain == 0) {
    return;
  }

  SetLevel(LevelFromGainError(residual_gain, level_, min_input_volume_));
}

InputVolumeController::InputVolumeController(int num_capture_channels,
                                             const Config& config)
    : num_capture_channels_(num_capture_channels),
      min_input_volume_(GetMinInputVolume()),
      capture_output_used_(true),
      clipped_level_step_(config.clipped_level_step),
      clipped_ratio_threshold_(config.clipped_ratio_threshold),
      clipped_wait_frames_(config.clipped_wait_frames),
      clipping_predictor_(CreateClippingPredictor(
          num_capture_channels,
          CreateClippingPredictorConfig(config.enable_clipping_predictor))),
      use_clipping_predictor_step_(
          !!clipping_predictor_ &&
          CreateClippingPredictorConfig(config.enable_clipping_predictor)
              .use_predicted_step),
      frames_since_clipped_(config.clipped_wait_frames),
      clipping_rate_log_counter_(0),
      clipping_rate_log_(0.0f),
      target_range_max_dbfs_(config.target_range_max_dbfs),
      target_range_min_dbfs_(config.target_range_min_dbfs),
      channel_controllers_(num_capture_channels) {
  RTC_LOG(LS_INFO)
      << "[agc] Input volume controller enabled (min input volume: "
      << min_input_volume_ << ")";
  for (auto& controller : channel_controllers_) {
    controller = std::make_unique<MonoInputVolumeController>(
        config.clipped_level_min, min_input_volume_,
        config.update_input_volume_wait_frames,
        config.speech_probability_threshold, config.speech_ratio_threshold);
  }

  RTC_DCHECK(!channel_controllers_.empty());
  RTC_DCHECK_GT(clipped_level_step_, 0);
  RTC_DCHECK_LE(clipped_level_step_, 255);
  RTC_DCHECK_GT(clipped_ratio_threshold_, 0.0f);
  RTC_DCHECK_LT(clipped_ratio_threshold_, 1.0f);
  RTC_DCHECK_GT(clipped_wait_frames_, 0);
  channel_controllers_[0]->ActivateLogging();
}

InputVolumeController::~InputVolumeController() {}

void InputVolumeController::Initialize() {
  RTC_DLOG(LS_INFO) << "InputVolumeController::Initialize";
  for (auto& controller : channel_controllers_) {
    controller->Initialize();
  }
  capture_output_used_ = true;

  AggregateChannelLevels();
  clipping_rate_log_ = 0.0f;
  clipping_rate_log_counter_ = 0;
}

void InputVolumeController::AnalyzePreProcess(const AudioBuffer& audio_buffer) {
  const float* const* audio = audio_buffer.channels_const();
  size_t samples_per_channel = audio_buffer.num_frames();
  RTC_DCHECK(audio);

  AggregateChannelLevels();
  if (!capture_output_used_) {
    return;
  }

  if (!!clipping_predictor_) {
    AudioFrameView<const float> frame = AudioFrameView<const float>(
        audio, num_capture_channels_, static_cast<int>(samples_per_channel));
    clipping_predictor_->Analyze(frame);
  }

  // Check for clipped samples. We do this in the preprocessing phase in order
  // to catch clipped echo as well.
  //
  // If we find a sufficiently clipped frame, drop the current microphone level
  // and enforce a new maximum level, dropped the same amount from the current
  // maximum. This harsh treatment is an effort to avoid repeated clipped echo
  // events.
  float clipped_ratio =
      ComputeClippedRatio(audio, num_capture_channels_, samples_per_channel);
  clipping_rate_log_ = std::max(clipped_ratio, clipping_rate_log_);
  clipping_rate_log_counter_++;
  constexpr int kNumFramesIn30Seconds = 3000;
  if (clipping_rate_log_counter_ == kNumFramesIn30Seconds) {
    LogClippingMetrics(std::round(100.0f * clipping_rate_log_));
    clipping_rate_log_ = 0.0f;
    clipping_rate_log_counter_ = 0;
  }

  if (frames_since_clipped_ < clipped_wait_frames_) {
    ++frames_since_clipped_;
    return;
  }

  const bool clipping_detected = clipped_ratio > clipped_ratio_threshold_;
  bool clipping_predicted = false;
  int predicted_step = 0;
  if (!!clipping_predictor_) {
    for (int channel = 0; channel < num_capture_channels_; ++channel) {
      const auto step = clipping_predictor_->EstimateClippedLevelStep(
          channel, recommended_input_volume_, clipped_level_step_,
          channel_controllers_[channel]->clipped_level_min(), kMaxMicLevel);
      if (step.has_value()) {
        predicted_step = std::max(predicted_step, step.value());
        clipping_predicted = true;
      }
    }
  }

  if (clipping_detected) {
    RTC_DLOG(LS_INFO) << "[agc] Clipping detected. clipped_ratio="
                      << clipped_ratio;
  }

  int step = clipped_level_step_;
  if (clipping_predicted) {
    predicted_step = std::max(predicted_step, clipped_level_step_);
    RTC_DLOG(LS_INFO) << "[agc] Clipping predicted. step=" << predicted_step;
    if (use_clipping_predictor_step_) {
      step = predicted_step;
    }
  }

  if (clipping_detected ||
      (clipping_predicted && use_clipping_predictor_step_)) {
    for (auto& state_ch : channel_controllers_) {
      state_ch->HandleClipping(step);
    }
    frames_since_clipped_ = 0;
    if (!!clipping_predictor_) {
      clipping_predictor_->Reset();
    }
  }

  AggregateChannelLevels();
}

void InputVolumeController::Process(float speech_probability,
                                    absl::optional<float> speech_level_dbfs) {
  AggregateChannelLevels();

  if (!capture_output_used_) {
    return;
  }

  absl::optional<int> rms_error_dbfs;
  if (speech_level_dbfs.has_value()) {
    // Compute the error for all frames (both speech and non-speech frames).
    rms_error_dbfs = GetSpeechLevelErrorDb(
        *speech_level_dbfs, target_range_min_dbfs_, target_range_max_dbfs_);
  }

  for (auto& controller : channel_controllers_) {
    controller->Process(rms_error_dbfs, speech_probability);
  }

  AggregateChannelLevels();
}

void InputVolumeController::HandleCaptureOutputUsedChange(
    bool capture_output_used) {
  for (auto& controller : channel_controllers_) {
    controller->HandleCaptureOutputUsedChange(capture_output_used);
  }

  capture_output_used_ = capture_output_used;
}

void InputVolumeController::set_stream_analog_level(int level) {
  for (auto& controller : channel_controllers_) {
    controller->set_stream_analog_level(level);
  }

  AggregateChannelLevels();
}

void InputVolumeController::AggregateChannelLevels() {
  int new_recommended_input_volume =
      channel_controllers_[0]->recommended_analog_level();
  channel_controlling_gain_ = 0;
  for (size_t ch = 1; ch < channel_controllers_.size(); ++ch) {
    int level = channel_controllers_[ch]->recommended_analog_level();
    if (level < new_recommended_input_volume) {
      new_recommended_input_volume = level;
      channel_controlling_gain_ = static_cast<int>(ch);
    }
  }

  if (new_recommended_input_volume > 0) {
    new_recommended_input_volume =
        std::max(new_recommended_input_volume, min_input_volume_);
  }

  recommended_input_volume_ = new_recommended_input_volume;
}

}  // namespace webrtc
