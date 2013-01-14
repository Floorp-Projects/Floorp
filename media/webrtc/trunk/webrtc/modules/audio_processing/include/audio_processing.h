/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_INCLUDE_AUDIO_PROCESSING_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_INCLUDE_AUDIO_PROCESSING_H_

#include <stddef.h>  // size_t

#include "webrtc/modules/interface/module.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class AudioFrame;
class EchoCancellation;
class EchoControlMobile;
class GainControl;
class HighPassFilter;
class LevelEstimator;
class NoiseSuppression;
class VoiceDetection;

// The Audio Processing Module (APM) provides a collection of voice processing
// components designed for real-time communications software.
//
// APM operates on two audio streams on a frame-by-frame basis. Frames of the
// primary stream, on which all processing is applied, are passed to
// |ProcessStream()|. Frames of the reverse direction stream, which are used for
// analysis by some components, are passed to |AnalyzeReverseStream()|. On the
// client-side, this will typically be the near-end (capture) and far-end
// (render) streams, respectively. APM should be placed in the signal chain as
// close to the audio hardware abstraction layer (HAL) as possible.
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
// APM accepts only 16-bit linear PCM audio data in frames of 10 ms. Multiple
// channels should be interleaved.
//
// Usage example, omitting error checking:
// AudioProcessing* apm = AudioProcessing::Create(0);
// apm->set_sample_rate_hz(32000); // Super-wideband processing.
//
// // Mono capture and stereo render.
// apm->set_num_channels(1, 1);
// apm->set_num_reverse_channels(2);
//
// apm->high_pass_filter()->Enable(true);
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
// apm->AnalyzeReverseStream(render_frame);
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
// AudioProcessing::Destroy(apm);
// apm = NULL;
//
class AudioProcessing : public Module {
 public:
  // Creates a APM instance, with identifier |id|. Use one instance for every
  // primary audio stream requiring processing. On the client-side, this would
  // typically be one instance for the near-end stream, and additional instances
  // for each far-end stream which requires processing. On the server-side,
  // this would typically be one instance for every incoming stream.
  static AudioProcessing* Create(int id);
  virtual ~AudioProcessing() {}

  // TODO(andrew): remove this method. We now allow users to delete instances
  // directly, useful for scoped_ptr.
  // Destroys a |apm| instance.
  static void Destroy(AudioProcessing* apm);

  // Initializes internal states, while retaining all user settings. This
  // should be called before beginning to process a new audio stream. However,
  // it is not necessary to call before processing the first stream after
  // creation.
  //
  // set_sample_rate_hz(), set_num_channels() and set_num_reverse_channels()
  // will trigger a full initialization if the settings are changed from their
  // existing values. Otherwise they are no-ops.
  virtual int Initialize() = 0;

  // Sets the sample |rate| in Hz for both the primary and reverse audio
  // streams. 8000, 16000 or 32000 Hz are permitted.
  virtual int set_sample_rate_hz(int rate) = 0;
  virtual int sample_rate_hz() const = 0;

  // Sets the number of channels for the primary audio stream. Input frames must
  // contain a number of channels given by |input_channels|, while output frames
  // will be returned with number of channels given by |output_channels|.
  virtual int set_num_channels(int input_channels, int output_channels) = 0;
  virtual int num_input_channels() const = 0;
  virtual int num_output_channels() const = 0;

  // Sets the number of channels for the reverse audio stream. Input frames must
  // contain a number of channels given by |channels|.
  virtual int set_num_reverse_channels(int channels) = 0;
  virtual int num_reverse_channels() const = 0;

  // Processes a 10 ms |frame| of the primary audio stream. On the client-side,
  // this is the near-end (or captured) audio.
  //
  // If needed for enabled functionality, any function with the set_stream_ tag
  // must be called prior to processing the current frame. Any getter function
  // with the stream_ tag which is needed should be called after processing.
  //
  // The |sample_rate_hz_|, |num_channels_|, and |samples_per_channel_|
  // members of |frame| must be valid, and correspond to settings supplied
  // to APM.
  virtual int ProcessStream(AudioFrame* frame) = 0;

  // Analyzes a 10 ms |frame| of the reverse direction audio stream. The frame
  // will not be modified. On the client-side, this is the far-end (or to be
  // rendered) audio.
  //
  // It is only necessary to provide this if echo processing is enabled, as the
  // reverse stream forms the echo reference signal. It is recommended, but not
  // necessary, to provide if gain control is enabled. On the server-side this
  // typically will not be used. If you're not sure what to pass in here,
  // chances are you don't need to use it.
  //
  // The |sample_rate_hz_|, |num_channels_|, and |samples_per_channel_|
  // members of |frame| must be valid.
  //
  // TODO(ajm): add const to input; requires an implementation fix.
  virtual int AnalyzeReverseStream(AudioFrame* frame) = 0;

  // This must be called if and only if echo processing is enabled.
  //
  // Sets the |delay| in ms between AnalyzeReverseStream() receiving a far-end
  // frame and ProcessStream() receiving a near-end frame containing the
  // corresponding echo. On the client-side this can be expressed as
  //   delay = (t_render - t_analyze) + (t_process - t_capture)
  // where,
  //   - t_analyze is the time a frame is passed to AnalyzeReverseStream() and
  //     t_render is the time the first sample of the same frame is rendered by
  //     the audio hardware.
  //   - t_capture is the time the first sample of a frame is captured by the
  //     audio hardware and t_pull is the time the same frame is passed to
  //     ProcessStream().
  virtual int set_stream_delay_ms(int delay) = 0;
  virtual int stream_delay_ms() const = 0;

  // Sets a delay |offset| in ms to add to the values passed in through
  // set_stream_delay_ms(). May be positive or negative.
  //
  // Note that this could cause an otherwise valid value passed to
  // set_stream_delay_ms() to return an error.
  virtual void set_delay_offset_ms(int offset) = 0;
  virtual int delay_offset_ms() const = 0;

  // Starts recording debugging information to a file specified by |filename|,
  // a NULL-terminated string. If there is an ongoing recording, the old file
  // will be closed, and recording will continue in the newly specified file.
  // An already existing file will be overwritten without warning.
  static const size_t kMaxFilenameSize = 1024;
  virtual int StartDebugRecording(const char filename[kMaxFilenameSize]) = 0;

  // Stops recording debugging information, and closes the file. Recording
  // cannot be resumed in the same file (without overwriting it).
  virtual int StopDebugRecording() = 0;

  // These provide access to the component interfaces and should never return
  // NULL. The pointers will be valid for the lifetime of the APM instance.
  // The memory for these objects is entirely managed internally.
  virtual EchoCancellation* echo_cancellation() const = 0;
  virtual EchoControlMobile* echo_control_mobile() const = 0;
  virtual GainControl* gain_control() const = 0;
  virtual HighPassFilter* high_pass_filter() const = 0;
  virtual LevelEstimator* level_estimator() const = 0;
  virtual NoiseSuppression* noise_suppression() const = 0;
  virtual VoiceDetection* voice_detection() const = 0;

  struct Statistic {
    int instant;  // Instantaneous value.
    int average;  // Long-term average.
    int maximum;  // Long-term maximum.
    int minimum;  // Long-term minimum.
  };

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

  // Inherited from Module.
  virtual WebRtc_Word32 TimeUntilNextProcess() { return -1; }
  virtual WebRtc_Word32 Process() { return -1; }
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
  // |set_device_sample_rate_hz()| and |set_stream_drift_samples()| be called.
  virtual int enable_drift_compensation(bool enable) = 0;
  virtual bool is_drift_compensation_enabled() const = 0;

  // Provides the sampling rate of the audio devices. It is assumed the render
  // and capture devices use the same nominal sample rate. Required if and only
  // if drift compensation is enabled.
  virtual int set_device_sample_rate_hz(int rate) = 0;
  virtual int device_sample_rate_hz() const = 0;

  // Sets the difference between the number of samples rendered and captured by
  // the audio devices since the last call to |ProcessStream()|. Must be called
  // if and only if drift compensation is enabled, prior to |ProcessStream()|.
  virtual int set_stream_drift_samples(int drift) = 0;
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
  };

  // TODO(ajm): discuss the metrics update period.
  virtual int GetMetrics(Metrics* metrics) = 0;

  // Enables computation and logging of delay values. Statistics are obtained
  // through |GetDelayMetrics()|.
  virtual int enable_delay_logging(bool enable) = 0;
  virtual bool is_delay_logging_enabled() const = 0;

  // The delay metrics consists of the delay |median| and the delay standard
  // deviation |std|. The values are averaged over the time period since the
  // last call to |GetDelayMetrics()|.
  virtual int GetDelayMetrics(int* median, int* std) = 0;

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

// A filtering component which removes DC offset and low-frequency noise.
// Recommended to be enabled on the client-side.
class HighPassFilter {
 public:
  virtual int Enable(bool enable) = 0;
  virtual bool is_enabled() const = 0;

 protected:
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
  // The computation follows:
  // http://tools.ietf.org/html/draft-ietf-avtext-client-to-mixer-audio-level-05
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

 protected:
  virtual ~NoiseSuppression() {}
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
}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_INCLUDE_AUDIO_PROCESSING_H_
