/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_INPUT_VOLUME_CONTROLLER_H_
#define MODULES_AUDIO_PROCESSING_AGC2_INPUT_VOLUME_CONTROLLER_H_

#include <memory>
#include <vector>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "modules/audio_processing/agc2/clipping_predictor.h"
#include "modules/audio_processing/audio_buffer.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "rtc_base/gtest_prod_util.h"

namespace webrtc {

class MonoInputVolumeController;

// The input volume controller recommends what volume to use, handles volume
// changes and clipping detection and prediction. In particular, it handles
// changes triggered by the user (e.g., volume set to zero by a HW mute button).
// This class is not thread-safe.
// TODO(bugs.webrtc.org/7494): Use applied/recommended input volume naming
// convention.
class InputVolumeController final {
 public:
  // Config for the constructor.
  struct Config {
    bool enabled = false;
    // TODO(bugs.webrtc.org/1275566): Describe `startup_min_volume`.
    int startup_min_volume = 0;
    // Lowest analog microphone level that will be applied in response to
    // clipping.
    int clipped_level_min = 70;
    // Amount the microphone level is lowered with every clipping event.
    // Limited to (0, 255].
    int clipped_level_step = 15;
    // Proportion of clipped samples required to declare a clipping event.
    // Limited to (0.f, 1.f).
    float clipped_ratio_threshold = 0.1f;
    // Time in frames to wait after a clipping event before checking again.
    // Limited to values higher than 0.
    int clipped_wait_frames = 300;
    // Enables clipping prediction functionality.
    bool enable_clipping_predictor = false;
    // Speech level target range (dBFS). If the speech level is in the range
    // [`target_range_min_dbfs`, `target_range_max_dbfs`], no input volume
    // adjustments are done based on the speech level. For speech levels below
    // and above the range, the targets `target_range_min_dbfs` and
    // `target_range_max_dbfs` are used, respectively. The example values
    // `target_range_max_dbfs` -18 and `target_range_min_dbfs` -48 refer to a
    // configuration where the zero-digital-gain target is -18 dBFS and the
    // digital gain control is expected to compensate for speech level errors
    // up to -30 dB.
    int target_range_max_dbfs = -18;
    int target_range_min_dbfs = -48;
  };

  // Ctor. `num_capture_channels` specifies the number of channels for the audio
  // passed to `AnalyzePreProcess()` and `Process()`. Clamps
  // `config.startup_min_level` in the [12, 255] range.
  InputVolumeController(int num_capture_channels, const Config& config);

  ~InputVolumeController();
  InputVolumeController(const InputVolumeController&) = delete;
  InputVolumeController& operator=(const InputVolumeController&) = delete;

  // TODO(webrtc:7494): Integrate initialization into ctor and remove.
  void Initialize();

  // Sets the applied input volume.
  void set_stream_analog_level(int level);

  // TODO(bugs.webrtc.org/7494): Add argument for the applied input volume and
  // remove `set_stream_analog_level()`.
  // Analyzes `audio` before `Process()` is called so that the analysis can be
  // performed before digital processing operations take place (e.g., echo
  // cancellation). The analysis consists of input clipping detection and
  // prediction (if enabled). Must be called after `set_stream_analog_level()`.
  void AnalyzePreProcess(const AudioBuffer& audio_buffer);

  // Adjusts the recommended input volume upwards/downwards based on
  // `speech_level_dbfs`. Must be called after `AnalyzePreProcess()`. The value
  // of `speech_probability` is expected to be in the range [0.0f, 1.0f] and
  // `speech_level_dbfs` in the the range [-90.f, 30.0f].
  void Process(absl::optional<float> speech_probability,
               absl::optional<float> speech_level_dbfs);

  // TODO(bugs.webrtc.org/7494): Return recommended input volume and remove
  // `recommended_analog_level()`.
  // Returns the recommended input volume. If the input volume contoller is
  // disabled, returns the input volume set via the latest
  // `set_stream_analog_level()` call. Must be called after
  // `AnalyzePreProcess()` and `Process()`.
  int recommended_analog_level() const { return recommended_input_volume_; }

  // Stores whether the capture output will be used or not. Call when the
  // capture stream output has been flagged to be used/not-used. If unused, the
  // controller disregards all incoming audio.
  void HandleCaptureOutputUsedChange(bool capture_output_used);

  // Returns true if clipping prediction is enabled.
  // TODO(bugs.webrtc.org/7494): Deprecate this method.
  bool clipping_predictor_enabled() const { return !!clipping_predictor_; }

  // Returns true if clipping prediction is used to adjust the input volume.
  // TODO(bugs.webrtc.org/7494): Deprecate this method.
  bool use_clipping_predictor_step() const {
    return use_clipping_predictor_step_;
  }

 private:
  friend class InputVolumeControllerTestHelper;

  FRIEND_TEST_ALL_PREFIXES(InputVolumeControllerTest,
                           AgcMinMicLevelExperimentDefault);
  FRIEND_TEST_ALL_PREFIXES(InputVolumeControllerTest,
                           AgcMinMicLevelExperimentDisabled);
  FRIEND_TEST_ALL_PREFIXES(InputVolumeControllerTest,
                           AgcMinMicLevelExperimentOutOfRangeAbove);
  FRIEND_TEST_ALL_PREFIXES(InputVolumeControllerTest,
                           AgcMinMicLevelExperimentOutOfRangeBelow);
  FRIEND_TEST_ALL_PREFIXES(InputVolumeControllerTest,
                           AgcMinMicLevelExperimentEnabled50);
  FRIEND_TEST_ALL_PREFIXES(InputVolumeControllerParametrizedTest,
                           ClippingParametersVerified);

  void AggregateChannelLevels();

  const bool analog_controller_enabled_;

  const int num_capture_channels_;

  // If not empty, the value is used to override the minimum input volume.
  const absl::optional<int> min_mic_level_override_;

  const bool use_min_channel_level_;

  // TODO(bugs.webrtc.org/7494): Create a separate member for the applied input
  // volume.
  // TODO(bugs.webrtc.org/7494): Once
  // `AudioProcessingImpl::recommended_stream_analog_level()` becomes a trivial
  // getter, leave uninitialized.
  // Recommended input volume. After `set_stream_analog_level()` is called it
  // holds the observed input volume. Possibly updated by `AnalyzePreProcess()`
  // and `Process()`; after these calls, holds the recommended input volume.
  int recommended_input_volume_ = 0;

  bool capture_output_used_;

  // Clipping detection and prediction.
  const int clipped_level_step_;
  const float clipped_ratio_threshold_;
  const int clipped_wait_frames_;
  const std::unique_ptr<ClippingPredictor> clipping_predictor_;
  const bool use_clipping_predictor_step_;
  int frames_since_clipped_;
  int clipping_rate_log_counter_;
  float clipping_rate_log_;

  // Target range minimum and maximum. If the seech level is in the range
  // [`target_range_min_dbfs`, `target_range_max_dbfs`], no volume adjustments
  // take place. Instead, the digital gain controller is assumed to adapt to
  // compensate for the speech level RMS error.
  const int target_range_max_dbfs_;
  const int target_range_min_dbfs_;

  // Channel controllers updating the gain upwards/downwards.
  std::vector<std::unique_ptr<MonoInputVolumeController>> channel_controllers_;
  int channel_controlling_gain_ = 0;
};

// TODO(bugs.webrtc.org/7494): Use applied/recommended input volume naming
// convention.
class MonoInputVolumeController {
 public:
  MonoInputVolumeController(int clipped_level_min, int min_mic_level);
  ~MonoInputVolumeController();
  MonoInputVolumeController(const MonoInputVolumeController&) = delete;
  MonoInputVolumeController& operator=(const MonoInputVolumeController&) =
      delete;

  void Initialize();
  void HandleCaptureOutputUsedChange(bool capture_output_used);

  // Sets the current input volume.
  void set_stream_analog_level(int level) { recommended_input_volume_ = level; }

  // Lowers the recommended input volume in response to clipping based on the
  // suggested reduction `clipped_level_step`. Must be called after
  // `set_stream_analog_level()`.
  void HandleClipping(int clipped_level_step);

  // Adjusts the recommended input volume upwards/downwards depending on whether
  // `rms_error_dbfs` is positive or negative. Must be called after
  // `HandleClipping()`.
  void Process(absl::optional<int> rms_error_dbfs);

  // Returns the recommended input volume. Must be called after `Process()`.
  int recommended_analog_level() const { return recommended_input_volume_; }

  void ActivateLogging() { log_to_histograms_ = true; }

  // Only used for testing.
  int min_mic_level() const { return min_mic_level_; }

 private:
  // Sets a new input volume, after first checking that it hasn't been updated
  // by the user, in which case no action is taken.
  void SetLevel(int new_level);

  // Sets the maximum input volume that the input volume controller is allowed
  // to apply. The volume must be at least `kClippedLevelMin`.
  void SetMaxLevel(int level);

  int CheckVolumeAndReset();

  // Updates the recommended input volume. If the volume slider needs to be
  // moved, we check first if the user has adjusted it, in which case we take no
  // action and cache the updated level.
  void UpdateInputVolume(int rms_error_dbfs);

  const int min_mic_level_;

  int level_ = 0;
  int max_level_;

  bool capture_output_used_ = true;
  bool check_volume_on_next_process_ = true;
  bool startup_ = true;

  // TODO(bugs.webrtc.org/7494): Create a separate member for the applied
  // input volume.
  // Recommended input volume. After `set_stream_analog_level()` is
  // called, it holds the observed applied input volume. Possibly updated by
  // `HandleClipping()` and `Process()`; after these calls, holds the
  // recommended input volume.
  int recommended_input_volume_ = 0;

  bool log_to_histograms_ = false;

  const int clipped_level_min_;

  // Frames since the last `UpdateInputVolume()` call.
  int frames_since_update_gain_ = 0;
  bool is_first_frame_ = true;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AGC2_INPUT_VOLUME_CONTROLLER_H_
