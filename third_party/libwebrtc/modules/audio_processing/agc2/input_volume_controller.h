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

#include <atomic>
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

// Input volume controller that controls the input volume. The input volume
// controller recommends what volume to use, handles volume changes and
// clipping. In particular, it handles changes triggered by the user (e.g.,
// volume set to zero by a HW mute button). The digital controller chooses and
// applies the digital compression gain. This class is not thread-safe.
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
    // If true, an adaptive digital gain is applied.
    bool digital_adaptive_follows = true;
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
    // Minimum and maximum digital gain used before input volume is
    // adjusted.
    int max_digital_gain_db = 30;
    int min_digital_gain_db = 0;
  };

  // Ctor. `num_capture_channels` specifies the number of channels for the audio
  // passed to `AnalyzePreProcess()` and `Process()`. Clamps
  // `config.startup_min_level` in the [12, 255] range.
  InputVolumeController(int num_capture_channels, const Config& config);

  ~InputVolumeController();
  InputVolumeController(const InputVolumeController&) = delete;
  InputVolumeController& operator=(const InputVolumeController&) = delete;

  // TODO(webrtc:7494): Integrate initialization into ctor and remove this
  // method.
  void Initialize();

  // Sets the applied input volume.
  void set_stream_analog_level(int level);

  // TODO(bugs.webrtc.org/7494): Add argument for the applied input volume and
  // remove `set_stream_analog_level()`.
  // Analyzes `audio` before `Process()` is called so that the analysis can be
  // performed before external digital processing operations take place (e.g.,
  // echo cancellation). The analysis consists of input clipping detection and
  // prediction (if enabled). Must be called after `set_stream_analog_level()`.
  void AnalyzePreProcess(const AudioBuffer& audio_buffer);

  // Chooses a digital compression gain and the new input volume to recommend.
  // Must be called after `AnalyzePreProcess()`. `speech_probability`
  // (range [0.0f, 1.0f]) and `speech_level_dbfs` (range [-90.f, 30.0f]) are
  // used to compute the RMS error.
  void Process(absl::optional<float> speech_probability,
               absl::optional<float> speech_level_dbfs);

  // TODO(bugs.webrtc.org/7494): Return recommended input volume and remove
  // `recommended_analog_level()`.
  // Returns the recommended input volume. If the input volume contoller is
  // disabled, returns the input volume set via the latest
  // `set_stream_analog_level()` call. Must be called after
  // `AnalyzePreProcess()` and `Process()`.
  int recommended_analog_level() const { return recommended_input_volume_; }

  // Call when the capture stream output has been flagged to be used/not-used.
  // If unused, the manager  disregards all incoming audio.
  void HandleCaptureOutputUsedChange(bool capture_output_used);

  float voice_probability() const;

  int num_channels() const { return num_capture_channels_; }

  // If available, returns the latest digital compression gain that has been
  // chosen.
  absl::optional<int> GetDigitalComressionGain();

  // Returns true if clipping prediction is enabled.
  bool clipping_predictor_enabled() const { return !!clipping_predictor_; }

  // Returns true if clipping prediction is used to adjust the input volume.
  bool use_clipping_predictor_step() const {
    return use_clipping_predictor_step_;
  }

 private:
  friend class InputVolumeControllerTestHelper;

  FRIEND_TEST_ALL_PREFIXES(InputVolumeControllerTest,
                           DisableDigitalDisablesDigital);
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
  FRIEND_TEST_ALL_PREFIXES(InputVolumeControllerTest,
                           AgcMinMicLevelExperimentEnabledAboveStartupLevel);
  FRIEND_TEST_ALL_PREFIXES(InputVolumeControllerParametrizedTest,
                           ClippingParametersVerified);
  FRIEND_TEST_ALL_PREFIXES(InputVolumeControllerParametrizedTest,
                           DisableClippingPredictorDoesNotLowerVolume);
  FRIEND_TEST_ALL_PREFIXES(InputVolumeControllerParametrizedTest,
                           UsedClippingPredictionsProduceLowerAnalogLevels);
  FRIEND_TEST_ALL_PREFIXES(InputVolumeControllerParametrizedTest,
                           UnusedClippingPredictionsProduceEqualAnalogLevels);
  FRIEND_TEST_ALL_PREFIXES(InputVolumeControllerParametrizedTest,
                           EmptyRmsErrorOverrideHasNoEffect);

  void AggregateChannelLevels();

  const bool analog_controller_enabled_;

  const absl::optional<int> min_mic_level_override_;
  static std::atomic<int> instance_counter_;
  const bool use_min_channel_level_;
  const int num_capture_channels_;

  // TODO(webrtc:7494): Replace with `digital_adaptive_follows_`.
  const bool disable_digital_adaptive_;

  int frames_since_clipped_;

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
  int channel_controlling_gain_ = 0;

  const int clipped_level_step_;
  const float clipped_ratio_threshold_;
  const int clipped_wait_frames_;

  std::vector<std::unique_ptr<MonoInputVolumeController>> channel_controllers_;

  const std::unique_ptr<ClippingPredictor> clipping_predictor_;
  const bool use_clipping_predictor_step_;
  float clipping_rate_log_;
  int clipping_rate_log_counter_;
};

// TODO(bugs.webrtc.org/7494): Use applied/recommended input volume naming
// convention.
class MonoInputVolumeController {
 public:
  MonoInputVolumeController(int startup_min_level,
                            int clipped_level_min,
                            bool disable_digital_adaptive,
                            int min_mic_level,
                            int max_digital_gain_db,
                            int min_digital_gain_db);
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

  // Updates the recommended input volume based on the estimated speech level
  // RMS error. Must be called after `HandleClipping()`.
  void Process(absl::optional<int> rms_error_override);

  // Returns the recommended input volume. Must be called after `Process()`.
  int recommended_analog_level() const { return recommended_input_volume_; }

  void ActivateLogging() { log_to_histograms_ = true; }

  // Only used for testing.
  int min_mic_level() const { return min_mic_level_; }
  int startup_min_level() const { return startup_min_level_; }

 private:
  // Sets a new input volume, after first checking that it hasn't been updated
  // by the user, in which case no action is taken.
  void SetLevel(int new_level);

  // Set the maximum input volume the input volume controller is allowed to
  // apply. The volume must be at least `kClippedLevelMin`.
  void SetMaxLevel(int level);

  int CheckVolumeAndReset();
  void UpdateGain(int rms_error_db);

  const int min_mic_level_;

  // TODO(webrtc:7494): Replace with `digital_adaptive_follows_`.
  const bool disable_digital_adaptive_;
  const int max_digital_gain_db_;
  const int min_digital_gain_db_;

  int level_ = 0;
  int max_level_;

  bool capture_output_used_ = true;
  bool check_volume_on_next_process_ = true;
  bool startup_ = true;
  int startup_min_level_;

  // TODO(bugs.webrtc.org/7494): Create a separate member for the applied
  // input volume.
  // Recommended input volume. After `set_stream_analog_level()` is
  // called, it holds the observed applied input volume. Possibly updated by
  // `HandleClipping()` and `Process()`; after these calls, holds the
  // recommended input volume.
  int recommended_input_volume_ = 0;

  bool log_to_histograms_ = false;

  const int clipped_level_min_;

  // Frames since the last `UpdateGain()` call.
  int frames_since_update_gain_ = 0;
  bool is_first_frame_ = true;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AGC2_INPUT_VOLUME_CONTROLLER_H_
