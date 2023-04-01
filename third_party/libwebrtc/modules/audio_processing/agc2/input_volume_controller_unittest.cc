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
#include <fstream>
#include <limits>
#include <string>
#include <vector>

#include "rtc_base/numerics/safe_minmax.h"
#include "rtc_base/strings/string_builder.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgPointee;

namespace webrtc {
namespace {

constexpr int kSampleRateHz = 32000;
constexpr int kNumChannels = 1;
constexpr int kInitialInputVolume = 128;
constexpr int kClippedMin = 165;  // Arbitrary, but different from the default.
constexpr float kAboveClippedThreshold = 0.2f;
constexpr int kMinMicLevel = 12;
constexpr int kClippedLevelStep = 15;
constexpr float kClippedRatioThreshold = 0.1f;
constexpr int kClippedWaitFrames = 300;
constexpr float kHighSpeechProbability = 0.7f;
constexpr float kLowSpeechProbability = 0.1f;
constexpr float kSpeechLevel = -25.0f;
constexpr float kSpeechProbabilityThreshold = 0.5f;
constexpr float kSpeechRatioThreshold = 0.8f;

constexpr float kMinSample = std::numeric_limits<int16_t>::min();
constexpr float kMaxSample = std::numeric_limits<int16_t>::max();

using ClippingPredictorConfig = AudioProcessing::Config::GainController1::
    AnalogGainController::ClippingPredictor;

using InputVolumeControllerConfig = InputVolumeController::Config;

constexpr InputVolumeControllerConfig kDefaultInputVolumeControllerConfig{};
constexpr ClippingPredictorConfig kDefaultClippingPredictorConfig{};

std::unique_ptr<InputVolumeController> CreateInputVolumeController(
    int clipped_level_step,
    float clipped_ratio_threshold,
    int clipped_wait_frames,
    bool enable_clipping_predictor = false,
    int update_input_volume_wait_frames = 0) {
  InputVolumeControllerConfig config{
      .clipped_level_min = kClippedMin,
      .clipped_level_step = clipped_level_step,
      .clipped_ratio_threshold = clipped_ratio_threshold,
      .clipped_wait_frames = clipped_wait_frames,
      .enable_clipping_predictor = enable_clipping_predictor,
      .target_range_max_dbfs = -18,
      .target_range_min_dbfs = -30,
      .update_input_volume_wait_frames = update_input_volume_wait_frames,
      .speech_probability_threshold = kSpeechProbabilityThreshold,
      .speech_ratio_threshold = kSpeechRatioThreshold,
  };

  return std::make_unique<InputVolumeController>(/*num_capture_channels=*/1,
                                                 config);
}

// Deprecated.
// TODO(bugs.webrtc.org/7494): Delete this helper, use
// `InputVolumeControllerTestHelper::CallAgcSequence()` instead.
// Calls `AnalyzePreProcess()` on `manager` `num_calls` times. `peak_ratio` is a
// value in [0, 1] which determines the amplitude of the samples (1 maps to full
// scale). The first half of the calls is made on frames which are half filled
// with zeros in order to simulate a signal with different crest factors.
void CallPreProcessAudioBuffer(int num_calls,
                               float peak_ratio,
                               InputVolumeController& manager) {
  RTC_DCHECK_LE(peak_ratio, 1.0f);
  AudioBuffer audio_buffer(kSampleRateHz, kNumChannels, kSampleRateHz,
                           kNumChannels, kSampleRateHz, kNumChannels);
  const int num_channels = audio_buffer.num_channels();
  const int num_frames = audio_buffer.num_frames();

  // Make half of the calls with half zeroed frames.
  for (int ch = 0; ch < num_channels; ++ch) {
    // 50% of the samples in one frame are zero.
    for (int i = 0; i < num_frames; i += 2) {
      audio_buffer.channels()[ch][i] = peak_ratio * 32767.0f;
      audio_buffer.channels()[ch][i + 1] = 0.0f;
    }
  }
  for (int n = 0; n < num_calls / 2; ++n) {
    manager.AnalyzePreProcess(audio_buffer);
  }

  // Make the remaining half of the calls with frames whose samples are all set.
  for (int ch = 0; ch < num_channels; ++ch) {
    for (int i = 0; i < num_frames; ++i) {
      audio_buffer.channels()[ch][i] = peak_ratio * 32767.0f;
    }
  }
  for (int n = 0; n < num_calls - num_calls / 2; ++n) {
    manager.AnalyzePreProcess(audio_buffer);
  }
}

constexpr char kMinInputVolumeFieldTrial[] = "WebRTC-Audio-Agc2-MinInputVolume";

std::string GetAgcMinInputVolumeFieldTrial(const std::string& value) {
  char field_trial_buffer[64];
  rtc::SimpleStringBuilder builder(field_trial_buffer);
  builder << kMinInputVolumeFieldTrial << "/" << value << "/";
  return builder.str();
}

std::string GetAgcMinInputVolumeFieldTrialEnabled(
    int enabled_value,
    const std::string& suffix = "") {
  RTC_DCHECK_GE(enabled_value, 0);
  RTC_DCHECK_LE(enabled_value, 255);
  char field_trial_buffer[64];
  rtc::SimpleStringBuilder builder(field_trial_buffer);
  builder << kMinInputVolumeFieldTrial << "/Enabled-" << enabled_value << suffix
          << "/";
  return builder.str();
}

std::string GetAgcMinInputVolumeFieldTrial(absl::optional<int> volume) {
  if (volume.has_value()) {
    return GetAgcMinInputVolumeFieldTrialEnabled(*volume);
  }
  return GetAgcMinInputVolumeFieldTrial("Disabled");
}

// (Over)writes `samples_value` for the samples in `audio_buffer`.
// When `clipped_ratio`, a value in [0, 1], is greater than 0, the corresponding
// fraction of the frame is set to a full scale value to simulate clipping.
void WriteAudioBufferSamples(float samples_value,
                             float clipped_ratio,
                             AudioBuffer& audio_buffer) {
  RTC_DCHECK_GE(samples_value, kMinSample);
  RTC_DCHECK_LE(samples_value, kMaxSample);
  RTC_DCHECK_GE(clipped_ratio, 0.0f);
  RTC_DCHECK_LE(clipped_ratio, 1.0f);
  int num_channels = audio_buffer.num_channels();
  int num_samples = audio_buffer.num_frames();
  int num_clipping_samples = clipped_ratio * num_samples;
  for (int ch = 0; ch < num_channels; ++ch) {
    int i = 0;
    for (; i < num_clipping_samples; ++i) {
      audio_buffer.channels()[ch][i] = 32767.0f;
    }
    for (; i < num_samples; ++i) {
      audio_buffer.channels()[ch][i] = samples_value;
    }
  }
}

// Deprecated.
// TODO(bugs.webrtc.org/7494): Delete this helper, use
// `InputVolumeControllerTestHelper::CallAgcSequence()` instead.
void CallPreProcessAndProcess(int num_calls,
                              const AudioBuffer& audio_buffer,
                              float speech_probability,
                              absl::optional<float> speech_level,
                              InputVolumeController& manager) {
  for (int n = 0; n < num_calls; ++n) {
    manager.AnalyzePreProcess(audio_buffer);
    manager.Process(speech_probability, speech_level);
  }
}

// Reads a given number of 10 ms chunks from a PCM file and feeds them to
// `InputVolumeController`.
class SpeechSamplesReader {
 private:
  // Recording properties.
  static constexpr int kPcmSampleRateHz = 16000;
  static constexpr int kPcmNumChannels = 1;
  static constexpr int kPcmBytesPerSamples = sizeof(int16_t);

 public:
  SpeechSamplesReader()
      : is_(test::ResourcePath("audio_processing/agc/agc_audio", "pcm"),
            std::ios::binary | std::ios::ate),
        audio_buffer_(kPcmSampleRateHz,
                      kPcmNumChannels,
                      kPcmSampleRateHz,
                      kPcmNumChannels,
                      kPcmSampleRateHz,
                      kPcmNumChannels),
        buffer_(audio_buffer_.num_frames()),
        buffer_num_bytes_(buffer_.size() * kPcmBytesPerSamples) {
    RTC_CHECK(is_);
  }

  // Reads `num_frames` 10 ms frames from the beginning of the PCM file, applies
  // `gain_db` and feeds the frames into `agc` by calling `AnalyzePreProcess()`
  // and `Process()` for each frame. Reads the number of 10 ms frames available
  // in the PCM file if `num_frames` is too large - i.e., does not loop.
  // `speech_probability` and `speech_level` are passed to
  // `Process()`.
  void Feed(int num_frames,
            int gain_db,
            float speech_probability,
            absl::optional<float> speech_level,
            InputVolumeController& agc) {
    float gain = std::pow(10.0f, gain_db / 20.0f);  // From dB to linear gain.
    is_.seekg(0, is_.beg);  // Start from the beginning of the PCM file.

    // Read and feed frames.
    for (int i = 0; i < num_frames; ++i) {
      is_.read(reinterpret_cast<char*>(buffer_.data()), buffer_num_bytes_);
      if (is_.gcount() < buffer_num_bytes_) {
        // EOF reached. Stop.
        break;
      }
      // Apply gain and copy samples into `audio_buffer_`.
      std::transform(buffer_.begin(), buffer_.end(),
                     audio_buffer_.channels()[0], [gain](int16_t v) -> float {
                       return rtc::SafeClamp(static_cast<float>(v) * gain,
                                             kMinSample, kMaxSample);
                     });

      agc.AnalyzePreProcess(audio_buffer_);
      agc.Process(speech_probability, speech_level);
    }
  }

 private:
  std::ifstream is_;
  AudioBuffer audio_buffer_;
  std::vector<int16_t> buffer_;
  const std::streamsize buffer_num_bytes_;
};

// Runs the MonoInputVolumeControl processing sequence following the API
// contract. Returns the updated recommended input volume.
float UpdateRecommendedInputVolume(MonoInputVolumeController& controller,
                                   int applied_input_volume,
                                   float speech_probability,
                                   const absl::optional<float> rms_error_dbfs) {
  controller.set_stream_analog_level(applied_input_volume);
  EXPECT_EQ(controller.recommended_analog_level(), applied_input_volume);
  controller.Process(rms_error_dbfs, speech_probability);
  return controller.recommended_analog_level();
}

}  // namespace

// TODO(bugs.webrtc.org/12874): Use constexpr struct with designated
// initializers once fixed.
constexpr InputVolumeControllerConfig GetInputVolumeControllerTestConfig() {
  InputVolumeControllerConfig config{
      .clipped_level_min = kClippedMin,
      .clipped_level_step = kClippedLevelStep,
      .clipped_ratio_threshold = kClippedRatioThreshold,
      .clipped_wait_frames = kClippedWaitFrames,
      .enable_clipping_predictor = kDefaultClippingPredictorConfig.enabled,
      .target_range_max_dbfs = -18,
      .target_range_min_dbfs = -30,
      .update_input_volume_wait_frames = 0,
      .speech_probability_threshold = 0.5f,
      .speech_ratio_threshold = 1.0f,
  };
  return config;
}

// Helper class that provides an `InputVolumeController` instance with an
// `AudioBuffer` instance and `CallAgcSequence()`, a helper method that runs the
// `InputVolumeController` instance on the `AudioBuffer` one by sticking to the
// API contract.
class InputVolumeControllerTestHelper {
 public:
  // Ctor. Initializes `audio_buffer` with zeros.
  InputVolumeControllerTestHelper()
      : audio_buffer(kSampleRateHz,
                     kNumChannels,
                     kSampleRateHz,
                     kNumChannels,
                     kSampleRateHz,
                     kNumChannels),
        manager(/*num_capture_channels=*/1,
                GetInputVolumeControllerTestConfig()) {
    manager.Initialize();
    WriteAudioBufferSamples(/*samples_value=*/0.0f, /*clipped_ratio=*/0.0f,
                            audio_buffer);
  }

  // Calls the sequence of `InputVolumeController` methods according to the API
  // contract, namely:
  // - Sets the applied input volume;
  // - Uses `audio_buffer` to call `AnalyzePreProcess()` and `Process()`;
  //  Returns the recommended input volume.
  int CallAgcSequence(int applied_input_volume,
                      float speech_probability,
                      absl::optional<float> speech_level) {
    manager.set_stream_analog_level(applied_input_volume);
    manager.AnalyzePreProcess(audio_buffer);
    manager.Process(speech_probability, speech_level);

    return manager.recommended_analog_level();
  }

  // Deprecated.
  // TODO(bugs.webrtc.org/7494): Let the caller write `audio_buffer` and use
  // `CallAgcSequence()`.
  void CallProcess(int num_calls,
                   float speech_probability,
                   absl::optional<float> speech_level) {
    for (int i = 0; i < num_calls; ++i) {
      manager.Process(speech_probability, speech_level);
    }
  }

  // Deprecated.
  // TODO(bugs.webrtc.org/7494): Let the caller write `audio_buffer` and use
  // `CallAgcSequence()`.
  void CallPreProc(int num_calls, float clipped_ratio) {
    RTC_DCHECK_GE(clipped_ratio, 0.0f);
    RTC_DCHECK_LE(clipped_ratio, 1.0f);
    WriteAudioBufferSamples(/*samples_value=*/0.0f, clipped_ratio,
                            audio_buffer);
    for (int i = 0; i < num_calls; ++i) {
      manager.AnalyzePreProcess(audio_buffer);
    }
  }

  // Deprecated.
  // TODO(bugs.webrtc.org/7494): Let the caller write `audio_buffer` and use
  // `CallAgcSequence()`.
  void CallPreProcForChangingAudio(int num_calls, float peak_ratio) {
    RTC_DCHECK_GE(peak_ratio, 0.0f);
    RTC_DCHECK_LE(peak_ratio, 1.0f);
    const float samples_value = peak_ratio * 32767.0f;

    // Make half of the calls on a frame where the samples alternate
    // `sample_values` and zeros.
    WriteAudioBufferSamples(samples_value, /*clipped_ratio=*/0.0f,
                            audio_buffer);
    for (size_t ch = 0; ch < audio_buffer.num_channels(); ++ch) {
      for (size_t k = 1; k < audio_buffer.num_frames(); k += 2) {
        audio_buffer.channels()[ch][k] = 0.0f;
      }
    }
    for (int i = 0; i < num_calls / 2; ++i) {
      manager.AnalyzePreProcess(audio_buffer);
    }

    // Make half of thecalls on a frame where all the samples equal
    // `sample_values`.
    WriteAudioBufferSamples(samples_value, /*clipped_ratio=*/0.0f,
                            audio_buffer);
    for (int i = 0; i < num_calls - num_calls / 2; ++i) {
      manager.AnalyzePreProcess(audio_buffer);
    }
  }

  AudioBuffer audio_buffer;
  InputVolumeController manager;
};

class InputVolumeControllerParametrizedTest
    : public ::testing::TestWithParam<absl::optional<int>> {
 protected:
  InputVolumeControllerParametrizedTest()
      : field_trials_(GetAgcMinInputVolumeFieldTrial(GetParam())) {}

  bool IsMinMicLevelOverridden() const { return GetParam().has_value(); }
  int GetMinMicLevel() const { return GetParam().value_or(kMinMicLevel); }

 private:
  test::ScopedFieldTrials field_trials_;
};

INSTANTIATE_TEST_SUITE_P(,
                         InputVolumeControllerParametrizedTest,
                         ::testing::Values(absl::nullopt, 12, 20));

TEST_P(InputVolumeControllerParametrizedTest,
       StartupMinVolumeConfigurationIsRespected) {
  InputVolumeControllerTestHelper helper;

  helper.CallAgcSequence(kInitialInputVolume, kHighSpeechProbability,
                         kSpeechLevel);

  EXPECT_EQ(kInitialInputVolume, helper.manager.recommended_analog_level());
}

TEST_P(InputVolumeControllerParametrizedTest, MicVolumeResponseToRmsError) {
  InputVolumeControllerTestHelper helper;
  helper.CallAgcSequence(kInitialInputVolume, kHighSpeechProbability,
                         kSpeechLevel);

  // Inside the digital gain's window; no change of volume.
  helper.CallProcess(/*num_calls=*/1, kHighSpeechProbability, -23.0f);

  // Inside the digital gain's window; no change of volume.
  helper.CallProcess(/*num_calls=*/1, kHighSpeechProbability, -28.0f);

  // Above the digital gain's  window; volume should be increased.
  helper.CallProcess(/*num_calls=*/1, kHighSpeechProbability, -29.0f);
  EXPECT_EQ(128, helper.manager.recommended_analog_level());

  helper.CallProcess(/*num_calls=*/1, kHighSpeechProbability, -38.0f);
  EXPECT_EQ(156, helper.manager.recommended_analog_level());

  // Inside the digital gain's window; no change of volume.
  helper.CallProcess(/*num_calls=*/1, kHighSpeechProbability, -23.0f);
  helper.CallProcess(/*num_calls=*/1, kHighSpeechProbability, -18.0f);

  // Below the digial gain's window; volume should be decreased.
  helper.CallProcess(/*num_calls=*/1, kHighSpeechProbability, -17.0f);
  EXPECT_EQ(155, helper.manager.recommended_analog_level());

  helper.CallProcess(/*num_calls=*/1, kHighSpeechProbability, -17.0f);
  EXPECT_EQ(151, helper.manager.recommended_analog_level());

  helper.CallProcess(/*num_calls=*/1, kHighSpeechProbability, -9.0f);
  EXPECT_EQ(119, helper.manager.recommended_analog_level());
}

TEST_P(InputVolumeControllerParametrizedTest, MicVolumeIsLimited) {
  InputVolumeControllerTestHelper helper;
  helper.CallAgcSequence(kInitialInputVolume, kHighSpeechProbability,
                         kSpeechLevel);

  // Maximum upwards change is limited.
  helper.CallProcess(/*num_calls=*/1, kHighSpeechProbability, -48.0f);
  EXPECT_EQ(183, helper.manager.recommended_analog_level());

  helper.CallProcess(/*num_calls=*/1, kHighSpeechProbability, -48.0f);
  EXPECT_EQ(243, helper.manager.recommended_analog_level());

  // Won't go higher than the maximum.
  helper.CallProcess(/*num_calls=*/1, kHighSpeechProbability, -48.0f);
  EXPECT_EQ(255, helper.manager.recommended_analog_level());

  helper.CallProcess(/*num_calls=*/1, kHighSpeechProbability, -17.0f);
  EXPECT_EQ(254, helper.manager.recommended_analog_level());

  // Maximum downwards change is limited.
  helper.CallProcess(/*num_calls=*/1, kHighSpeechProbability, 22.0f);
  EXPECT_EQ(194, helper.manager.recommended_analog_level());

  helper.CallProcess(/*num_calls=*/1, kHighSpeechProbability, 22.0f);
  EXPECT_EQ(137, helper.manager.recommended_analog_level());

  helper.CallProcess(/*num_calls=*/1, kHighSpeechProbability, 22.0f);
  EXPECT_EQ(88, helper.manager.recommended_analog_level());

  helper.CallProcess(/*num_calls=*/1, kHighSpeechProbability, 22.0f);
  EXPECT_EQ(54, helper.manager.recommended_analog_level());

  helper.CallProcess(/*num_calls=*/1, kHighSpeechProbability, 22.0f);
  EXPECT_EQ(33, helper.manager.recommended_analog_level());

  // Won't go lower than the minimum.
  helper.CallProcess(/*num_calls=*/1, kHighSpeechProbability, 22.0f);
  EXPECT_EQ(std::max(18, GetMinMicLevel()),
            helper.manager.recommended_analog_level());

  helper.CallProcess(/*num_calls=*/1, kHighSpeechProbability, 22.0f);
  EXPECT_EQ(std::max(12, GetMinMicLevel()),
            helper.manager.recommended_analog_level());
}

TEST_P(InputVolumeControllerParametrizedTest, NoActionWhileMuted) {
  InputVolumeControllerTestHelper helper;
  helper.CallAgcSequence(kInitialInputVolume, kHighSpeechProbability,
                         kSpeechLevel);

  helper.manager.HandleCaptureOutputUsedChange(false);
  helper.manager.Process(kHighSpeechProbability, kSpeechLevel);
}

TEST_P(InputVolumeControllerParametrizedTest,
       UnmutingChecksVolumeWithoutRaising) {
  InputVolumeControllerTestHelper helper;
  helper.CallAgcSequence(kInitialInputVolume, kHighSpeechProbability,
                         kSpeechLevel);

  helper.manager.HandleCaptureOutputUsedChange(false);
  helper.manager.HandleCaptureOutputUsedChange(true);

  constexpr int kInputVolume = 127;
  helper.manager.set_stream_analog_level(kInputVolume);

  // SetMicVolume should not be called.
  helper.CallProcess(/*num_calls=*/1, kHighSpeechProbability, kSpeechLevel);
  EXPECT_EQ(127, helper.manager.recommended_analog_level());
}

TEST_P(InputVolumeControllerParametrizedTest, UnmutingRaisesTooLowVolume) {
  InputVolumeControllerTestHelper helper;
  helper.CallAgcSequence(kInitialInputVolume, kHighSpeechProbability,
                         kSpeechLevel);

  helper.manager.HandleCaptureOutputUsedChange(false);
  helper.manager.HandleCaptureOutputUsedChange(true);

  constexpr int kInputVolume = 11;
  helper.manager.set_stream_analog_level(kInputVolume);

  helper.CallProcess(/*num_calls=*/1, kHighSpeechProbability, kSpeechLevel);
  EXPECT_EQ(GetMinMicLevel(), helper.manager.recommended_analog_level());
}

TEST_P(InputVolumeControllerParametrizedTest,
       ManualLevelChangeResultsInNoSetMicCall) {
  InputVolumeControllerTestHelper helper;
  helper.CallAgcSequence(kInitialInputVolume, kHighSpeechProbability,
                         kSpeechLevel);

  // GetMicVolume returns a value outside of the quantization slack, indicating
  // a manual volume change.
  ASSERT_NE(helper.manager.recommended_analog_level(), 154);
  helper.manager.set_stream_analog_level(154);
  helper.CallProcess(/*num_calls=*/1, kHighSpeechProbability, -29.0f);
  EXPECT_EQ(154, helper.manager.recommended_analog_level());

  // Do the same thing, except downwards now.
  helper.manager.set_stream_analog_level(100);
  helper.CallProcess(/*num_calls=*/1, kHighSpeechProbability, -17.0f);
  EXPECT_EQ(100, helper.manager.recommended_analog_level());

  // And finally verify the AGC continues working without a manual change.
  helper.CallProcess(/*num_calls=*/1, kHighSpeechProbability, -17.0f);
  EXPECT_EQ(99, helper.manager.recommended_analog_level());
}

TEST_P(InputVolumeControllerParametrizedTest,
       RecoveryAfterManualLevelChangeFromMax) {
  InputVolumeControllerTestHelper helper;
  helper.CallAgcSequence(kInitialInputVolume, kHighSpeechProbability,
                         kSpeechLevel);

  // Force the mic up to max volume. Takes a few steps due to the residual
  // gain limitation.
  helper.CallProcess(/*num_calls=*/1, kHighSpeechProbability, -48.0f);
  EXPECT_EQ(183, helper.manager.recommended_analog_level());
  helper.CallProcess(/*num_calls=*/1, kHighSpeechProbability, -48.0f);
  EXPECT_EQ(243, helper.manager.recommended_analog_level());
  helper.CallProcess(/*num_calls=*/1, kHighSpeechProbability, -48.0f);
  EXPECT_EQ(255, helper.manager.recommended_analog_level());

  // Manual change does not result in SetMicVolume call.
  helper.manager.set_stream_analog_level(50);
  helper.CallProcess(/*num_calls=*/1, kHighSpeechProbability, -17.0f);
  EXPECT_EQ(50, helper.manager.recommended_analog_level());

  // Continues working as usual afterwards.
  helper.CallProcess(/*num_calls=*/1, kHighSpeechProbability, -38.0f);

  EXPECT_EQ(65, helper.manager.recommended_analog_level());
}

// Checks that the minimum input volume is enforced during the upward adjustment
// of the input volume.
TEST_P(InputVolumeControllerParametrizedTest,
       EnforceMinInputVolumeDuringUpwardsAdjustment) {
  InputVolumeControllerTestHelper helper;
  helper.CallAgcSequence(kInitialInputVolume, kHighSpeechProbability,
                         kSpeechLevel);

  // Manual change below min, but strictly positive, otherwise no action will be
  // taken.
  helper.manager.set_stream_analog_level(1);
  helper.CallProcess(/*num_calls=*/1, kHighSpeechProbability, -17.0f);

  // Trigger an upward adjustment of the input volume.
  EXPECT_EQ(helper.manager.recommended_analog_level(), GetMinMicLevel());
  helper.CallProcess(/*num_calls=*/1, kHighSpeechProbability, -29.0f);
  EXPECT_EQ(helper.manager.recommended_analog_level(), GetMinMicLevel());
  helper.CallProcess(/*num_calls=*/1, kHighSpeechProbability, -48.0f);
  EXPECT_EQ(helper.manager.recommended_analog_level(), GetMinMicLevel());

  // After a number of consistently low speech level observations, the input
  // volume is eventually raised above the minimum.
  helper.CallProcess(/*num_calls=*/10, kHighSpeechProbability, -38.0f);
  EXPECT_GT(helper.manager.recommended_analog_level(), GetMinMicLevel());
}

// Checks that, when the min mic level override is specified, AGC immediately
// applies the minimum mic level after the mic level is manually set below the
// minimum gain to enforce.
TEST_P(InputVolumeControllerParametrizedTest,
       RecoveryAfterManualLevelChangeBelowMin) {
  InputVolumeControllerTestHelper helper;
  helper.CallAgcSequence(kInitialInputVolume, kHighSpeechProbability,
                         kSpeechLevel);

  // Manual change below min, but strictly positive, otherwise
  // AGC won't take any action.
  helper.manager.set_stream_analog_level(1);
  helper.CallProcess(/*num_calls=*/1, kHighSpeechProbability, -17.0f);
  EXPECT_EQ(GetMinMicLevel(), helper.manager.recommended_analog_level());
}

TEST_P(InputVolumeControllerParametrizedTest, NoClippingHasNoImpact) {
  InputVolumeControllerTestHelper helper;
  helper.CallAgcSequence(kInitialInputVolume, kHighSpeechProbability,
                         kSpeechLevel);

  helper.CallPreProc(/*num_calls=*/100, /*clipped_ratio=*/0);
  EXPECT_EQ(128, helper.manager.recommended_analog_level());
}

TEST_P(InputVolumeControllerParametrizedTest,
       ClippingUnderThresholdHasNoImpact) {
  InputVolumeControllerTestHelper helper;
  helper.CallAgcSequence(kInitialInputVolume, kHighSpeechProbability,
                         kSpeechLevel);

  helper.CallPreProc(/*num_calls=*/1, /*clipped_ratio=*/0.099);
  EXPECT_EQ(128, helper.manager.recommended_analog_level());
}

TEST_P(InputVolumeControllerParametrizedTest, ClippingLowersVolume) {
  InputVolumeControllerTestHelper helper;
  helper.CallAgcSequence(/*applied_input_volume=*/255, kHighSpeechProbability,
                         kSpeechLevel);

  helper.CallPreProc(/*num_calls=*/1, /*clipped_ratio=*/0.2);
  EXPECT_EQ(240, helper.manager.recommended_analog_level());
}

TEST_P(InputVolumeControllerParametrizedTest,
       WaitingPeriodBetweenClippingChecks) {
  InputVolumeControllerTestHelper helper;
  helper.CallAgcSequence(/*applied_input_volume=*/255, kHighSpeechProbability,
                         kSpeechLevel);

  helper.CallPreProc(/*num_calls=*/1, /*clipped_ratio=*/kAboveClippedThreshold);
  EXPECT_EQ(240, helper.manager.recommended_analog_level());

  helper.CallPreProc(/*num_calls=*/300,
                     /*clipped_ratio=*/kAboveClippedThreshold);
  EXPECT_EQ(240, helper.manager.recommended_analog_level());

  helper.CallPreProc(/*num_calls=*/1, /*clipped_ratio=*/kAboveClippedThreshold);
  EXPECT_EQ(225, helper.manager.recommended_analog_level());
}

TEST_P(InputVolumeControllerParametrizedTest, ClippingLoweringIsLimited) {
  InputVolumeControllerTestHelper helper;
  helper.CallAgcSequence(/*applied_input_volume=*/180, kHighSpeechProbability,
                         kSpeechLevel);

  helper.CallPreProc(/*num_calls=*/1, /*clipped_ratio=*/kAboveClippedThreshold);
  EXPECT_EQ(kClippedMin, helper.manager.recommended_analog_level());

  helper.CallPreProc(/*num_calls=*/1000,
                     /*clipped_ratio=*/kAboveClippedThreshold);
  EXPECT_EQ(kClippedMin, helper.manager.recommended_analog_level());
}

TEST_P(InputVolumeControllerParametrizedTest,
       ClippingMaxIsRespectedWhenEqualToLevel) {
  InputVolumeControllerTestHelper helper;
  helper.CallAgcSequence(/*applied_input_volume=*/255, kHighSpeechProbability,
                         kSpeechLevel);

  helper.CallPreProc(/*num_calls=*/1, /*clipped_ratio=*/kAboveClippedThreshold);
  EXPECT_EQ(240, helper.manager.recommended_analog_level());

  helper.CallProcess(/*num_calls=*/10, kHighSpeechProbability, -48.0f);
  EXPECT_EQ(240, helper.manager.recommended_analog_level());
}

TEST_P(InputVolumeControllerParametrizedTest,
       ClippingMaxIsRespectedWhenHigherThanLevel) {
  InputVolumeControllerTestHelper helper;
  helper.CallAgcSequence(/*applied_input_volume=*/200, kHighSpeechProbability,
                         kSpeechLevel);

  helper.CallPreProc(/*num_calls=*/1, /*clipped_ratio=*/kAboveClippedThreshold);
  EXPECT_EQ(185, helper.manager.recommended_analog_level());

  helper.CallProcess(/*num_calls=*/1, kHighSpeechProbability, -58.0f);
  EXPECT_EQ(240, helper.manager.recommended_analog_level());
  helper.CallProcess(/*num_calls=*/10, kHighSpeechProbability, -58.0f);
  EXPECT_EQ(240, helper.manager.recommended_analog_level());
}

TEST_P(InputVolumeControllerParametrizedTest, UserCanRaiseVolumeAfterClipping) {
  InputVolumeControllerTestHelper helper;
  helper.CallAgcSequence(/*applied_input_volume=*/225, kHighSpeechProbability,
                         kSpeechLevel);

  helper.CallPreProc(/*num_calls=*/1, /*clipped_ratio=*/kAboveClippedThreshold);
  EXPECT_EQ(210, helper.manager.recommended_analog_level());

  // User changed the volume.
  helper.manager.set_stream_analog_level(250);
  helper.CallProcess(/*num_calls=*/1, kHighSpeechProbability, -32.0f);
  EXPECT_EQ(250, helper.manager.recommended_analog_level());

  // Move down...
  helper.CallProcess(/*num_calls=*/1, kHighSpeechProbability, -8.0f);
  EXPECT_EQ(210, helper.manager.recommended_analog_level());
  // And back up to the new max established by the user.
  helper.CallProcess(/*num_calls=*/1, kHighSpeechProbability, -58.0f);
  EXPECT_EQ(250, helper.manager.recommended_analog_level());
  // Will not move above new maximum.
  helper.CallProcess(/*num_calls=*/1, kHighSpeechProbability, -48.0f);
  EXPECT_EQ(250, helper.manager.recommended_analog_level());
}

TEST_P(InputVolumeControllerParametrizedTest,
       ClippingDoesNotPullLowVolumeBackUp) {
  InputVolumeControllerTestHelper helper;
  helper.CallAgcSequence(/*applied_input_volume=*/80, kHighSpeechProbability,
                         kSpeechLevel);

  int initial_volume = helper.manager.recommended_analog_level();
  helper.CallPreProc(/*num_calls=*/1, /*clipped_ratio=*/kAboveClippedThreshold);
  EXPECT_EQ(initial_volume, helper.manager.recommended_analog_level());
}

TEST_P(InputVolumeControllerParametrizedTest, TakesNoActionOnZeroMicVolume) {
  InputVolumeControllerTestHelper helper;
  helper.CallAgcSequence(kInitialInputVolume, kHighSpeechProbability,
                         kSpeechLevel);

  helper.manager.set_stream_analog_level(0);
  helper.CallProcess(/*num_calls=*/10, kHighSpeechProbability, -48.0f);
  EXPECT_EQ(0, helper.manager.recommended_analog_level());
}

TEST_P(InputVolumeControllerParametrizedTest, ClippingDetectionLowersVolume) {
  InputVolumeControllerTestHelper helper;
  helper.CallAgcSequence(/*applied_input_volume=*/255, kHighSpeechProbability,
                         kSpeechLevel);

  EXPECT_EQ(255, helper.manager.recommended_analog_level());
  helper.CallPreProcForChangingAudio(/*num_calls=*/100, /*peak_ratio=*/0.99f);
  EXPECT_EQ(255, helper.manager.recommended_analog_level());
  helper.CallPreProcForChangingAudio(/*num_calls=*/100, /*peak_ratio=*/1.0f);
  EXPECT_EQ(240, helper.manager.recommended_analog_level());
}

TEST_P(InputVolumeControllerParametrizedTest,
       DisabledClippingPredictorDoesNotLowerVolume) {
  InputVolumeControllerTestHelper helper;
  helper.CallAgcSequence(/*applied_input_volume=*/255, kHighSpeechProbability,
                         kSpeechLevel);

  EXPECT_FALSE(helper.manager.clipping_predictor_enabled());
  EXPECT_EQ(255, helper.manager.recommended_analog_level());
  helper.CallPreProcForChangingAudio(/*num_calls=*/100, /*peak_ratio=*/0.99f);
  EXPECT_EQ(255, helper.manager.recommended_analog_level());
  helper.CallPreProcForChangingAudio(/*num_calls=*/100, /*peak_ratio=*/0.99f);
  EXPECT_EQ(255, helper.manager.recommended_analog_level());
}

TEST(InputVolumeControllerTest, AgcMinMicLevelExperimentDefault) {
  std::unique_ptr<InputVolumeController> manager = CreateInputVolumeController(
      kClippedLevelStep, kClippedRatioThreshold, kClippedWaitFrames);
  EXPECT_EQ(manager->channel_controllers_[0]->min_input_volume(), kMinMicLevel);
}

TEST(InputVolumeControllerTest, AgcMinMicLevelExperimentDisabled) {
  for (const std::string& field_trial_suffix : {"", "_20220210"}) {
    test::ScopedFieldTrials field_trial(
        GetAgcMinInputVolumeFieldTrial("Disabled" + field_trial_suffix));
    std::unique_ptr<InputVolumeController> manager =
        CreateInputVolumeController(kClippedLevelStep, kClippedRatioThreshold,
                                    kClippedWaitFrames);

    EXPECT_EQ(manager->channel_controllers_[0]->min_input_volume(),
              kMinMicLevel);
  }
}

// Checks that a field-trial parameter outside of the valid range [0,255] is
// ignored.
TEST(InputVolumeControllerTest, AgcMinMicLevelExperimentOutOfRangeAbove) {
  test::ScopedFieldTrials field_trial(
      GetAgcMinInputVolumeFieldTrial("Enabled-256"));
  std::unique_ptr<InputVolumeController> manager = CreateInputVolumeController(
      kClippedLevelStep, kClippedRatioThreshold, kClippedWaitFrames);
  EXPECT_EQ(manager->channel_controllers_[0]->min_input_volume(), kMinMicLevel);
}

// Checks that a field-trial parameter outside of the valid range [0,255] is
// ignored.
TEST(InputVolumeControllerTest, AgcMinMicLevelExperimentOutOfRangeBelow) {
  test::ScopedFieldTrials field_trial(
      GetAgcMinInputVolumeFieldTrial("Enabled--1"));
  std::unique_ptr<InputVolumeController> manager = CreateInputVolumeController(
      kClippedLevelStep, kClippedRatioThreshold, kClippedWaitFrames);
  EXPECT_EQ(manager->channel_controllers_[0]->min_input_volume(), kMinMicLevel);
}

// Verifies that a valid experiment changes the minimum microphone level. The
// start volume is larger than the min level and should therefore not be
// changed.
TEST(InputVolumeControllerTest, AgcMinMicLevelExperimentEnabled50) {
  constexpr int kMinMicLevelOverride = 50;
  for (const std::string& field_trial_suffix : {"", "_20220210"}) {
    SCOPED_TRACE(field_trial_suffix);
    test::ScopedFieldTrials field_trial(GetAgcMinInputVolumeFieldTrialEnabled(
        kMinMicLevelOverride, field_trial_suffix));
    std::unique_ptr<InputVolumeController> manager =
        CreateInputVolumeController(kClippedLevelStep, kClippedRatioThreshold,
                                    kClippedWaitFrames);

    EXPECT_EQ(manager->channel_controllers_[0]->min_input_volume(),
              kMinMicLevelOverride);
  }
}

// Checks that, when the "WebRTC-Audio-AgcMinMicLevelExperiment" field trial is
// specified with a valid value, the mic level never gets lowered beyond the
// override value in the presence of clipping.
TEST(InputVolumeControllerTest,
     AgcMinMicLevelExperimentCheckMinLevelWithClipping) {
  constexpr int kMinMicLevelOverride = 250;

  // Create and initialize two AGCs by specifying and leaving unspecified the
  // relevant field trial.
  const auto factory = []() {
    std::unique_ptr<InputVolumeController> manager =
        CreateInputVolumeController(kClippedLevelStep, kClippedRatioThreshold,
                                    kClippedWaitFrames);
    manager->Initialize();
    manager->set_stream_analog_level(kInitialInputVolume);
    return manager;
  };
  std::unique_ptr<InputVolumeController> manager = factory();
  std::unique_ptr<InputVolumeController> manager_with_override;
  {
    test::ScopedFieldTrials field_trial(
        GetAgcMinInputVolumeFieldTrialEnabled(kMinMicLevelOverride));
    manager_with_override = factory();
  }

  // Create a test input signal which containts 80% of clipped samples.
  AudioBuffer audio_buffer(kSampleRateHz, 1, kSampleRateHz, 1, kSampleRateHz,
                           1);
  WriteAudioBufferSamples(/*samples_value=*/4000.0f, /*clipped_ratio=*/0.8f,
                          audio_buffer);

  // Simulate 4 seconds of clipping; it is expected to trigger a downward
  // adjustment of the analog gain. Use low speech probability to limit the
  // volume changes to clipping handling.
  CallPreProcessAndProcess(/*num_calls=*/400, audio_buffer,
                           kLowSpeechProbability, /*speech_level=*/-42.0f,
                           *manager);
  CallPreProcessAndProcess(/*num_calls=*/400, audio_buffer,
                           kLowSpeechProbability, /*speech_level=*/-42.0f,
                           *manager_with_override);

  // Make sure that an adaptation occurred.
  ASSERT_GT(manager->recommended_analog_level(), 0);

  // Check that the test signal triggers a larger downward adaptation for
  // `manager`, which is allowed to reach a lower gain.
  EXPECT_GT(manager_with_override->recommended_analog_level(),
            manager->recommended_analog_level());
  // Check that the gain selected by `manager_with_override` equals the
  // minimum value overridden via field trial.
  EXPECT_EQ(manager_with_override->recommended_analog_level(),
            kMinMicLevelOverride);
}

// Checks that, when the "WebRTC-Audio-AgcMinMicLevelExperiment" field trial is
// specified with a valid value, the mic level never gets lowered beyond the
// override value in the presence of clipping when RMS error is not empty.
// TODO(webrtc:7494): Revisit the test after moving the number of update wait
// frames to APM config. The test passes but internally the gain update timing
// differs.
TEST(InputVolumeControllerTest,
     AgcMinMicLevelExperimentCheckMinLevelWithClippingWithRmsError) {
  constexpr int kMinMicLevelOverride = 250;

  // Create and initialize two AGCs by specifying and leaving unspecified the
  // relevant field trial.
  const auto factory = []() {
    std::unique_ptr<InputVolumeController> manager =
        CreateInputVolumeController(kClippedLevelStep, kClippedRatioThreshold,
                                    kClippedWaitFrames);
    manager->Initialize();
    manager->set_stream_analog_level(kInitialInputVolume);
    return manager;
  };
  std::unique_ptr<InputVolumeController> manager = factory();
  std::unique_ptr<InputVolumeController> manager_with_override;
  {
    test::ScopedFieldTrials field_trial(
        GetAgcMinInputVolumeFieldTrialEnabled(kMinMicLevelOverride));
    manager_with_override = factory();
  }

  // Create a test input signal which containts 80% of clipped samples.
  AudioBuffer audio_buffer(kSampleRateHz, 1, kSampleRateHz, 1, kSampleRateHz,
                           1);
  WriteAudioBufferSamples(/*samples_value=*/4000.0f, /*clipped_ratio=*/0.8f,
                          audio_buffer);

  // Simulate 4 seconds of clipping; it is expected to trigger a downward
  // adjustment of the analog gain.
  CallPreProcessAndProcess(
      /*num_calls=*/400, audio_buffer, kHighSpeechProbability,
      /*speech_level=*/-18.0f, *manager);
  CallPreProcessAndProcess(
      /*num_calls=*/400, audio_buffer, kHighSpeechProbability,
      /*speech_level=*/-18.0f, *manager_with_override);

  // Make sure that an adaptation occurred.
  ASSERT_GT(manager->recommended_analog_level(), 0);

  // Check that the test signal triggers a larger downward adaptation for
  // `manager`, which is allowed to reach a lower gain.
  EXPECT_GT(manager_with_override->recommended_analog_level(),
            manager->recommended_analog_level());
  // Check that the gain selected by `manager_with_override` equals the minimum
  // value overridden via field trial.
  EXPECT_EQ(manager_with_override->recommended_analog_level(),
            kMinMicLevelOverride);
}

// Checks that, when the "WebRTC-Audio-AgcMinMicLevelExperiment" field trial is
// specified with a value lower than the `clipped_level_min`, the behavior of
// the analog gain controller is the same as that obtained when the field trial
// is not specified.
TEST(InputVolumeControllerTest,
     AgcMinMicLevelExperimentCompareMicLevelWithClipping) {
  // Create and initialize two AGCs by specifying and leaving unspecified the
  // relevant field trial.
  const auto factory = []() {
    // Use a large clipped level step to more quickly decrease the analog gain
    // with clipping.
    InputVolumeControllerConfig config = kDefaultInputVolumeControllerConfig;
    config.clipped_level_step = 64;
    config.clipped_ratio_threshold = kClippedRatioThreshold;
    config.clipped_wait_frames = kClippedWaitFrames;
    auto controller = std::make_unique<InputVolumeController>(
        /*num_capture_channels=*/1, config);
    controller->Initialize();
    controller->set_stream_analog_level(kInitialInputVolume);
    return controller;
  };
  std::unique_ptr<InputVolumeController> manager = factory();
  std::unique_ptr<InputVolumeController> manager_with_override;
  {
    constexpr int kMinMicLevelOverride = 20;
    static_assert(kDefaultInputVolumeControllerConfig.clipped_level_min >=
                      kMinMicLevelOverride,
                  "Use a lower override value.");
    test::ScopedFieldTrials field_trial(
        GetAgcMinInputVolumeFieldTrialEnabled(kMinMicLevelOverride));
    manager_with_override = factory();
  }

  // Create a test input signal which containts 80% of clipped samples.
  AudioBuffer audio_buffer(kSampleRateHz, 1, kSampleRateHz, 1, kSampleRateHz,
                           1);
  WriteAudioBufferSamples(/*samples_value=*/4000.0f, /*clipped_ratio=*/0.8f,
                          audio_buffer);

  // Simulate 4 seconds of clipping; it is expected to trigger a downward
  // adjustment of the analog gain. Use low speech probability to limit the
  // volume changes to clipping handling.
  CallPreProcessAndProcess(/*num_calls=*/400, audio_buffer,
                           kLowSpeechProbability, /*speech_level=*/-18,
                           *manager);
  CallPreProcessAndProcess(/*num_calls=*/400, audio_buffer,
                           kLowSpeechProbability, /*speech_level=*/-18,
                           *manager_with_override);

  // Make sure that an adaptation occurred.
  ASSERT_GT(manager->recommended_analog_level(), 0);

  // Check that the selected analog gain is the same for both controllers and
  // that it equals the minimum level reached when clipping is handled. That is
  // expected because the minimum microphone level override is less than the
  // minimum level used when clipping is detected.
  EXPECT_EQ(manager->recommended_analog_level(),
            manager_with_override->recommended_analog_level());
  EXPECT_EQ(manager_with_override->recommended_analog_level(),
            kDefaultInputVolumeControllerConfig.clipped_level_min);
}

// Checks that, when the "WebRTC-Audio-AgcMinMicLevelExperiment" field trial is
// specified with a value lower than the `clipped_level_min`, the behavior of
// the analog gain controller is the same as that obtained when the field trial
// is not specified.
// TODO(webrtc:7494): Revisit the test after moving the number of update wait
// frames to APM config. The test passes but internally the gain update timing
// differs.
TEST(InputVolumeControllerTest,
     AgcMinMicLevelExperimentCompareMicLevelWithClippingWithRmsError) {
  // Create and initialize two AGCs by specifying and leaving unspecified the
  // relevant field trial.
  const auto factory = []() {
    // Use a large clipped level step to more quickly decrease the analog gain
    // with clipping.
    InputVolumeControllerConfig config = kDefaultInputVolumeControllerConfig;
    config.clipped_level_step = 64;
    config.clipped_ratio_threshold = kClippedRatioThreshold;
    config.clipped_wait_frames = kClippedWaitFrames;
    auto controller = std::make_unique<InputVolumeController>(
        /*num_capture_channels=*/1, config);
    controller->Initialize();
    controller->set_stream_analog_level(kInitialInputVolume);
    return controller;
  };
  std::unique_ptr<InputVolumeController> manager = factory();
  std::unique_ptr<InputVolumeController> manager_with_override;
  {
    constexpr int kMinMicLevelOverride = 20;
    static_assert(kDefaultInputVolumeControllerConfig.clipped_level_min >=
                      kMinMicLevelOverride,
                  "Use a lower override value.");
    test::ScopedFieldTrials field_trial(
        GetAgcMinInputVolumeFieldTrialEnabled(kMinMicLevelOverride));
    manager_with_override = factory();
  }

  // Create a test input signal which containts 80% of clipped samples.
  AudioBuffer audio_buffer(kSampleRateHz, 1, kSampleRateHz, 1, kSampleRateHz,
                           1);
  WriteAudioBufferSamples(/*samples_value=*/4000.0f, /*clipped_ratio=*/0.8f,
                          audio_buffer);

  CallPreProcessAndProcess(
      /*num_calls=*/400, audio_buffer,
      /*speech_probability=*/0.7f,
      /*speech_level=*/-18.0f, *manager);
  CallPreProcessAndProcess(
      /*num_calls=*/400, audio_buffer,
      /*speech_probability=*/0.7f,
      /*speech_level=*/-18.0f, *manager_with_override);

  // Make sure that an adaptation occurred.
  ASSERT_GT(manager->recommended_analog_level(), 0);

  // Check that the selected analog gain is the same for both controllers and
  // that it equals the minimum level reached when clipping is handled. That is
  // expected because the minimum microphone level override is less than the
  // minimum level used when clipping is detected.
  EXPECT_EQ(manager->recommended_analog_level(),
            manager_with_override->recommended_analog_level());
  EXPECT_EQ(manager_with_override->recommended_analog_level(),
            kDefaultInputVolumeControllerConfig.clipped_level_min);
}

// TODO(bugs.webrtc.org/12774): Test the bahavior of `clipped_level_step`.
// TODO(bugs.webrtc.org/12774): Test the bahavior of `clipped_ratio_threshold`.
// TODO(bugs.webrtc.org/12774): Test the bahavior of `clipped_wait_frames`.
// Verifies that configurable clipping parameters are initialized as intended.
TEST_P(InputVolumeControllerParametrizedTest, ClippingParametersVerified) {
  std::unique_ptr<InputVolumeController> manager = CreateInputVolumeController(
      kClippedLevelStep, kClippedRatioThreshold, kClippedWaitFrames);
  manager->Initialize();
  EXPECT_EQ(manager->clipped_level_step_, kClippedLevelStep);
  EXPECT_EQ(manager->clipped_ratio_threshold_, kClippedRatioThreshold);
  EXPECT_EQ(manager->clipped_wait_frames_, kClippedWaitFrames);
  std::unique_ptr<InputVolumeController> manager_custom =
      CreateInputVolumeController(/*clipped_level_step=*/10,
                                  /*clipped_ratio_threshold=*/0.2f,
                                  /*clipped_wait_frames=*/50);
  manager_custom->Initialize();
  EXPECT_EQ(manager_custom->clipped_level_step_, 10);
  EXPECT_EQ(manager_custom->clipped_ratio_threshold_, 0.2f);
  EXPECT_EQ(manager_custom->clipped_wait_frames_, 50);
}

TEST_P(InputVolumeControllerParametrizedTest,
       DisableClippingPredictorDisablesClippingPredictor) {
  std::unique_ptr<InputVolumeController> manager = CreateInputVolumeController(
      kClippedLevelStep, kClippedRatioThreshold, kClippedWaitFrames,
      /*enable_clipping_predictor=*/false);
  manager->Initialize();

  EXPECT_FALSE(manager->clipping_predictor_enabled());
  EXPECT_FALSE(manager->use_clipping_predictor_step());
}

TEST_P(InputVolumeControllerParametrizedTest,
       EnableClippingPredictorEnablesClippingPredictor) {
  std::unique_ptr<InputVolumeController> manager = CreateInputVolumeController(
      kClippedLevelStep, kClippedRatioThreshold, kClippedWaitFrames,
      /*enable_clipping_predictor=*/true);
  manager->Initialize();

  EXPECT_TRUE(manager->clipping_predictor_enabled());
  EXPECT_TRUE(manager->use_clipping_predictor_step());
}

TEST_P(InputVolumeControllerParametrizedTest,
       DisableClippingPredictorDoesNotLowerVolume) {
  AudioBuffer audio_buffer(kSampleRateHz, kNumChannels, kSampleRateHz,
                           kNumChannels, kSampleRateHz, kNumChannels);

  InputVolumeControllerConfig config = GetInputVolumeControllerTestConfig();
  config.enable_clipping_predictor = false;
  InputVolumeController manager(/*num_capture_channels=*/1, config);
  manager.Initialize();
  manager.set_stream_analog_level(/*level=*/255);
  EXPECT_FALSE(manager.clipping_predictor_enabled());
  EXPECT_FALSE(manager.use_clipping_predictor_step());
  EXPECT_EQ(manager.recommended_analog_level(), 255);
  manager.Process(kHighSpeechProbability, kSpeechLevel);
  CallPreProcessAudioBuffer(/*num_calls=*/10, /*peak_ratio=*/0.99f, manager);
  EXPECT_EQ(manager.recommended_analog_level(), 255);
  CallPreProcessAudioBuffer(/*num_calls=*/300, /*peak_ratio=*/0.99f, manager);
  EXPECT_EQ(manager.recommended_analog_level(), 255);
  CallPreProcessAudioBuffer(/*num_calls=*/10, /*peak_ratio=*/0.99f, manager);
  EXPECT_EQ(manager.recommended_analog_level(), 255);
}

TEST_P(InputVolumeControllerParametrizedTest,
       UsedClippingPredictionsProduceLowerAnalogLevels) {
  AudioBuffer audio_buffer(kSampleRateHz, kNumChannels, kSampleRateHz,
                           kNumChannels, kSampleRateHz, kNumChannels);

  InputVolumeControllerConfig config_with_prediction =
      GetInputVolumeControllerTestConfig();
  config_with_prediction.enable_clipping_predictor = true;

  InputVolumeControllerConfig config_without_prediction =
      GetInputVolumeControllerTestConfig();

  config_without_prediction.enable_clipping_predictor = false;
  InputVolumeController manager_without_prediction(/*num_capture_channels=*/1,
                                                   config_without_prediction);
  InputVolumeController manager_with_prediction(/*num_capture_channels=*/1,
                                                config_with_prediction);

  manager_with_prediction.Initialize();
  manager_without_prediction.Initialize();

  constexpr int kInitialLevel = 255;
  constexpr float kClippingPeakRatio = 1.0f;
  constexpr float kCloseToClippingPeakRatio = 0.99f;
  constexpr float kZeroPeakRatio = 0.0f;
  manager_with_prediction.set_stream_analog_level(kInitialLevel);
  manager_without_prediction.set_stream_analog_level(kInitialLevel);

  manager_with_prediction.Process(kHighSpeechProbability, kSpeechLevel);
  manager_without_prediction.Process(kHighSpeechProbability, kSpeechLevel);

  EXPECT_TRUE(manager_with_prediction.clipping_predictor_enabled());
  EXPECT_FALSE(manager_without_prediction.clipping_predictor_enabled());
  EXPECT_TRUE(manager_with_prediction.use_clipping_predictor_step());
  EXPECT_EQ(manager_with_prediction.recommended_analog_level(), kInitialLevel);
  EXPECT_EQ(manager_without_prediction.recommended_analog_level(),
            kInitialLevel);

  // Expect a change in the analog level when the prediction step is used.
  CallPreProcessAudioBuffer(/*num_calls=*/10, kCloseToClippingPeakRatio,
                            manager_with_prediction);
  CallPreProcessAudioBuffer(/*num_calls=*/10, kCloseToClippingPeakRatio,
                            manager_without_prediction);
  EXPECT_EQ(manager_with_prediction.recommended_analog_level(),
            kInitialLevel - kClippedLevelStep);
  EXPECT_EQ(manager_without_prediction.recommended_analog_level(),
            kInitialLevel);

  // Expect no change during waiting.
  CallPreProcessAudioBuffer(kClippedWaitFrames, kCloseToClippingPeakRatio,
                            manager_with_prediction);
  CallPreProcessAudioBuffer(kClippedWaitFrames, kCloseToClippingPeakRatio,
                            manager_without_prediction);
  EXPECT_EQ(manager_with_prediction.recommended_analog_level(),
            kInitialLevel - kClippedLevelStep);
  EXPECT_EQ(manager_without_prediction.recommended_analog_level(),
            kInitialLevel);

  // Expect a change when the prediction step is used.
  CallPreProcessAudioBuffer(/*num_calls=*/10, kCloseToClippingPeakRatio,
                            manager_with_prediction);
  CallPreProcessAudioBuffer(/*num_calls=*/10, kCloseToClippingPeakRatio,
                            manager_without_prediction);
  EXPECT_EQ(manager_with_prediction.recommended_analog_level(),
            kInitialLevel - 2 * kClippedLevelStep);
  EXPECT_EQ(manager_without_prediction.recommended_analog_level(),
            kInitialLevel);

  // Expect no change when clipping is not detected or predicted.
  CallPreProcessAudioBuffer(2 * kClippedWaitFrames, kZeroPeakRatio,
                            manager_with_prediction);
  CallPreProcessAudioBuffer(2 * kClippedWaitFrames, kZeroPeakRatio,
                            manager_without_prediction);
  EXPECT_EQ(manager_with_prediction.recommended_analog_level(),
            kInitialLevel - 2 * kClippedLevelStep);
  EXPECT_EQ(manager_without_prediction.recommended_analog_level(),
            kInitialLevel);

  // Expect a change for clipping frames.
  CallPreProcessAudioBuffer(/*num_calls=*/1, kClippingPeakRatio,
                            manager_with_prediction);
  CallPreProcessAudioBuffer(/*num_calls=*/1, kClippingPeakRatio,
                            manager_without_prediction);
  EXPECT_EQ(manager_with_prediction.recommended_analog_level(),
            kInitialLevel - 3 * kClippedLevelStep);
  EXPECT_EQ(manager_without_prediction.recommended_analog_level(),
            kInitialLevel - kClippedLevelStep);

  // Expect no change during waiting.
  CallPreProcessAudioBuffer(kClippedWaitFrames, kClippingPeakRatio,
                            manager_with_prediction);
  CallPreProcessAudioBuffer(kClippedWaitFrames, kClippingPeakRatio,
                            manager_without_prediction);
  EXPECT_EQ(manager_with_prediction.recommended_analog_level(),
            kInitialLevel - 3 * kClippedLevelStep);
  EXPECT_EQ(manager_without_prediction.recommended_analog_level(),
            kInitialLevel - kClippedLevelStep);

  // Expect a change for clipping frames.
  CallPreProcessAudioBuffer(/*num_calls=*/1, kClippingPeakRatio,
                            manager_with_prediction);
  CallPreProcessAudioBuffer(/*num_calls=*/1, kClippingPeakRatio,
                            manager_without_prediction);
  EXPECT_EQ(manager_with_prediction.recommended_analog_level(),
            kInitialLevel - 4 * kClippedLevelStep);
  EXPECT_EQ(manager_without_prediction.recommended_analog_level(),
            kInitialLevel - 2 * kClippedLevelStep);
}

// Checks that passing an empty speech level has no effect on the input volume.
TEST_P(InputVolumeControllerParametrizedTest, EmptyRmsErrorHasNoEffect) {
  InputVolumeController manager(kNumChannels,
                                GetInputVolumeControllerTestConfig());
  manager.Initialize();

  constexpr int kInputVolume = kInitialInputVolume;
  manager.set_stream_analog_level(kInputVolume);

  // Feed speech with low energy that would trigger an upward adapation of
  // the analog level if an speech level was not low and the RMS level empty.
  constexpr int kNumFrames = 125;
  constexpr int kGainDb = -20;
  SpeechSamplesReader reader;
  reader.Feed(kNumFrames, kGainDb, kLowSpeechProbability, absl::nullopt,
              manager);

  // Check that no adaptation occurs.
  ASSERT_EQ(manager.recommended_analog_level(), kInputVolume);
}

// Checks that the recommended input volume is not updated unless enough
// frames have been processed after the previous update.
TEST(InputVolumeControllerTest, UpdateInputVolumeWaitFramesIsEffective) {
  constexpr int kInputVolume = kInitialInputVolume;
  std::unique_ptr<InputVolumeController> controller_wait_0 =
      CreateInputVolumeController(kClippedLevelStep, kClippedRatioThreshold,
                                  kClippedWaitFrames,
                                  /*enable_clipping_predictor=*/false,
                                  /*update_input_volume_wait_frames=*/0);
  std::unique_ptr<InputVolumeController> controller_wait_100 =
      CreateInputVolumeController(kClippedLevelStep, kClippedRatioThreshold,
                                  kClippedWaitFrames,
                                  /*enable_clipping_predictor=*/false,
                                  /*update_input_volume_wait_frames=*/100);
  controller_wait_0->Initialize();
  controller_wait_100->Initialize();
  controller_wait_0->set_stream_analog_level(kInputVolume);
  controller_wait_100->set_stream_analog_level(kInputVolume);

  SpeechSamplesReader reader_1;
  SpeechSamplesReader reader_2;
  reader_1.Feed(/*num_frames=*/99, /*gain_db=*/0, kHighSpeechProbability,
                /*speech_level=*/-42.0f, *controller_wait_0);
  reader_2.Feed(/*num_frames=*/99, /*gain_db=*/0, kHighSpeechProbability,
                /*speech_level=*/-42.0f, *controller_wait_100);

  // Check that adaptation only occurs if enough frames have been processed.
  ASSERT_GT(controller_wait_0->recommended_analog_level(), kInputVolume);
  ASSERT_EQ(controller_wait_100->recommended_analog_level(), kInputVolume);

  reader_1.Feed(/*num_frames=*/1, /*gain_db=*/0, kHighSpeechProbability,
                /*speech_level=*/-42.0f, *controller_wait_0);
  reader_2.Feed(/*num_frames=*/1, /*gain_db=*/0, kHighSpeechProbability,
                /*speech_level=*/-42.0f, *controller_wait_100);

  // Check that adaptation only occurs when enough frames have been processed.
  ASSERT_GT(controller_wait_0->recommended_analog_level(), kInputVolume);
  ASSERT_GT(controller_wait_100->recommended_analog_level(), kInputVolume);
}

TEST(InputVolumeControllerTest, SpeechRatioThresholdIsEffective) {
  constexpr int kInputVolume = kInitialInputVolume;
  // Create two input volume controllers with 10 frames between volume updates
  // and the minimum speech ratio of 0.8 and speech probability threshold 0.5.
  std::unique_ptr<InputVolumeController> controller_1 =
      CreateInputVolumeController(kClippedLevelStep, kClippedRatioThreshold,
                                  kClippedWaitFrames,
                                  /*enable_clipping_predictor=*/false,
                                  /*update_input_volume_wait_frames=*/10);
  std::unique_ptr<InputVolumeController> controller_2 =
      CreateInputVolumeController(kClippedLevelStep, kClippedRatioThreshold,
                                  kClippedWaitFrames,
                                  /*enable_clipping_predictor=*/false,
                                  /*update_input_volume_wait_frames=*/10);
  controller_1->Initialize();
  controller_2->Initialize();
  controller_1->set_stream_analog_level(kInputVolume);
  controller_2->set_stream_analog_level(kInputVolume);

  SpeechSamplesReader reader_1;
  SpeechSamplesReader reader_2;

  reader_1.Feed(/*num_frames=*/1, /*gain_db=*/0,
                /*speech_probability=*/0.7f, /*speech_level=*/-42.0f,
                *controller_1);
  reader_2.Feed(/*num_frames=*/1, /*gain_db=*/0,
                /*speech_probability=*/0.4f, /*speech_level=*/-42.0f,
                *controller_2);

  ASSERT_EQ(controller_1->recommended_analog_level(), kInputVolume);
  ASSERT_EQ(controller_2->recommended_analog_level(), kInputVolume);

  reader_1.Feed(/*num_frames=*/2, /*gain_db=*/0,
                /*speech_probability=*/0.4f, /*speech_level=*/-42.0f,
                *controller_1);
  reader_2.Feed(/*num_frames=*/2, /*gain_db=*/0,
                /*speech_probability=*/0.4f, /*speech_level=*/-42.0f,
                *controller_2);

  ASSERT_EQ(controller_1->recommended_analog_level(), kInputVolume);
  ASSERT_EQ(controller_2->recommended_analog_level(), kInputVolume);

  reader_1.Feed(/*num_frames=*/7, /*gain_db=*/0,
                /*speech_probability=*/0.7f, /*speech_level=*/-42.0f,
                *controller_1);
  reader_2.Feed(/*num_frames=*/7, /*gain_db=*/0,
                /*speech_probability=*/0.7f, /*speech_level=*/-42.0f,
                *controller_2);

  ASSERT_GT(controller_1->recommended_analog_level(), kInputVolume);
  ASSERT_EQ(controller_2->recommended_analog_level(), kInputVolume);
}

TEST(InputVolumeControllerTest, SpeechProbabilityThresholdIsEffective) {
  constexpr int kInputVolume = kInitialInputVolume;
  // Create two input volume controllers with the exact same settings and
  // 10 frames between volume updates.
  std::unique_ptr<InputVolumeController> controller_1 =
      CreateInputVolumeController(kClippedLevelStep, kClippedRatioThreshold,
                                  kClippedWaitFrames,
                                  /*enable_clipping_predictor=*/false,
                                  /*update_input_volume_wait_frames=*/10);
  std::unique_ptr<InputVolumeController> controller_2 =
      CreateInputVolumeController(kClippedLevelStep, kClippedRatioThreshold,
                                  kClippedWaitFrames,
                                  /*enable_clipping_predictor=*/false,
                                  /*update_input_volume_wait_frames=*/10);
  controller_1->Initialize();
  controller_2->Initialize();
  controller_1->set_stream_analog_level(kInputVolume);
  controller_2->set_stream_analog_level(kInputVolume);

  SpeechSamplesReader reader_1;
  SpeechSamplesReader reader_2;

  // Process with two sets of inputs: Use `reader_1` to process inputs
  // that make the volume to be adjusted after enough frames have been
  // processsed and `reader_2` to process inputs that won't make the volume
  // to be adjusted.
  reader_1.Feed(/*num_frames=*/1, /*gain_db=*/0,
                /*speech_probability=*/0.5f, /*speech_level=*/-42.0f,
                *controller_1);
  reader_2.Feed(/*num_frames=*/1, /*gain_db=*/0,
                /*speech_probability=*/0.49f, /*speech_level=*/-42.0f,
                *controller_2);

  ASSERT_EQ(controller_1->recommended_analog_level(), kInputVolume);
  ASSERT_EQ(controller_2->recommended_analog_level(), kInputVolume);

  reader_1.Feed(/*num_frames=*/2, /*gain_db=*/0,
                /*speech_probability=*/0.49f, /*speech_level=*/-42.0f,
                *controller_1);
  reader_2.Feed(/*num_frames=*/2, /*gain_db=*/0,
                /*speech_probability=*/0.49f, /*speech_level=*/-42.0f,
                *controller_2);

  ASSERT_EQ(controller_1->recommended_analog_level(), kInputVolume);
  ASSERT_EQ(controller_2->recommended_analog_level(), kInputVolume);

  reader_1.Feed(/*num_frames=*/7, /*gain_db=*/0,
                /*speech_probability=*/0.5f, /*speech_level=*/-42.0f,
                *controller_1);
  reader_2.Feed(/*num_frames=*/7, /*gain_db=*/0,
                /*speech_probability=*/0.5f, /*speech_level=*/-42.0f,
                *controller_2);

  ASSERT_GT(controller_1->recommended_analog_level(), kInputVolume);
  ASSERT_EQ(controller_2->recommended_analog_level(), kInputVolume);
}

TEST(MonoInputVolumeControllerTest, CheckHandleClippingLowersVolume) {
  constexpr int kInitialInputVolume = 100;
  constexpr int kInputVolumeStep = 29;
  MonoInputVolumeController mono_controller(
      /*clipped_level_min=*/70,
      /*min_mic_level=*/32,
      /*update_input_volume_wait_frames=*/3, kHighSpeechProbability,
      kSpeechRatioThreshold);
  mono_controller.Initialize();

  UpdateRecommendedInputVolume(mono_controller, kInitialInputVolume,
                               kLowSpeechProbability,
                               /*rms_error_dbfs*/ -10.0f);

  mono_controller.HandleClipping(kInputVolumeStep);

  EXPECT_EQ(mono_controller.recommended_analog_level(),
            kInitialInputVolume - kInputVolumeStep);
}

TEST(MonoInputVolumeControllerTest,
     CheckProcessNegativeRmsErrorDecreasesInputVolume) {
  constexpr int kInitialInputVolume = 100;
  MonoInputVolumeController mono_controller(
      /*clipped_level_min=*/64,
      /*min_mic_level=*/32,
      /*update_input_volume_wait_frames=*/3, kHighSpeechProbability,
      kSpeechRatioThreshold);
  mono_controller.Initialize();

  int volume = UpdateRecommendedInputVolume(
      mono_controller, kInitialInputVolume, kHighSpeechProbability, -10.0f);
  volume = UpdateRecommendedInputVolume(mono_controller, volume,
                                        kHighSpeechProbability, -10.0f);
  volume = UpdateRecommendedInputVolume(mono_controller, volume,
                                        kHighSpeechProbability, -10.0f);

  EXPECT_LT(volume, kInitialInputVolume);
}

TEST(MonoInputVolumeControllerTest,
     CheckProcessPositiveRmsErrorIncreasesInputVolume) {
  constexpr int kInitialInputVolume = 100;
  MonoInputVolumeController mono_controller(
      /*clipped_level_min=*/64,
      /*min_mic_level=*/32,
      /*update_input_volume_wait_frames=*/3, kHighSpeechProbability,
      kSpeechRatioThreshold);
  mono_controller.Initialize();

  int volume = UpdateRecommendedInputVolume(
      mono_controller, kInitialInputVolume, kHighSpeechProbability, 10.0f);
  volume = UpdateRecommendedInputVolume(mono_controller, volume,
                                        kHighSpeechProbability, 10.0f);
  volume = UpdateRecommendedInputVolume(mono_controller, volume,
                                        kHighSpeechProbability, 10.0f);

  EXPECT_GT(volume, kInitialInputVolume);
}

TEST(MonoInputVolumeControllerTest,
     CheckProcessNegativeRmsErrorDecreasesInputVolumeWithLimit) {
  constexpr int kInitialInputVolume = 100;
  MonoInputVolumeController mono_controller_1(
      /*clipped_level_min=*/64,
      /*min_mic_level=*/32,
      /*update_input_volume_wait_frames=*/2, kHighSpeechProbability,
      kSpeechRatioThreshold);
  MonoInputVolumeController mono_controller_2(
      /*clipped_level_min=*/64,
      /*min_mic_level=*/32,
      /*update_input_volume_wait_frames=*/2, kHighSpeechProbability,
      kSpeechRatioThreshold);
  MonoInputVolumeController mono_controller_3(
      /*clipped_level_min=*/64,
      /*min_mic_level=*/32,
      /*update_input_volume_wait_frames=*/2,
      /*speech_probability_threshold=*/0.7,
      /*speech_ratio_threshold=*/0.8);
  mono_controller_1.Initialize();
  mono_controller_2.Initialize();
  mono_controller_3.Initialize();

  // Process RMS errors in the range
  // [`-kMaxResidualGainChange`, `kMaxResidualGainChange`].
  int volume_1 = UpdateRecommendedInputVolume(
      mono_controller_1, kInitialInputVolume, kHighSpeechProbability, -14.0f);
  volume_1 = UpdateRecommendedInputVolume(mono_controller_1, volume_1,
                                          kHighSpeechProbability, -14.0f);
  // Process RMS errors outside the range
  // [`-kMaxResidualGainChange`, `kMaxResidualGainChange`].
  int volume_2 = UpdateRecommendedInputVolume(
      mono_controller_2, kInitialInputVolume, kHighSpeechProbability, -15.0f);
  int volume_3 = UpdateRecommendedInputVolume(
      mono_controller_3, kInitialInputVolume, kHighSpeechProbability, -30.0f);
  volume_2 = UpdateRecommendedInputVolume(mono_controller_2, volume_2,
                                          kHighSpeechProbability, -15.0f);
  volume_3 = UpdateRecommendedInputVolume(mono_controller_3, volume_3,
                                          kHighSpeechProbability, -30.0f);

  EXPECT_LT(volume_1, kInitialInputVolume);
  EXPECT_LT(volume_2, volume_1);
  EXPECT_EQ(volume_2, volume_3);
}

TEST(MonoInputVolumeControllerTest,
     CheckProcessPositiveRmsErrorIncreasesInputVolumeWithLimit) {
  constexpr int kInitialInputVolume = 100;
  MonoInputVolumeController mono_controller_1(
      /*clipped_level_min=*/64,
      /*min_mic_level=*/32,
      /*update_input_volume_wait_frames=*/2, kHighSpeechProbability,
      kSpeechRatioThreshold);
  MonoInputVolumeController mono_controller_2(
      /*clipped_level_min=*/64,
      /*min_mic_level=*/32,
      /*update_input_volume_wait_frames=*/2, kHighSpeechProbability,
      kSpeechRatioThreshold);
  MonoInputVolumeController mono_controller_3(
      /*clipped_level_min=*/64,
      /*min_mic_level=*/32,
      /*update_input_volume_wait_frames=*/2, kHighSpeechProbability,
      kSpeechRatioThreshold);
  mono_controller_1.Initialize();
  mono_controller_2.Initialize();
  mono_controller_3.Initialize();

  // Process RMS errors in the range
  // [`-kMaxResidualGainChange`, `kMaxResidualGainChange`].
  int volume_1 = UpdateRecommendedInputVolume(
      mono_controller_1, kInitialInputVolume, kHighSpeechProbability, 14.0f);
  volume_1 = UpdateRecommendedInputVolume(mono_controller_1, volume_1,
                                          kHighSpeechProbability, 14.0f);
  // Process RMS errors outside the range
  // [`-kMaxResidualGainChange`, `kMaxResidualGainChange`].
  int volume_2 = UpdateRecommendedInputVolume(
      mono_controller_2, kInitialInputVolume, kHighSpeechProbability, 15.0f);
  int volume_3 = UpdateRecommendedInputVolume(
      mono_controller_3, kInitialInputVolume, kHighSpeechProbability, 30.0f);
  volume_2 = UpdateRecommendedInputVolume(mono_controller_2, volume_2,
                                          kHighSpeechProbability, 15.0f);
  volume_3 = UpdateRecommendedInputVolume(mono_controller_3, volume_3,
                                          kHighSpeechProbability, 30.0f);

  EXPECT_GT(volume_1, kInitialInputVolume);
  EXPECT_GT(volume_2, volume_1);
  EXPECT_EQ(volume_2, volume_3);
}

TEST(MonoInputVolumeControllerTest,
     CheckProcessRmsErrorDecreasesInputVolumeRepeatedly) {
  constexpr int kInitialInputVolume = 100;
  MonoInputVolumeController mono_controller(
      /*clipped_level_min=*/64,
      /*min_mic_level=*/32,
      /*update_input_volume_wait_frames=*/2, kHighSpeechProbability,
      kSpeechRatioThreshold);
  mono_controller.Initialize();

  int volume_before = UpdateRecommendedInputVolume(
      mono_controller, kInitialInputVolume, kHighSpeechProbability, -10.0f);
  volume_before = UpdateRecommendedInputVolume(mono_controller, volume_before,
                                               kHighSpeechProbability, -10.0f);

  EXPECT_LT(volume_before, kInitialInputVolume);

  int volume_after = UpdateRecommendedInputVolume(
      mono_controller, volume_before, kHighSpeechProbability, -10.0f);
  volume_after = UpdateRecommendedInputVolume(mono_controller, volume_after,
                                              kHighSpeechProbability, -10.0f);

  EXPECT_LT(volume_after, volume_before);
}

TEST(MonoInputVolumeControllerTest,
     CheckProcessPositiveRmsErrorIncreasesInputVolumeRepeatedly) {
  constexpr int kInitialInputVolume = 100;
  MonoInputVolumeController mono_controller(
      /*clipped_level_min=*/64,
      /*min_mic_level=*/32,
      /*update_input_volume_wait_frames=*/2, kHighSpeechProbability,
      kSpeechRatioThreshold);
  mono_controller.Initialize();

  int volume_before = UpdateRecommendedInputVolume(
      mono_controller, kInitialInputVolume, kHighSpeechProbability, 10.0f);
  volume_before = UpdateRecommendedInputVolume(mono_controller, volume_before,
                                               kHighSpeechProbability, 10.0f);

  EXPECT_GT(volume_before, kInitialInputVolume);

  int volume_after = UpdateRecommendedInputVolume(
      mono_controller, volume_before, kHighSpeechProbability, 10.0f);
  volume_after = UpdateRecommendedInputVolume(mono_controller, volume_after,
                                              kHighSpeechProbability, 10.0f);

  EXPECT_GT(volume_after, volume_before);
}

TEST(MonoInputVolumeControllerTest, CheckClippedLevelMinIsEffective) {
  constexpr int kInitialInputVolume = 100;
  constexpr int kClippedLevelMin = 70;
  MonoInputVolumeController mono_controller_1(
      kClippedLevelMin,
      /*min_mic_level=*/84,
      /*update_input_volume_wait_frames=*/2, kHighSpeechProbability,
      kSpeechRatioThreshold);
  MonoInputVolumeController mono_controller_2(
      kClippedLevelMin,
      /*min_mic_level=*/84,
      /*update_input_volume_wait_frames=*/2, kHighSpeechProbability,
      kSpeechRatioThreshold);
  mono_controller_1.Initialize();
  mono_controller_2.Initialize();

  // Process one frame to reset the state for `HandleClipping()`.
  EXPECT_EQ(UpdateRecommendedInputVolume(mono_controller_1, kInitialInputVolume,
                                         kLowSpeechProbability, -10.0f),
            kInitialInputVolume);
  EXPECT_EQ(UpdateRecommendedInputVolume(mono_controller_2, kInitialInputVolume,
                                         kLowSpeechProbability, -10.0f),
            kInitialInputVolume);

  mono_controller_1.HandleClipping(29);
  mono_controller_2.HandleClipping(31);

  EXPECT_EQ(mono_controller_2.recommended_analog_level(), kClippedLevelMin);
  EXPECT_LT(mono_controller_2.recommended_analog_level(),
            mono_controller_1.recommended_analog_level());
}

TEST(MonoInputVolumeControllerTest, CheckMinMicLevelIsEffective) {
  constexpr int kInitialInputVolume = 100;
  constexpr int kMinMicLevel = 64;
  MonoInputVolumeController mono_controller_1(
      /*clipped_level_min=*/64, kMinMicLevel,
      /*update_input_volume_wait_frames=*/2, kHighSpeechProbability,
      kSpeechRatioThreshold);
  MonoInputVolumeController mono_controller_2(
      /*clipped_level_min=*/64, kMinMicLevel,
      /*update_input_volume_wait_frames=*/2, kHighSpeechProbability,
      kSpeechRatioThreshold);
  mono_controller_1.Initialize();
  mono_controller_2.Initialize();

  int volume_1 = UpdateRecommendedInputVolume(
      mono_controller_1, kInitialInputVolume, kHighSpeechProbability, -10.0f);
  int volume_2 = UpdateRecommendedInputVolume(
      mono_controller_2, kInitialInputVolume, kHighSpeechProbability, -10.0f);

  EXPECT_EQ(volume_1, kInitialInputVolume);
  EXPECT_EQ(volume_2, kInitialInputVolume);

  volume_1 = UpdateRecommendedInputVolume(mono_controller_1, volume_1,
                                          kHighSpeechProbability, -10.0f);
  volume_2 = UpdateRecommendedInputVolume(mono_controller_2, volume_2,
                                          kHighSpeechProbability, -30.0f);

  EXPECT_LT(volume_1, kInitialInputVolume);
  EXPECT_LT(volume_2, volume_1);
  EXPECT_EQ(volume_2, kMinMicLevel);
}

TEST(MonoInputVolumeControllerTest,
     CheckUpdateInputVolumeWaitFramesIsEffective) {
  constexpr int kInitialInputVolume = 100;
  MonoInputVolumeController mono_controller_1(
      /*clipped_level_min=*/64,
      /*min_mic_level=*/84,
      /*update_input_volume_wait_frames=*/1, kHighSpeechProbability,
      kSpeechRatioThreshold);
  MonoInputVolumeController mono_controller_2(
      /*clipped_level_min=*/64,
      /*min_mic_level=*/84,
      /*update_input_volume_wait_frames=*/3, kHighSpeechProbability,
      kSpeechRatioThreshold);
  mono_controller_1.Initialize();
  mono_controller_2.Initialize();

  int volume_1 = UpdateRecommendedInputVolume(
      mono_controller_1, kInitialInputVolume, kHighSpeechProbability, -10.0f);
  int volume_2 = UpdateRecommendedInputVolume(
      mono_controller_2, kInitialInputVolume, kHighSpeechProbability, -10.0f);

  EXPECT_EQ(volume_1, kInitialInputVolume);
  EXPECT_EQ(volume_2, kInitialInputVolume);

  volume_1 = UpdateRecommendedInputVolume(mono_controller_1, volume_1,
                                          kHighSpeechProbability, -10.0f);
  volume_2 = UpdateRecommendedInputVolume(mono_controller_2, volume_2,
                                          kHighSpeechProbability, -10.0f);

  EXPECT_LT(volume_1, kInitialInputVolume);
  EXPECT_EQ(volume_2, kInitialInputVolume);

  volume_2 = UpdateRecommendedInputVolume(mono_controller_2, volume_2,
                                          kHighSpeechProbability, -10.0f);

  EXPECT_LT(volume_2, kInitialInputVolume);
}

TEST(MonoInputVolumeControllerTest,
     CheckSpeechProbabilityThresholdIsEffective) {
  constexpr int kInitialInputVolume = 100;
  constexpr float kSpeechProbabilityThreshold = 0.8f;
  MonoInputVolumeController mono_controller_1(
      /*clipped_level_min=*/64,
      /*min_mic_level=*/84,
      /*update_input_volume_wait_frames=*/2, kSpeechProbabilityThreshold,
      kSpeechRatioThreshold);
  MonoInputVolumeController mono_controller_2(
      /*clipped_level_min=*/64,
      /*min_mic_level=*/84,
      /*update_input_volume_wait_frames=*/2, kSpeechProbabilityThreshold,
      kSpeechRatioThreshold);
  mono_controller_1.Initialize();
  mono_controller_2.Initialize();

  int volume_1 =
      UpdateRecommendedInputVolume(mono_controller_1, kInitialInputVolume,
                                   kSpeechProbabilityThreshold, -10.0f);
  int volume_2 =
      UpdateRecommendedInputVolume(mono_controller_2, kInitialInputVolume,
                                   kSpeechProbabilityThreshold, -10.0f);

  EXPECT_EQ(volume_1, kInitialInputVolume);
  EXPECT_EQ(volume_2, kInitialInputVolume);

  volume_1 = UpdateRecommendedInputVolume(
      mono_controller_1, volume_1, kSpeechProbabilityThreshold - 0.1f, -10.0f);
  volume_2 = UpdateRecommendedInputVolume(mono_controller_2, volume_2,
                                          kSpeechProbabilityThreshold, -10.0f);

  EXPECT_EQ(volume_1, kInitialInputVolume);
  EXPECT_LT(volume_2, volume_1);
}

TEST(MonoInputVolumeControllerTest, CheckSpeechRatioThresholdIsEffective) {
  constexpr int kInitialInputVolume = 100;
  MonoInputVolumeController mono_controller_1(
      /*clipped_level_min=*/64,
      /*min_mic_level=*/84,
      /*update_input_volume_wait_frames=*/4, kHighSpeechProbability,
      /*speech_ratio_threshold=*/0.75f);
  MonoInputVolumeController mono_controller_2(
      /*clipped_level_min=*/64,
      /*min_mic_level=*/84,
      /*update_input_volume_wait_frames=*/4, kHighSpeechProbability,
      /*speech_ratio_threshold=*/0.75f);
  mono_controller_1.Initialize();
  mono_controller_2.Initialize();

  int volume_1 = UpdateRecommendedInputVolume(
      mono_controller_1, kInitialInputVolume, kHighSpeechProbability, -10.0f);
  int volume_2 = UpdateRecommendedInputVolume(
      mono_controller_2, kInitialInputVolume, kHighSpeechProbability, -10.0f);

  volume_1 = UpdateRecommendedInputVolume(mono_controller_1, volume_1,
                                          kHighSpeechProbability, -10.0f);
  volume_2 = UpdateRecommendedInputVolume(mono_controller_2, volume_2,
                                          kHighSpeechProbability, -10.0f);

  volume_1 = UpdateRecommendedInputVolume(mono_controller_1, volume_1,
                                          kLowSpeechProbability, -10.0f);
  volume_2 = UpdateRecommendedInputVolume(mono_controller_2, volume_2,
                                          kLowSpeechProbability, -10.0f);

  EXPECT_EQ(volume_1, kInitialInputVolume);
  EXPECT_EQ(volume_2, kInitialInputVolume);

  volume_1 = UpdateRecommendedInputVolume(mono_controller_1, volume_1,
                                          kLowSpeechProbability, -10.0f);
  volume_2 = UpdateRecommendedInputVolume(mono_controller_2, volume_2,
                                          kHighSpeechProbability, -10.0f);

  EXPECT_EQ(volume_1, kInitialInputVolume);
  EXPECT_LT(volume_2, volume_1);
}

TEST(MonoInputVolumeControllerTest,
     CheckProcessEmptyRmsErrorDoesNotLowerVolume) {
  constexpr int kInitialInputVolume = 100;
  MonoInputVolumeController mono_controller_1(
      /*clipped_level_min=*/64,
      /*min_mic_level=*/84,
      /*update_input_volume_wait_frames=*/2, kHighSpeechProbability,
      kSpeechRatioThreshold);
  MonoInputVolumeController mono_controller_2(
      /*clipped_level_min=*/64,
      /*min_mic_level=*/84,
      /*update_input_volume_wait_frames=*/2, kHighSpeechProbability,
      kSpeechRatioThreshold);
  mono_controller_1.Initialize();
  mono_controller_2.Initialize();

  int volume_1 = UpdateRecommendedInputVolume(
      mono_controller_1, kInitialInputVolume, kHighSpeechProbability, -10.0f);
  int volume_2 = UpdateRecommendedInputVolume(
      mono_controller_2, kInitialInputVolume, kHighSpeechProbability, -10.0f);

  EXPECT_EQ(volume_1, kInitialInputVolume);
  EXPECT_EQ(volume_2, kInitialInputVolume);

  volume_1 = UpdateRecommendedInputVolume(
      mono_controller_1, volume_1, kHighSpeechProbability, absl::nullopt);
  volume_2 = UpdateRecommendedInputVolume(mono_controller_2, volume_2,
                                          kHighSpeechProbability, -10.0f);

  EXPECT_EQ(volume_1, kInitialInputVolume);
  EXPECT_LT(volume_2, volume_1);
}

}  // namespace webrtc
