/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_INCLUDE_AUDIO_PROCESSING_H_
#define MODULES_AUDIO_PROCESSING_INCLUDE_AUDIO_PROCESSING_H_

// MSVC++ requires this to be set before any other includes to get M_PI.
// MOZILLA: this is already defined in mozilla-config.h
// #define _USE_MATH_DEFINES

#include <math.h>
#include <stddef.h>  // size_t
#include <stdio.h>  // FILE
#include <string.h>
#include <vector>

#include "api/optional.h"
#include "modules/audio_processing/beamformer/array_util.h"
#include "modules/audio_processing/include/audio_processing_statistics.h"
#include "modules/audio_processing/include/config.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/deprecation.h"
#include "rtc_base/platform_file.h"
#include "rtc_base/refcount.h"
#include "typedefs.h"  // NOLINT(build/include)

namespace webrtc {

struct AecCore;

class AecDump;
class AudioBuffer;
class AudioFrame;

class NonlinearBeamformer;

class StreamConfig;
class ProcessingConfig;

class EchoCancellation;
class EchoControlMobile;
class EchoControlFactory;
class GainControl;
class HighPassFilter;
class LevelEstimator;
class NoiseSuppression;
class PostProcessing;
class VoiceDetection;

// Use to enable the extended filter mode in the AEC, along with robustness
// measures around the reported system delays. It comes with a significant
// increase in AEC complexity, but is much more robust to unreliable reported
// delays.
//
// Detailed changes to the algorithm:
// - The filter length is changed from 48 to 128 ms. This comes with tuning of
//   several parameters: i) filter adaptation stepsize and error threshold;
//   ii) non-linear processing smoothing and overdrive.
// - Option to ignore the reported delays on platforms which we deem
//   sufficiently unreliable. See WEBRTC_UNTRUSTED_DELAY in echo_cancellation.c.
// - Faster startup times by removing the excessive "startup phase" processing
//   of reported delays.
// - Much more conservative adjustments to the far-end read pointer. We smooth
//   the delay difference more heavily, and back off from the difference more.
//   Adjustments force a readaptation of the filter, so they should be avoided
//   except when really necessary.
struct ExtendedFilter {
  ExtendedFilter() : enabled(false) {}
  explicit ExtendedFilter(bool enabled) : enabled(enabled) {}
  static const ConfigOptionID identifier = ConfigOptionID::kExtendedFilter;
  bool enabled;
};

// Enables the refined linear filter adaptation in the echo canceller.
// This configuration only applies to EchoCancellation and not
// EchoControlMobile. It can be set in the constructor
// or using AudioProcessing::SetExtraOptions().
struct RefinedAdaptiveFilter {
  RefinedAdaptiveFilter() : enabled(false) {}
  explicit RefinedAdaptiveFilter(bool enabled) : enabled(enabled) {}
  static const ConfigOptionID identifier =
      ConfigOptionID::kAecRefinedAdaptiveFilter;
  bool enabled;
};

// Enables delay-agnostic echo cancellation. This feature relies on internally
// estimated delays between the process and reverse streams, thus not relying
// on reported system delays. This configuration only applies to
// EchoCancellation and not EchoControlMobile. It can be set in the constructor
// or using AudioProcessing::SetExtraOptions().
struct DelayAgnostic {
  DelayAgnostic() : enabled(false) {}
  explicit DelayAgnostic(bool enabled) : enabled(enabled) {}
  static const ConfigOptionID identifier = ConfigOptionID::kDelayAgnostic;
  bool enabled;
};

// Use to enable experimental gain control (AGC). At startup the experimental
// AGC moves the microphone volume up to |startup_min_volume| if the current
// microphone volume is set too low. The value is clamped to its operating range
// [12, 255]. Here, 255 maps to 100%.
//
// Must be provided through AudioProcessing::Create(Confg&).
#if defined(WEBRTC_CHROMIUM_BUILD)
static const int kAgcStartupMinVolume = 85;
#else
static const int kAgcStartupMinVolume = 0;
#endif  // defined(WEBRTC_CHROMIUM_BUILD)
static constexpr int kClippedLevelMin = 70;
struct ExperimentalAgc {
  ExperimentalAgc() = default;
  explicit ExperimentalAgc(bool enabled) : enabled(enabled) {}
  ExperimentalAgc(bool enabled, int startup_min_volume)
      : enabled(enabled), startup_min_volume(startup_min_volume) {}
  ExperimentalAgc(bool enabled, int startup_min_volume, int clipped_level_min)
      : enabled(enabled),
        startup_min_volume(startup_min_volume),
        clipped_level_min(clipped_level_min) {}
  static const ConfigOptionID identifier = ConfigOptionID::kExperimentalAgc;
  bool enabled = true;
  int startup_min_volume = kAgcStartupMinVolume;
  // Lowest microphone level that will be applied in response to clipping.
  int clipped_level_min = kClippedLevelMin;
};

// Use to enable experimental noise suppression. It can be set in the
// constructor or using AudioProcessing::SetExtraOptions().
struct ExperimentalNs {
  ExperimentalNs() : enabled(false) {}
  explicit ExperimentalNs(bool enabled) : enabled(enabled) {}
  static const ConfigOptionID identifier = ConfigOptionID::kExperimentalNs;
  bool enabled;
};

// Use to enable beamforming. Must be provided through the constructor. It will
// have no impact if used with AudioProcessing::SetExtraOptions().
struct Beamforming {
  Beamforming();
  Beamforming(bool enabled, const std::vector<Point>& array_geometry);
  Beamforming(bool enabled,
              const std::vector<Point>& array_geometry,
              SphericalPointf target_direction);
  ~Beamforming();

  static const ConfigOptionID identifier = ConfigOptionID::kBeamforming;
  const bool enabled;
  const std::vector<Point> array_geometry;
  const SphericalPointf target_direction;
};

// Use to enable intelligibility enhancer in audio processing.
//
// Note: If enabled and the reverse stream has more than one output channel,
// the reverse stream will become an upmixed mono signal.
struct Intelligibility {
  Intelligibility() : enabled(false) {}
  explicit Intelligibility(bool enabled) : enabled(enabled) {}
  static const ConfigOptionID identifier = ConfigOptionID::kIntelligibility;
  bool enabled;
};

// The Audio Processing Module (APM) provides a collection of voice processing
// components designed for real-time communications software.
//
// APM operates on two audio streams on a frame-by-frame basis. Frames of the
// primary stream, on which all processing is applied, are passed to
// |ProcessStream()|. Frames of the reverse direction stream are passed to
// |ProcessReverseStream()|. On the client-side, this will typically be the
// near-end (capture) and far-end (render) streams, respectively. APM should be
// placed in the signal chain as close to the audio hardware abstraction layer
// (HAL) as possible.
//
// On the server-side, the reverse stream will normally not be used, with
// processing occurring on each incoming stream.
//
// Component interfaces follow a similar pattern and are accessed through
// corresponding getters in APM. All components are disabled at create-time,
// with default settings that are recommended for most situations. New settings
// can be applied without enabling a component. Enabling a component triggers
// memory allocation and initialization to allow it to start processing the
// streams.
//
// Thread safety is provided with the following assumptions to reduce locking
// overhead:
//   1. The stream getters and setters are called from the same thread as
//      ProcessStream(). More precisely, stream functions are never called
//      concurrently with ProcessStream().
//   2. Parameter getters are never called concurrently with the corresponding
//      setter.
//
// APM accepts only linear PCM audio data in chunks of 10 ms. The int16
// interfaces use interleaved data, while the float interfaces use deinterleaved
// data.
//
// Usage example, omitting error checking:
// AudioProcessing* apm = AudioProcessing::Create(0);
//
// AudioProcessing::Config config;
// config.level_controller.enabled = true;
// config.high_pass_filter.enabled = true;
// apm->ApplyConfig(config)
//
// apm->echo_cancellation()->enable_drift_compensation(false);
// apm->echo_cancellation()->Enable(true);
//
// apm->noise_reduction()->set_level(kHighSuppression);
// apm->noise_reduction()->Enable(true);
//
// apm->gain_control()->set_analog_level_limits(0, 255);
// apm->gain_control()->set_mode(kAdaptiveAnalog);
// apm->gain_control()->Enable(true);
//
// apm->voice_detection()->Enable(true);
//
// // Start a voice call...
//
// // ... Render frame arrives bound for the audio HAL ...
// apm->ProcessReverseStream(render_frame);
//
// // ... Capture frame arrives from the audio HAL ...
// // Call required set_stream_ functions.
// apm->set_stream_delay_ms(delay_ms);
// apm->gain_control()->set_stream_analog_level(analog_level);
//
// apm->ProcessStream(capture_frame);
//
// // Call required stream_ functions.
// analog_level = apm->gain_control()->stream_analog_level();
// has_voice = apm->stream_has_voice();
//
// // Repeate render and capture processing for the duration of the call...
// // Start a new call...
// apm->Initialize();
//
// // Close the application...
// delete apm;
//
class AudioProcessing : public rtc::RefCountInterface {
 public:
  // The struct below constitutes the new parameter scheme for the audio
  // processing. It is being introduced gradually and until it is fully
  // introduced, it is prone to change.
  // TODO(peah): Remove this comment once the new config scheme is fully rolled
  // out.
  //
  // The parameters and behavior of the audio processing module are controlled
  // by changing the default values in the AudioProcessing::Config struct.
  // The config is applied by passing the struct to the ApplyConfig method.
  struct Config {
    struct LevelController {
      bool enabled = false;

      // Sets the initial peak level to use inside the level controller in order
      // to compute the signal gain. The unit for the peak level is dBFS and
      // the allowed range is [-100, 0].
      float initial_peak_level_dbfs = -6.0206f;
    } level_controller;
    struct ResidualEchoDetector {
      bool enabled = true;
    } residual_echo_detector;

    struct HighPassFilter {
      bool enabled = false;
    } high_pass_filter;

    // Deprecated way of activating AEC3.
    // TODO(gustaf): Remove when possible.
    struct EchoCanceller3 {
      bool enabled = false;
    } echo_canceller3;

    // Enables the next generation AGC functionality. This feature replaces the
    // standard methods of gain control in the previous AGC.
    // The functionality is not yet activated in the code and turning this on
    // does not yet have the desired behavior.
    struct GainController2 {
      bool enabled = false;
      float fixed_gain_db = 0.f;
    } gain_controller2;

    // Explicit copy assignment implementation to avoid issues with memory
    // sanitizer complaints in case of self-assignment.
    // TODO(peah): Add buildflag to ensure that this is only included for memory
    // sanitizer builds.
    Config& operator=(const Config& config) {
      if (this != &config) {
        memcpy(this, &config, sizeof(*this));
      }
      return *this;
    }
  };

  // TODO(mgraczyk): Remove once all methods that use ChannelLayout are gone.
  enum ChannelLayout {
    kMono,
    // Left, right.
    kStereo,
    // Mono, keyboard, and mic.
    kMonoAndKeyboard,
    // Left, right, keyboard, and mic.
    kStereoAndKeyboard
  };

  // Creates an APM instance. Use one instance for every primary audio stream
  // requiring processing. On the client-side, this would typically be one
  // instance for the near-end stream, and additional instances for each far-end
  // stream which requires processing. On the server-side, this would typically
  // be one instance for every incoming stream.
  static AudioProcessing* Create();
  // Allows passing in an optional configuration at create-time.
  static AudioProcessing* Create(const webrtc::Config& config);
  // Deprecated. Use the Create below, with nullptr PostProcessing.
  RTC_DEPRECATED
  static AudioProcessing* Create(const webrtc::Config& config,
                                 NonlinearBeamformer* beamformer);
  // Allows passing in optional user-defined processing modules.
  static AudioProcessing* Create(
      const webrtc::Config& config,
      std::unique_ptr<PostProcessing> capture_post_processor,
      std::unique_ptr<EchoControlFactory> echo_control_factory,
      NonlinearBeamformer* beamformer);
  ~AudioProcessing() override {}

  // Initializes internal states, while retaining all user settings. This
  // should be called before beginning to process a new audio stream. However,
  // it is not necessary to call before processing the first stream after
  // creation.
  //
  // It is also not necessary to call if the audio parameters (sample
  // rate and number of channels) have changed. Passing updated parameters
  // directly to |ProcessStream()| and |ProcessReverseStream()| is permissible.
  // If the parameters are known at init-time though, they may be provided.
  virtual int Initialize() = 0;

  // The int16 interfaces require:
  //   - only |NativeRate|s be used
  //   - that the input, output and reverse rates must match
  //   - that |processing_config.output_stream()| matches
  //     |processing_config.input_stream()|.
  //
  // The float interfaces accept arbitrary rates and support differing input and
  // output layouts, but the output must have either one channel or the same
  // number of channels as the input.
  virtual int Initialize(const ProcessingConfig& processing_config) = 0;

  // Initialize with unpacked parameters. See Initialize() above for details.
  //
  // TODO(mgraczyk): Remove once clients are updated to use the new interface.
  virtual int Initialize(int capture_input_sample_rate_hz,
                         int capture_output_sample_rate_hz,
                         int render_sample_rate_hz,
                         ChannelLayout capture_input_layout,
                         ChannelLayout capture_output_layout,
                         ChannelLayout render_input_layout) = 0;

  // TODO(peah): This method is a temporary solution used to take control
  // over the parameters in the audio processing module and is likely to change.
  virtual void ApplyConfig(const Config& config) = 0;

  // Pass down additional options which don't have explicit setters. This
  // ensures the options are applied immediately.
  virtual void SetExtraOptions(const webrtc::Config& config) = 0;

  // TODO(ajm): Only intended for internal use. Make private and friend the
  // necessary classes?
  virtual int proc_sample_rate_hz() const = 0;
  virtual int proc_split_sample_rate_hz() const = 0;
  virtual size_t num_input_channels() const = 0;
  virtual size_t num_proc_channels() const = 0;
  virtual size_t num_output_channels() const = 0;
  virtual size_t num_reverse_channels() const = 0;

  // Set to true when the output of AudioProcessing will be muted or in some
  // other way not used. Ideally, the captured audio would still be processed,
  // but some components may change behavior based on this information.
  // Default false.
  virtual void set_output_will_be_muted(bool muted) = 0;

  // Processes a 10 ms |frame| of the primary audio stream. On the client-side,
  // this is the near-end (or captured) audio.
  //
  // If needed for enabled functionality, any function with the set_stream_ tag
  // must be called prior to processing the current frame. Any getter function
  // with the stream_ tag which is needed should be called after processing.
  //
  // The |sample_rate_hz_|, |num_channels_|, and |samples_per_channel_|
  // members of |frame| must be valid. If changed from the previous call to this
  // method, it will trigger an initialization.
  virtual int ProcessStream(AudioFrame* frame) = 0;

  // Accepts deinterleaved float audio with the range [-1, 1]. Each element
  // of |src| points to a channel buffer, arranged according to
  // |input_layout|. At output, the channels will be arranged according to
  // |output_layout| at |output_sample_rate_hz| in |dest|.
  //
  // The output layout must have one channel or as many channels as the input.
  // |src| and |dest| may use the same memory, if desired.
  //
  // TODO(mgraczyk): Remove once clients are updated to use the new interface.
  virtual int ProcessStream(const float* const* src,
                            size_t samples_per_channel,
                            int input_sample_rate_hz,
                            ChannelLayout input_layout,
                            int output_sample_rate_hz,
                            ChannelLayout output_layout,
                            float* const* dest) = 0;

  // Accepts deinterleaved float audio with the range [-1, 1]. Each element of
  // |src| points to a channel buffer, arranged according to |input_stream|. At
  // output, the channels will be arranged according to |output_stream| in
  // |dest|.
  //
  // The output must have one channel or as many channels as the input. |src|
  // and |dest| may use the same memory, if desired.
  virtual int ProcessStream(const float* const* src,
                            const StreamConfig& input_config,
                            const StreamConfig& output_config,
                            float* const* dest) = 0;

  // Processes a 10 ms |frame| of the reverse direction audio stream. The frame
  // may be modified. On the client-side, this is the far-end (or to be
  // rendered) audio.
  //
  // It is necessary to provide this if echo processing is enabled, as the
  // reverse stream forms the echo reference signal. It is recommended, but not
  // necessary, to provide if gain control is enabled. On the server-side this
  // typically will not be used. If you're not sure what to pass in here,
  // chances are you don't need to use it.
  //
  // The |sample_rate_hz_|, |num_channels_|, and |samples_per_channel_|
  // members of |frame| must be valid.
  virtual int ProcessReverseStream(AudioFrame* frame) = 0;

  // Accepts deinterleaved float audio with the range [-1, 1]. Each element
  // of |data| points to a channel buffer, arranged according to |layout|.
  // TODO(mgraczyk): Remove once clients are updated to use the new interface.
  virtual int AnalyzeReverseStream(const float* const* data,
                                   size_t samples_per_channel,
                                   int sample_rate_hz,
                                   ChannelLayout layout) = 0;

  // Accepts deinterleaved float audio with the range [-1, 1]. Each element of
  // |data| points to a channel buffer, arranged according to |reverse_config|.
  virtual int ProcessReverseStream(const float* const* src,
                                   const StreamConfig& input_config,
                                   const StreamConfig& output_config,
                                   float* const* dest) = 0;

  // This must be called if and only if echo processing is enabled.
  //
  // Sets the |delay| in ms between ProcessReverseStream() receiving a far-end
  // frame and ProcessStream() receiving a near-end frame containing the
  // corresponding echo. On the client-side this can be expressed as
  //   delay = (t_render - t_analyze) + (t_process - t_capture)
  // where,
  //   - t_analyze is the time a frame is passed to ProcessReverseStream() and
  //     t_render is the time the first sample of the same frame is rendered by
  //     the audio hardware.
  //   - t_capture is the time the first sample of a frame is captured by the
  //     audio hardware and t_process is the time the same frame is passed to
  //     ProcessStream().
  virtual int set_stream_delay_ms(int delay) = 0;
  virtual int stream_delay_ms() const = 0;
  virtual bool was_stream_delay_set() const = 0;

  // Call to signal that a key press occurred (true) or did not occur (false)
  // with this chunk of audio.
  virtual void set_stream_key_pressed(bool key_pressed) = 0;

  // Sets a delay |offset| in ms to add to the values passed in through
  // set_stream_delay_ms(). May be positive or negative.
  //
  // Note that this could cause an otherwise valid value passed to
  // set_stream_delay_ms() to return an error.
  virtual void set_delay_offset_ms(int offset) = 0;
  virtual int delay_offset_ms() const = 0;

  // Attaches provided webrtc::AecDump for recording debugging
  // information. Log file and maximum file size logic is supposed to
  // be handled by implementing instance of AecDump. Calling this
  // method when another AecDump is attached resets the active AecDump
  // with a new one. This causes the d-tor of the earlier AecDump to
  // be called. The d-tor call may block until all pending logging
  // tasks are completed.
  virtual void AttachAecDump(std::unique_ptr<AecDump> aec_dump) = 0;

  // If no AecDump is attached, this has no effect. If an AecDump is
  // attached, it's destructor is called. The d-tor may block until
  // all pending logging tasks are completed.
  virtual void DetachAecDump() = 0;

  // Use to send UMA histograms at end of a call. Note that all histogram
  // specific member variables are reset.
  virtual void UpdateHistogramsOnCallEnd() = 0;

  // TODO(ivoc): Remove when the calling code no longer uses the old Statistics
  //             API.
  struct Statistic {
    int instant = 0;  // Instantaneous value.
    int average = 0;  // Long-term average.
    int maximum = 0;  // Long-term maximum.
    int minimum = 0;  // Long-term minimum.
  };

  struct Stat {
    void Set(const Statistic& other) {
      Set(other.instant, other.average, other.maximum, other.minimum);
    }
    void Set(float instant, float average, float maximum, float minimum) {
      instant_ = instant;
      average_ = average;
      maximum_ = maximum;
      minimum_ = minimum;
    }
    float instant() const { return instant_; }
    float average() const { return average_; }
    float maximum() const { return maximum_; }
    float minimum() const { return minimum_; }

   private:
    float instant_ = 0.0f;  // Instantaneous value.
    float average_ = 0.0f;  // Long-term average.
    float maximum_ = 0.0f;  // Long-term maximum.
    float minimum_ = 0.0f;  // Long-term minimum.
  };

  struct AudioProcessingStatistics {
    AudioProcessingStatistics();
    AudioProcessingStatistics(const AudioProcessingStatistics& other);
    ~AudioProcessingStatistics();

    // AEC Statistics.
    // RERL = ERL + ERLE
    Stat residual_echo_return_loss;
    // ERL = 10log_10(P_far / P_echo)
    Stat echo_return_loss;
    // ERLE = 10log_10(P_echo / P_out)
    Stat echo_return_loss_enhancement;
    // (Pre non-linear processing suppression) A_NLP = 10log_10(P_echo / P_a)
    Stat a_nlp;
    // Fraction of time that the AEC linear filter is divergent, in a 1-second
    // non-overlapped aggregation window.
    float divergent_filter_fraction = -1.0f;

    // The delay metrics consists of the delay median and standard deviation. It
    // also consists of the fraction of delay estimates that can make the echo
    // cancellation perform poorly. The values are aggregated until the first
    // call to |GetStatistics()| and afterwards aggregated and updated every
    // second. Note that if there are several clients pulling metrics from
    // |GetStatistics()| during a session the first call from any of them will
    // change to one second aggregation window for all.
    int delay_median = -1;
    int delay_standard_deviation = -1;
    float fraction_poor_delays = -1.0f;

    // Residual echo detector likelihood.
    float residual_echo_likelihood = -1.0f;
    // Maximum residual echo likelihood from the last time period.
    float residual_echo_likelihood_recent_max = -1.0f;
  };

  // TODO(ivoc): Make this pure virtual when all subclasses have been updated.
  virtual AudioProcessingStatistics GetStatistics() const;

  // This returns the stats as optionals and it will replace the regular
  // GetStatistics.
  virtual AudioProcessingStats GetStatistics(bool has_remote_tracks) const;

  // These provide access to the component interfaces and should never return
  // NULL. The pointers will be valid for the lifetime of the APM instance.
  // The memory for these objects is entirely managed internally.
  virtual EchoCancellation* echo_cancellation() const = 0;
  virtual EchoControlMobile* echo_control_mobile() const = 0;
  virtual GainControl* gain_control() const = 0;
  // TODO(peah): Deprecate this API call.
  virtual HighPassFilter* high_pass_filter() const = 0;
  virtual LevelEstimator* level_estimator() const = 0;
  virtual NoiseSuppression* noise_suppression() const = 0;
  virtual VoiceDetection* voice_detection() const = 0;

  // Returns the last applied configuration.
  virtual AudioProcessing::Config GetConfig() const = 0;

  enum Error {
    // Fatal errors.
    kNoError = 0,
    kUnspecifiedError = -1,
    kCreationFailedError = -2,
    kUnsupportedComponentError = -3,
    kUnsupportedFunctionError = -4,
    kNullPointerError = -5,
    kBadParameterError = -6,
    kBadSampleRateError = -7,
    kBadDataLengthError = -8,
    kBadNumberChannelsError = -9,
    kFileError = -10,
    kStreamParameterNotSetError = -11,
    kNotEnabledError = -12,

    // Warnings are non-fatal.
    // This results when a set_stream_ parameter is out of range. Processing
    // will continue, but the parameter may have been truncated.
    kBadStreamParameterWarning = -13
  };

  enum NativeRate {
    kSampleRate8kHz = 8000,
    kSampleRate16kHz = 16000,
    kSampleRate32kHz = 32000,
    kSampleRate44_1kHz = 44100,
    kSampleRate48kHz = 48000
  };

  // TODO(kwiberg): We currently need to support a compiler (Visual C++) that
  // complains if we don't explicitly state the size of the array here. Remove
  // the size when that's no longer the case.
  static constexpr int kNativeSampleRatesHz[4] = {
      kSampleRate8kHz, kSampleRate16kHz, kSampleRate32kHz, kSampleRate48kHz};
  static constexpr size_t kNumNativeSampleRates =
      arraysize(kNativeSampleRatesHz);
  static constexpr int kMaxNativeSampleRateHz =
      kNativeSampleRatesHz[kNumNativeSampleRates - 1];

  static const int kChunkSizeMs = 10;
};

class StreamConfig {
 public:
  // sample_rate_hz: The sampling rate of the stream.
  //
  // num_channels: The number of audio channels in the stream, excluding the
  //               keyboard channel if it is present. When passing a
  //               StreamConfig with an array of arrays T*[N],
  //
  //                N == {num_channels + 1  if  has_keyboard
  //                     {num_channels      if  !has_keyboard
  //
  // has_keyboard: True if the stream has a keyboard channel. When has_keyboard
  //               is true, the last channel in any corresponding list of
  //               channels is the keyboard channel.
  StreamConfig(int sample_rate_hz = 0,
               size_t num_channels = 0,
               bool has_keyboard = false)
      : sample_rate_hz_(sample_rate_hz),
        num_channels_(num_channels),
        has_keyboard_(has_keyboard),
        num_frames_(calculate_frames(sample_rate_hz)) {}

  void set_sample_rate_hz(int value) {
    sample_rate_hz_ = value;
    num_frames_ = calculate_frames(value);
  }
  void set_num_channels(size_t value) { num_channels_ = value; }
  void set_has_keyboard(bool value) { has_keyboard_ = value; }

  int sample_rate_hz() const { return sample_rate_hz_; }

  // The number of channels in the stream, not including the keyboard channel if
  // present.
  size_t num_channels() const { return num_channels_; }

  bool has_keyboard() const { return has_keyboard_; }
  size_t num_frames() const { return num_frames_; }
  size_t num_samples() const { return num_channels_ * num_frames_; }

  bool operator==(const StreamConfig& other) const {
    return sample_rate_hz_ == other.sample_rate_hz_ &&
           num_channels_ == other.num_channels_ &&
           has_keyboard_ == other.has_keyboard_;
  }

  bool operator!=(const StreamConfig& other) const { return !(*this == other); }

 private:
  static size_t calculate_frames(int sample_rate_hz) {
    return static_cast<size_t>(
        AudioProcessing::kChunkSizeMs * sample_rate_hz / 1000);
  }

  int sample_rate_hz_;
  size_t num_channels_;
  bool has_keyboard_;
  size_t num_frames_;
};

class ProcessingConfig {
 public:
  enum StreamName {
    kInputStream,
    kOutputStream,
    kReverseInputStream,
    kReverseOutputStream,
    kNumStreamNames,
  };

  const StreamConfig& input_stream() const {
    return streams[StreamName::kInputStream];
  }
  const StreamConfig& output_stream() const {
    return streams[StreamName::kOutputStream];
  }
  const StreamConfig& reverse_input_stream() const {
    return streams[StreamName::kReverseInputStream];
  }
  const StreamConfig& reverse_output_stream() const {
    return streams[StreamName::kReverseOutputStream];
  }

  StreamConfig& input_stream() { return streams[StreamName::kInputStream]; }
  StreamConfig& output_stream() { return streams[StreamName::kOutputStream]; }
  StreamConfig& reverse_input_stream() {
    return streams[StreamName::kReverseInputStream];
  }
  StreamConfig& reverse_output_stream() {
    return streams[StreamName::kReverseOutputStream];
  }

  bool operator==(const ProcessingConfig& other) const {
    for (int i = 0; i < StreamName::kNumStreamNames; ++i) {
      if (this->streams[i] != other.streams[i]) {
        return false;
      }
    }
    return true;
  }

  bool operator!=(const ProcessingConfig& other) const {
    return !(*this == other);
  }

  StreamConfig streams[StreamName::kNumStreamNames];
};

// The acoustic echo cancellation (AEC) component provides better performance
// than AECM but also requires more processing power and is dependent on delay
// stability and reporting accuracy. As such it is well-suited and recommended
// for PC and IP phone applications.
//
// Not recommended to be enabled on the server-side.
class EchoCancellation {
 public:
  // EchoCancellation and EchoControlMobile may not be enabled simultaneously.
  // Enabling one will disable the other.
  virtual int Enable(bool enable) = 0;
  virtual bool is_enabled() const = 0;

  // Differences in clock speed on the primary and reverse streams can impact
  // the AEC performance. On the client-side, this could be seen when different
  // render and capture devices are used, particularly with webcams.
  //
  // This enables a compensation mechanism, and requires that
  // set_stream_drift_samples() be called.
  virtual int enable_drift_compensation(bool enable) = 0;
  virtual bool is_drift_compensation_enabled() const = 0;

  // Sets the difference between the number of samples rendered and captured by
  // the audio devices since the last call to |ProcessStream()|. Must be called
  // if drift compensation is enabled, prior to |ProcessStream()|.
  virtual void set_stream_drift_samples(int drift) = 0;
  virtual int stream_drift_samples() const = 0;

  enum SuppressionLevel {
    kLowSuppression,
    kModerateSuppression,
    kHighSuppression
  };

  // Sets the aggressiveness of the suppressor. A higher level trades off
  // double-talk performance for increased echo suppression.
  virtual int set_suppression_level(SuppressionLevel level) = 0;
  virtual SuppressionLevel suppression_level() const = 0;

  // Returns false if the current frame almost certainly contains no echo
  // and true if it _might_ contain echo.
  virtual bool stream_has_echo() const = 0;

  // Enables the computation of various echo metrics. These are obtained
  // through |GetMetrics()|.
  virtual int enable_metrics(bool enable) = 0;
  virtual bool are_metrics_enabled() const = 0;

  // Each statistic is reported in dB.
  // P_far:  Far-end (render) signal power.
  // P_echo: Near-end (capture) echo signal power.
  // P_out:  Signal power at the output of the AEC.
  // P_a:    Internal signal power at the point before the AEC's non-linear
  //         processor.
  struct Metrics {
    // RERL = ERL + ERLE
    AudioProcessing::Statistic residual_echo_return_loss;

    // ERL = 10log_10(P_far / P_echo)
    AudioProcessing::Statistic echo_return_loss;

    // ERLE = 10log_10(P_echo / P_out)
    AudioProcessing::Statistic echo_return_loss_enhancement;

    // (Pre non-linear processing suppression) A_NLP = 10log_10(P_echo / P_a)
    AudioProcessing::Statistic a_nlp;

    // Fraction of time that the AEC linear filter is divergent, in a 1-second
    // non-overlapped aggregation window.
    float divergent_filter_fraction;
  };

  // Deprecated. Use GetStatistics on the AudioProcessing interface instead.
  // TODO(ajm): discuss the metrics update period.
  virtual int GetMetrics(Metrics* metrics) = 0;

  // Enables computation and logging of delay values. Statistics are obtained
  // through |GetDelayMetrics()|.
  virtual int enable_delay_logging(bool enable) = 0;
  virtual bool is_delay_logging_enabled() const = 0;

  // The delay metrics consists of the delay |median| and the delay standard
  // deviation |std|. It also consists of the fraction of delay estimates
  // |fraction_poor_delays| that can make the echo cancellation perform poorly.
  // The values are aggregated until the first call to |GetDelayMetrics()| and
  // afterwards aggregated and updated every second.
  // Note that if there are several clients pulling metrics from
  // |GetDelayMetrics()| during a session the first call from any of them will
  // change to one second aggregation window for all.
  // Deprecated. Use GetStatistics on the AudioProcessing interface instead.
  virtual int GetDelayMetrics(int* median, int* std) = 0;
  // Deprecated. Use GetStatistics on the AudioProcessing interface instead.
  virtual int GetDelayMetrics(int* median, int* std,
                              float* fraction_poor_delays) = 0;

  // Returns a pointer to the low level AEC component.  In case of multiple
  // channels, the pointer to the first one is returned.  A NULL pointer is
  // returned when the AEC component is disabled or has not been initialized
  // successfully.
  virtual struct AecCore* aec_core() const = 0;

 protected:
  virtual ~EchoCancellation() {}
};

// The acoustic echo control for mobile (AECM) component is a low complexity
// robust option intended for use on mobile devices.
//
// Not recommended to be enabled on the server-side.
class EchoControlMobile {
 public:
  // EchoCancellation and EchoControlMobile may not be enabled simultaneously.
  // Enabling one will disable the other.
  virtual int Enable(bool enable) = 0;
  virtual bool is_enabled() const = 0;

  // Recommended settings for particular audio routes. In general, the louder
  // the echo is expected to be, the higher this value should be set. The
  // preferred setting may vary from device to device.
  enum RoutingMode {
    kQuietEarpieceOrHeadset,
    kEarpiece,
    kLoudEarpiece,
    kSpeakerphone,
    kLoudSpeakerphone
  };

  // Sets echo control appropriate for the audio routing |mode| on the device.
  // It can and should be updated during a call if the audio routing changes.
  virtual int set_routing_mode(RoutingMode mode) = 0;
  virtual RoutingMode routing_mode() const = 0;

  // Comfort noise replaces suppressed background noise to maintain a
  // consistent signal level.
  virtual int enable_comfort_noise(bool enable) = 0;
  virtual bool is_comfort_noise_enabled() const = 0;

  // A typical use case is to initialize the component with an echo path from a
  // previous call. The echo path is retrieved using |GetEchoPath()|, typically
  // at the end of a call. The data can then be stored for later use as an
  // initializer before the next call, using |SetEchoPath()|.
  //
  // Controlling the echo path this way requires the data |size_bytes| to match
  // the internal echo path size. This size can be acquired using
  // |echo_path_size_bytes()|. |SetEchoPath()| causes an entire reset, worth
  // noting if it is to be called during an ongoing call.
  //
  // It is possible that version incompatibilities may result in a stored echo
  // path of the incorrect size. In this case, the stored path should be
  // discarded.
  virtual int SetEchoPath(const void* echo_path, size_t size_bytes) = 0;
  virtual int GetEchoPath(void* echo_path, size_t size_bytes) const = 0;

  // The returned path size is guaranteed not to change for the lifetime of
  // the application.
  static size_t echo_path_size_bytes();

 protected:
  virtual ~EchoControlMobile() {}
};

// Interface for an acoustic echo cancellation (AEC) submodule.
class EchoControl {
 public:
  // Analysis (not changing) of the render signal.
  virtual void AnalyzeRender(AudioBuffer* render) = 0;

  // Analysis (not changing) of the capture signal.
  virtual void AnalyzeCapture(AudioBuffer* capture) = 0;

  // Processes the capture signal in order to remove the echo.
  virtual void ProcessCapture(AudioBuffer* capture, bool echo_path_change) = 0;

  struct Metrics {
    double echo_return_loss;
    double echo_return_loss_enhancement;
    int delay_ms;
  };

  // Collect current metrics from the echo controller.
  virtual Metrics GetMetrics() const = 0;

  virtual ~EchoControl() {}
};

// Interface for a factory that creates EchoControllers.
class EchoControlFactory {
 public:
  virtual std::unique_ptr<EchoControl> Create(int sample_rate_hz) = 0;
  virtual ~EchoControlFactory() = default;
};

// The automatic gain control (AGC) component brings the signal to an
// appropriate range. This is done by applying a digital gain directly and, in
// the analog mode, prescribing an analog gain to be applied at the audio HAL.
//
// Recommended to be enabled on the client-side.
class GainControl {
 public:
  virtual int Enable(bool enable) = 0;
  virtual bool is_enabled() const = 0;

  // When an analog mode is set, this must be called prior to |ProcessStream()|
  // to pass the current analog level from the audio HAL. Must be within the
  // range provided to |set_analog_level_limits()|.
  virtual int set_stream_analog_level(int level) = 0;

  // When an analog mode is set, this should be called after |ProcessStream()|
  // to obtain the recommended new analog level for the audio HAL. It is the
  // users responsibility to apply this level.
  virtual int stream_analog_level() = 0;

  enum Mode {
    // Adaptive mode intended for use if an analog volume control is available
    // on the capture device. It will require the user to provide coupling
    // between the OS mixer controls and AGC through the |stream_analog_level()|
    // functions.
    //
    // It consists of an analog gain prescription for the audio device and a
    // digital compression stage.
    kAdaptiveAnalog,

    // Adaptive mode intended for situations in which an analog volume control
    // is unavailable. It operates in a similar fashion to the adaptive analog
    // mode, but with scaling instead applied in the digital domain. As with
    // the analog mode, it additionally uses a digital compression stage.
    kAdaptiveDigital,

    // Fixed mode which enables only the digital compression stage also used by
    // the two adaptive modes.
    //
    // It is distinguished from the adaptive modes by considering only a
    // short time-window of the input signal. It applies a fixed gain through
    // most of the input level range, and compresses (gradually reduces gain
    // with increasing level) the input signal at higher levels. This mode is
    // preferred on embedded devices where the capture signal level is
    // predictable, so that a known gain can be applied.
    kFixedDigital
  };

  virtual int set_mode(Mode mode) = 0;
  virtual Mode mode() const = 0;

  // Sets the target peak |level| (or envelope) of the AGC in dBFs (decibels
  // from digital full-scale). The convention is to use positive values. For
  // instance, passing in a value of 3 corresponds to -3 dBFs, or a target
  // level 3 dB below full-scale. Limited to [0, 31].
  //
  // TODO(ajm): use a negative value here instead, if/when VoE will similarly
  //            update its interface.
  virtual int set_target_level_dbfs(int level) = 0;
  virtual int target_level_dbfs() const = 0;

  // Sets the maximum |gain| the digital compression stage may apply, in dB. A
  // higher number corresponds to greater compression, while a value of 0 will
  // leave the signal uncompressed. Limited to [0, 90].
  virtual int set_compression_gain_db(int gain) = 0;
  virtual int compression_gain_db() const = 0;

  // When enabled, the compression stage will hard limit the signal to the
  // target level. Otherwise, the signal will be compressed but not limited
  // above the target level.
  virtual int enable_limiter(bool enable) = 0;
  virtual bool is_limiter_enabled() const = 0;

  // Sets the |minimum| and |maximum| analog levels of the audio capture device.
  // Must be set if and only if an analog mode is used. Limited to [0, 65535].
  virtual int set_analog_level_limits(int minimum,
                                      int maximum) = 0;
  virtual int analog_level_minimum() const = 0;
  virtual int analog_level_maximum() const = 0;

  // Returns true if the AGC has detected a saturation event (period where the
  // signal reaches digital full-scale) in the current frame and the analog
  // level cannot be reduced.
  //
  // This could be used as an indicator to reduce or disable analog mic gain at
  // the audio HAL.
  virtual bool stream_is_saturated() const = 0;

 protected:
  virtual ~GainControl() {}
};
// TODO(peah): Remove this interface.
// A filtering component which removes DC offset and low-frequency noise.
// Recommended to be enabled on the client-side.
class HighPassFilter {
 public:
  virtual int Enable(bool enable) = 0;
  virtual bool is_enabled() const = 0;

  virtual ~HighPassFilter() {}
};

// An estimation component used to retrieve level metrics.
class LevelEstimator {
 public:
  virtual int Enable(bool enable) = 0;
  virtual bool is_enabled() const = 0;

  // Returns the root mean square (RMS) level in dBFs (decibels from digital
  // full-scale), or alternately dBov. It is computed over all primary stream
  // frames since the last call to RMS(). The returned value is positive but
  // should be interpreted as negative. It is constrained to [0, 127].
  //
  // The computation follows: https://tools.ietf.org/html/rfc6465
  // with the intent that it can provide the RTP audio level indication.
  //
  // Frames passed to ProcessStream() with an |_energy| of zero are considered
  // to have been muted. The RMS of the frame will be interpreted as -127.
  virtual int RMS() = 0;

 protected:
  virtual ~LevelEstimator() {}
};

// The noise suppression (NS) component attempts to remove noise while
// retaining speech. Recommended to be enabled on the client-side.
//
// Recommended to be enabled on the client-side.
class NoiseSuppression {
 public:
  virtual int Enable(bool enable) = 0;
  virtual bool is_enabled() const = 0;

  // Determines the aggressiveness of the suppression. Increasing the level
  // will reduce the noise level at the expense of a higher speech distortion.
  enum Level {
    kLow,
    kModerate,
    kHigh,
    kVeryHigh
  };

  virtual int set_level(Level level) = 0;
  virtual Level level() const = 0;

  // Returns the internally computed prior speech probability of current frame
  // averaged over output channels. This is not supported in fixed point, for
  // which |kUnsupportedFunctionError| is returned.
  virtual float speech_probability() const = 0;

  // Returns the noise estimate per frequency bin averaged over all channels.
  virtual std::vector<float> NoiseEstimate() = 0;

 protected:
  virtual ~NoiseSuppression() {}
};

// Interface for a post processing submodule.
class PostProcessing {
 public:
  // (Re-)Initializes the submodule.
  virtual void Initialize(int sample_rate_hz, int num_channels) = 0;
  // Processes the given capture or render signal.
  virtual void Process(AudioBuffer* audio) = 0;
  // Returns a string representation of the module state.
  virtual std::string ToString() const = 0;

  virtual ~PostProcessing() {}
};

// The voice activity detection (VAD) component analyzes the stream to
// determine if voice is present. A facility is also provided to pass in an
// external VAD decision.
//
// In addition to |stream_has_voice()| the VAD decision is provided through the
// |AudioFrame| passed to |ProcessStream()|. The |vad_activity_| member will be
// modified to reflect the current decision.
class VoiceDetection {
 public:
  virtual int Enable(bool enable) = 0;
  virtual bool is_enabled() const = 0;

  // Returns true if voice is detected in the current frame. Should be called
  // after |ProcessStream()|.
  virtual bool stream_has_voice() const = 0;

  // Some of the APM functionality requires a VAD decision. In the case that
  // a decision is externally available for the current frame, it can be passed
  // in here, before |ProcessStream()| is called.
  //
  // VoiceDetection does _not_ need to be enabled to use this. If it happens to
  // be enabled, detection will be skipped for any frame in which an external
  // VAD decision is provided.
  virtual int set_stream_has_voice(bool has_voice) = 0;

  // Specifies the likelihood that a frame will be declared to contain voice.
  // A higher value makes it more likely that speech will not be clipped, at
  // the expense of more noise being detected as voice.
  enum Likelihood {
    kVeryLowLikelihood,
    kLowLikelihood,
    kModerateLikelihood,
    kHighLikelihood
  };

  virtual int set_likelihood(Likelihood likelihood) = 0;
  virtual Likelihood likelihood() const = 0;

  // Sets the |size| of the frames in ms on which the VAD will operate. Larger
  // frames will improve detection accuracy, but reduce the frequency of
  // updates.
  //
  // This does not impact the size of frames passed to |ProcessStream()|.
  virtual int set_frame_size_ms(int size) = 0;
  virtual int frame_size_ms() const = 0;

 protected:
  virtual ~VoiceDetection() {}
};

// Configuration struct for EchoCanceller3
struct EchoCanceller3Config {
  struct Delay {
    size_t default_delay = 5;
    size_t down_sampling_factor = 4;
    size_t num_filters = 4;
  } delay;

  struct Erle {
    float min = 1.f;
    float max_l = 8.f;
    float max_h = 1.5f;
  } erle;

  struct EpStrength {
    float lf = 10.f;
    float mf = 10.f;
    float hf = 10.f;
    float default_len = 0.f;
    bool echo_can_saturate = true;
    bool bounded_erl = false;
  } ep_strength;

  struct Mask {
    float m1 = 0.01f;
    float m2 = 0.0001f;
    float m3 = 0.01f;
    float m4 = 0.1f;
    float m5 = 0.3f;
    float m6 = 0.0001f;
    float m7 = 0.01f;
    float m8 = 0.0001f;
    float m9 = 0.1f;
  } gain_mask;

  struct EchoAudibility {
    float low_render_limit = 4 * 64.f;
    float normal_render_limit = 64.f;
  } echo_audibility;

  struct RenderLevels {
    float active_render_limit = 100.f;
    float poor_excitation_render_limit = 150.f;
  } render_levels;

  struct GainUpdates {
    struct GainChanges {
      float max_inc;
      float max_dec;
      float rate_inc;
      float rate_dec;
      float min_inc;
      float min_dec;
    };

    GainChanges low_noise = {3.f, 3.f, 1.5f, 1.5f, 1.5f, 1.5f};
    GainChanges normal = {2.f, 2.f, 1.5f, 1.5f, 1.2f, 1.2f};
    GainChanges saturation = {1.2f, 1.2f, 1.5f, 1.5f, 1.f, 1.f};
    GainChanges nonlinear = {1.5f, 1.5f, 1.2f, 1.2f, 1.1f, 1.1f};

    float floor_first_increase = 0.0001f;
  } gain_updates;
};

class EchoCanceller3Factory : public EchoControlFactory {
 public:
  EchoCanceller3Factory();
  EchoCanceller3Factory(const EchoCanceller3Config& config);
  std::unique_ptr<EchoControl> Create(int sample_rate_hz) override;

 private:
  EchoCanceller3Config config_;
};
}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_INCLUDE_AUDIO_PROCESSING_H_
