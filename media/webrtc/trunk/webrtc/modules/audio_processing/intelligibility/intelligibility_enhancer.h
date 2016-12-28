/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

//
//  Specifies core class for intelligbility enhancement.
//

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_INTELLIGIBILITY_INTELLIGIBILITY_ENHANCER_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_INTELLIGIBILITY_INTELLIGIBILITY_ENHANCER_H_

#include <complex>
#include <vector>

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/common_audio/lapped_transform.h"
#include "webrtc/common_audio/channel_buffer.h"
#include "webrtc/modules/audio_processing/intelligibility/intelligibility_utils.h"

namespace webrtc {

// Speech intelligibility enhancement module. Reads render and capture
// audio streams and modifies the render stream with a set of gains per
// frequency bin to enhance speech against the noise background.
// Note: assumes speech and noise streams are already separated.
class IntelligibilityEnhancer {
 public:
  struct Config {
    // |var_*| are parameters for the VarianceArray constructor for the
    // clear speech stream.
    // TODO(bercic): the |var_*|, |*_rate| and |gain_limit| parameters should
    // probably go away once fine tuning is done.
    Config()
        : sample_rate_hz(16000),
          num_capture_channels(1),
          num_render_channels(1),
          var_type(intelligibility::VarianceArray::kStepDecaying),
          var_decay_rate(0.9f),
          var_window_size(10),
          analysis_rate(800),
          gain_change_limit(0.1f),
          rho(0.02f) {}
    int sample_rate_hz;
    size_t num_capture_channels;
    size_t num_render_channels;
    intelligibility::VarianceArray::StepType var_type;
    float var_decay_rate;
    size_t var_window_size;
    int analysis_rate;
    float gain_change_limit;
    float rho;
  };

  explicit IntelligibilityEnhancer(const Config& config);
  IntelligibilityEnhancer();  // Initialize with default config.

  // Reads and processes chunk of noise stream in time domain.
  void AnalyzeCaptureAudio(float* const* audio,
                           int sample_rate_hz,
                           size_t num_channels);

  // Reads chunk of speech in time domain and updates with modified signal.
  void ProcessRenderAudio(float* const* audio,
                          int sample_rate_hz,
                          size_t num_channels);
  bool active() const;

 private:
  enum AudioSource {
    kRenderStream = 0,  // Clear speech stream.
    kCaptureStream,  // Noise stream.
  };

  // Provides access point to the frequency domain.
  class TransformCallback : public LappedTransform::Callback {
   public:
    TransformCallback(IntelligibilityEnhancer* parent, AudioSource source);

    // All in frequency domain, receives input |in_block|, applies
    // intelligibility enhancement, and writes result to |out_block|.
    void ProcessAudioBlock(const std::complex<float>* const* in_block,
                           size_t in_channels,
                           size_t frames,
                           size_t out_channels,
                           std::complex<float>* const* out_block) override;

   private:
    IntelligibilityEnhancer* parent_;
    AudioSource source_;
  };
  friend class TransformCallback;
  FRIEND_TEST_ALL_PREFIXES(IntelligibilityEnhancerTest, TestErbCreation);
  FRIEND_TEST_ALL_PREFIXES(IntelligibilityEnhancerTest, TestSolveForGains);

  // Sends streams to ProcessClearBlock or ProcessNoiseBlock based on source.
  void DispatchAudio(AudioSource source,
                     const std::complex<float>* in_block,
                     std::complex<float>* out_block);

  // Updates variance computation and analysis with |in_block_|,
  // and writes modified speech to |out_block|.
  void ProcessClearBlock(const std::complex<float>* in_block,
                         std::complex<float>* out_block);

  // Computes and sets modified gains.
  void AnalyzeClearBlock(float power_target);

  // Bisection search for optimal |lambda|.
  void SolveForLambda(float power_target, float power_bot, float power_top);

  // Transforms freq gains to ERB gains.
  void UpdateErbGains();

  // Updates variance calculation for noise input with |in_block|.
  void ProcessNoiseBlock(const std::complex<float>* in_block,
                         std::complex<float>* out_block);

  // Returns number of ERB filters.
  static size_t GetBankSize(int sample_rate, size_t erb_resolution);

  // Initializes ERB filterbank.
  void CreateErbBank();

  // Analytically solves quadratic for optimal gains given |lambda|.
  // Negative gains are set to 0. Stores the results in |sols|.
  void SolveForGainsGivenLambda(float lambda, size_t start_freq, float* sols);

  // Computes variance across ERB filters from freq variance |var|.
  // Stores in |result|.
  void FilterVariance(const float* var, float* result);

  // Returns dot product of vectors specified by size |length| arrays |a|,|b|.
  static float DotProduct(const float* a, const float* b, size_t length);

  const size_t freqs_;         // Num frequencies in frequency domain.
  const size_t window_size_;   // Window size in samples; also the block size.
  const size_t chunk_length_;  // Chunk size in samples.
  const size_t bank_size_;     // Num ERB filters.
  const int sample_rate_hz_;
  const int erb_resolution_;
  const size_t num_capture_channels_;
  const size_t num_render_channels_;
  const int analysis_rate_;    // Num blocks before gains recalculated.

  const bool active_;          // Whether render gains are being updated.
                               // TODO(ekm): Add logic for updating |active_|.

  intelligibility::VarianceArray clear_variance_;
  intelligibility::VarianceArray noise_variance_;
  rtc::scoped_ptr<float[]> filtered_clear_var_;
  rtc::scoped_ptr<float[]> filtered_noise_var_;
  std::vector<std::vector<float>> filter_bank_;
  rtc::scoped_ptr<float[]> center_freqs_;
  size_t start_freq_;
  rtc::scoped_ptr<float[]> rho_;  // Production and interpretation SNR.
                                  // for each ERB band.
  rtc::scoped_ptr<float[]> gains_eq_;  // Pre-filter modified gains.
  intelligibility::GainApplier gain_applier_;

  // Destination buffers used to reassemble blocked chunks before overwriting
  // the original input array with modifications.
  ChannelBuffer<float> temp_render_out_buffer_;
  ChannelBuffer<float> temp_capture_out_buffer_;

  rtc::scoped_ptr<float[]> kbd_window_;
  TransformCallback render_callback_;
  TransformCallback capture_callback_;
  rtc::scoped_ptr<LappedTransform> render_mangler_;
  rtc::scoped_ptr<LappedTransform> capture_mangler_;
  int block_count_;
  int analysis_step_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_INTELLIGIBILITY_INTELLIGIBILITY_ENHANCER_H_
