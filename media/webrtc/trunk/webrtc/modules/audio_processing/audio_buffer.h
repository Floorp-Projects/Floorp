/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_AUDIO_BUFFER_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_AUDIO_BUFFER_H_

#include <vector>

#include "webrtc/modules/audio_processing/common.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/system_wrappers/interface/scoped_vector.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class PushSincResampler;
class IFChannelBuffer;

struct SplitFilterStates {
  SplitFilterStates() {
    memset(analysis_filter_state1, 0, sizeof(analysis_filter_state1));
    memset(analysis_filter_state2, 0, sizeof(analysis_filter_state2));
    memset(synthesis_filter_state1, 0, sizeof(synthesis_filter_state1));
    memset(synthesis_filter_state2, 0, sizeof(synthesis_filter_state2));
  }

  static const int kStateSize = 6;
  int analysis_filter_state1[kStateSize];
  int analysis_filter_state2[kStateSize];
  int synthesis_filter_state1[kStateSize];
  int synthesis_filter_state2[kStateSize];
};

class AudioBuffer {
 public:
  // TODO(ajm): Switch to take ChannelLayouts.
  AudioBuffer(int input_samples_per_channel,
              int num_input_channels,
              int process_samples_per_channel,
              int num_process_channels,
              int output_samples_per_channel);
  virtual ~AudioBuffer();

  int num_channels() const;
  int samples_per_channel() const;
  int samples_per_split_channel() const;
  int samples_per_keyboard_channel() const;

  // Sample array accessors. Channels are guaranteed to be stored contiguously
  // in memory. Prefer to use the const variants of each accessor when
  // possible, since they incur less float<->int16 conversion overhead.
  int16_t* data(int channel);
  const int16_t* data(int channel) const;
  int16_t* low_pass_split_data(int channel);
  const int16_t* low_pass_split_data(int channel) const;
  int16_t* high_pass_split_data(int channel);
  const int16_t* high_pass_split_data(int channel) const;\
  // Returns a pointer to the low-pass data downmixed to mono. If this data
  // isn't already available it re-calculates it.
  const int16_t* mixed_low_pass_data();
  const int16_t* low_pass_reference(int channel) const;

  // Float versions of the accessors, with automatic conversion back and forth
  // as necessary. The range of the numbers are the same as for int16_t.
  float* data_f(int channel);
  const float* data_f(int channel) const;

  float* const* channels_f();
  const float* const* channels_f() const;

  float* low_pass_split_data_f(int channel);
  const float* low_pass_split_data_f(int channel) const;
  float* high_pass_split_data_f(int channel);
  const float* high_pass_split_data_f(int channel) const;

  float* const* low_pass_split_channels_f();
  const float* const* low_pass_split_channels_f() const;
  float* const* high_pass_split_channels_f();
  const float* const* high_pass_split_channels_f() const;

  const float* keyboard_data() const;

  SplitFilterStates* filter_states(int channel);

  void set_activity(AudioFrame::VADActivity activity);
  AudioFrame::VADActivity activity() const;

  // Use for int16 interleaved data.
  void DeinterleaveFrom(AudioFrame* audioFrame);
  // If |data_changed| is false, only the non-audio data members will be copied
  // to |frame|.
  void InterleaveTo(AudioFrame* frame, bool data_changed) const;

  // Use for float deinterleaved data.
  void CopyFrom(const float* const* data,
                int samples_per_channel,
                AudioProcessing::ChannelLayout layout);
  void CopyTo(int samples_per_channel,
              AudioProcessing::ChannelLayout layout,
              float* const* data);
  void CopyLowPassToReference();

 private:
  // Called from DeinterleaveFrom() and CopyFrom().
  void InitForNewData();

  const int input_samples_per_channel_;
  const int num_input_channels_;
  const int proc_samples_per_channel_;
  const int num_proc_channels_;
  const int output_samples_per_channel_;
  int samples_per_split_channel_;
  bool mixed_low_pass_valid_;
  bool reference_copied_;
  AudioFrame::VADActivity activity_;

  const float* keyboard_data_;
  scoped_ptr<IFChannelBuffer> channels_;
  scoped_ptr<IFChannelBuffer> split_channels_low_;
  scoped_ptr<IFChannelBuffer> split_channels_high_;
  scoped_ptr<SplitFilterStates[]> filter_states_;
  scoped_ptr<ChannelBuffer<int16_t> > mixed_low_pass_channels_;
  scoped_ptr<ChannelBuffer<int16_t> > low_pass_reference_channels_;
  scoped_ptr<ChannelBuffer<float> > input_buffer_;
  scoped_ptr<ChannelBuffer<float> > process_buffer_;
  ScopedVector<PushSincResampler> input_resamplers_;
  ScopedVector<PushSincResampler> output_resamplers_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_AUDIO_BUFFER_H_
