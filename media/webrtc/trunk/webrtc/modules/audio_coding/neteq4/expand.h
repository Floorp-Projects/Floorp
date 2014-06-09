/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_NETEQ4_EXPAND_H_
#define WEBRTC_MODULES_AUDIO_CODING_NETEQ4_EXPAND_H_

#include <assert.h>

#include "webrtc/modules/audio_coding/neteq4/audio_multi_vector.h"
#include "webrtc/system_wrappers/interface/constructor_magic.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/typedefs.h"

namespace webrtc {

// Forward declarations.
class BackgroundNoise;
class RandomVector;
class SyncBuffer;

// This class handles extrapolation of audio data from the sync_buffer to
// produce packet-loss concealment.
// TODO(hlundin): Refactor this class to divide the long methods into shorter
// ones.
class Expand {
 public:
  Expand(BackgroundNoise* background_noise,
         SyncBuffer* sync_buffer,
         RandomVector* random_vector,
         int fs,
         size_t num_channels)
      : background_noise_(background_noise),
        sync_buffer_(sync_buffer),
        random_vector_(random_vector),
        first_expand_(true),
        fs_hz_(fs),
        num_channels_(num_channels),
        overlap_length_(5 * fs / 8000),
        lag_index_direction_(0),
        current_lag_index_(0),
        stop_muting_(false),
        channel_parameters_(new ChannelParameters[num_channels_]) {
    assert(fs == 8000 || fs == 16000 || fs == 32000 || fs == 48000);
    assert(fs <= kMaxSampleRate);  // Should not be possible.
    assert(num_channels_ > 0);
    memset(expand_lags_, 0, sizeof(expand_lags_));
    Reset();
  }

  virtual ~Expand() {}

  // Resets the object.
  void Reset();

  // The main method to produce concealment data. The data is appended to the
  // end of |output|.
  int Process(AudioMultiVector* output);

  // Prepare the object to do extra expansion during normal operation following
  // a period of expands.
  void SetParametersForNormalAfterExpand();

  // Prepare the object to do extra expansion during merge operation following
  // a period of expands.
  void SetParametersForMergeAfterExpand();

  // Sets the mute factor for |channel| to |value|.
  void SetMuteFactor(int16_t value, size_t channel) {
    assert(channel < num_channels_);
    channel_parameters_[channel].mute_factor = value;
  }

  // Returns the mute factor for |channel|.
  int16_t MuteFactor(size_t channel) {
    assert(channel < num_channels_);
    return channel_parameters_[channel].mute_factor;
  }

  // Accessors and mutators.
  size_t overlap_length() const { return overlap_length_; }
  int16_t max_lag() const { return max_lag_; }

 private:
  static const int kUnvoicedLpcOrder = 6;
  static const int kNumCorrelationCandidates = 3;
  static const int kDistortionLength = 20;
  static const int kLpcAnalysisLength = 160;
  static const int kMaxSampleRate = 48000;
  static const int kNumLags = 3;
  static const int kMaxConsecutiveExpands = 200;

  struct ChannelParameters {
    // Constructor.
    ChannelParameters()
        : mute_factor(16384),
          ar_gain(0),
          ar_gain_scale(0),
          voice_mix_factor(0),
          current_voice_mix_factor(0),
          onset(false),
          mute_slope(0) {
      memset(ar_filter, 0, sizeof(ar_filter));
      memset(ar_filter_state, 0, sizeof(ar_filter_state));
    }
    int16_t mute_factor;
    int16_t ar_filter[kUnvoicedLpcOrder + 1];
    int16_t ar_filter_state[kUnvoicedLpcOrder];
    int16_t ar_gain;
    int16_t ar_gain_scale;
    int16_t voice_mix_factor; /* Q14 */
    int16_t current_voice_mix_factor; /* Q14 */
    AudioVector expand_vector0;
    AudioVector expand_vector1;
    bool onset;
    int16_t mute_slope; /* Q20 */
  };

  // Analyze the signal history in |sync_buffer_|, and set up all parameters
  // necessary to produce concealment data.
  void AnalyzeSignal(int16_t* random_vector);

  // Calculate the auto-correlation of |input|, with length |input_length|
  // samples. The correlation is calculated from a downsampled version of
  // |input|, and is written to |output|. The scale factor is written to
  // |output_scale|. Returns the length of the correlation vector.
  int16_t Correlation(const int16_t* input, size_t input_length,
                      int16_t* output, int16_t* output_scale) const;

  void UpdateLagIndex();

  BackgroundNoise* background_noise_;
  SyncBuffer* sync_buffer_;
  RandomVector* random_vector_;
  bool first_expand_;
  const int fs_hz_;
  const size_t num_channels_;
  const size_t overlap_length_;
  int consecutive_expands_;
  int16_t max_lag_;
  size_t expand_lags_[kNumLags];
  int lag_index_direction_;
  int current_lag_index_;
  bool stop_muting_;
  scoped_array<ChannelParameters> channel_parameters_;

  DISALLOW_COPY_AND_ASSIGN(Expand);
};

struct ExpandFactory {
  ExpandFactory() {}
  virtual ~ExpandFactory() {}

  virtual Expand* Create(BackgroundNoise* background_noise,
                         SyncBuffer* sync_buffer,
                         RandomVector* random_vector,
                         int fs,
                         size_t num_channels) const;
};

}  // namespace webrtc
#endif  // WEBRTC_MODULES_AUDIO_CODING_NETEQ4_EXPAND_H_
