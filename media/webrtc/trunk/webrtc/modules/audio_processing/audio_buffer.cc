/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/audio_buffer.h"

#include "webrtc/common_audio/signal_processing/include/signal_processing_library.h"

namespace webrtc {
namespace {

enum {
  kSamplesPer8kHzChannel = 80,
  kSamplesPer16kHzChannel = 160,
  kSamplesPer32kHzChannel = 320
};

void StereoToMono(const int16_t* left, const int16_t* right,
                  int16_t* out, int samples_per_channel) {
  assert(left != NULL && right != NULL && out != NULL);
  for (int i = 0; i < samples_per_channel; i++) {
    int32_t data32 = (static_cast<int32_t>(left[i]) +
                      static_cast<int32_t>(right[i])) >> 1;

    out[i] = WebRtcSpl_SatW32ToW16(data32);
  }
}
}  // namespace

struct AudioChannel {
  AudioChannel() {
    memset(data, 0, sizeof(data));
  }

  int16_t data[kSamplesPer32kHzChannel];
};

struct SplitAudioChannel {
  SplitAudioChannel() {
    memset(low_pass_data, 0, sizeof(low_pass_data));
    memset(high_pass_data, 0, sizeof(high_pass_data));
    memset(analysis_filter_state1, 0, sizeof(analysis_filter_state1));
    memset(analysis_filter_state2, 0, sizeof(analysis_filter_state2));
    memset(synthesis_filter_state1, 0, sizeof(synthesis_filter_state1));
    memset(synthesis_filter_state2, 0, sizeof(synthesis_filter_state2));
  }

  int16_t low_pass_data[kSamplesPer16kHzChannel];
  int16_t high_pass_data[kSamplesPer16kHzChannel];

  int32_t analysis_filter_state1[6];
  int32_t analysis_filter_state2[6];
  int32_t synthesis_filter_state1[6];
  int32_t synthesis_filter_state2[6];
};

// TODO(andrew): check range of input parameters?
AudioBuffer::AudioBuffer(int max_num_channels,
                         int samples_per_channel)
  : max_num_channels_(max_num_channels),
    num_channels_(0),
    num_mixed_channels_(0),
    num_mixed_low_pass_channels_(0),
    data_was_mixed_(false),
    samples_per_channel_(samples_per_channel),
    samples_per_split_channel_(samples_per_channel),
    reference_copied_(false),
    activity_(AudioFrame::kVadUnknown),
    is_muted_(false),
    data_(NULL),
    channels_(NULL),
    split_channels_(NULL),
    mixed_channels_(NULL),
    mixed_low_pass_channels_(NULL),
    low_pass_reference_channels_(NULL) {
  if (max_num_channels_ > 1) {
    channels_.reset(new AudioChannel[max_num_channels_]);
    mixed_channels_.reset(new AudioChannel[max_num_channels_]);
    mixed_low_pass_channels_.reset(new AudioChannel[max_num_channels_]);
  }
  low_pass_reference_channels_.reset(new AudioChannel[max_num_channels_]);

  if (samples_per_channel_ == kSamplesPer32kHzChannel) {
    split_channels_.reset(new SplitAudioChannel[max_num_channels_]);
    samples_per_split_channel_ = kSamplesPer16kHzChannel;
  }
}

AudioBuffer::~AudioBuffer() {}

int16_t* AudioBuffer::data(int channel) const {
  assert(channel >= 0 && channel < num_channels_);
  if (data_ != NULL) {
    return data_;
  }

  return channels_[channel].data;
}

int16_t* AudioBuffer::low_pass_split_data(int channel) const {
  assert(channel >= 0 && channel < num_channels_);
  if (split_channels_.get() == NULL) {
    return data(channel);
  }

  return split_channels_[channel].low_pass_data;
}

int16_t* AudioBuffer::high_pass_split_data(int channel) const {
  assert(channel >= 0 && channel < num_channels_);
  if (split_channels_.get() == NULL) {
    return NULL;
  }

  return split_channels_[channel].high_pass_data;
}

int16_t* AudioBuffer::mixed_data(int channel) const {
  assert(channel >= 0 && channel < num_mixed_channels_);

  return mixed_channels_[channel].data;
}

int16_t* AudioBuffer::mixed_low_pass_data(int channel) const {
  assert(channel >= 0 && channel < num_mixed_low_pass_channels_);

  return mixed_low_pass_channels_[channel].data;
}

int16_t* AudioBuffer::low_pass_reference(int channel) const {
  assert(channel >= 0 && channel < num_channels_);
  if (!reference_copied_) {
    return NULL;
  }

  return low_pass_reference_channels_[channel].data;
}

int32_t* AudioBuffer::analysis_filter_state1(int channel) const {
  assert(channel >= 0 && channel < num_channels_);
  return split_channels_[channel].analysis_filter_state1;
}

int32_t* AudioBuffer::analysis_filter_state2(int channel) const {
  assert(channel >= 0 && channel < num_channels_);
  return split_channels_[channel].analysis_filter_state2;
}

int32_t* AudioBuffer::synthesis_filter_state1(int channel) const {
  assert(channel >= 0 && channel < num_channels_);
  return split_channels_[channel].synthesis_filter_state1;
}

int32_t* AudioBuffer::synthesis_filter_state2(int channel) const {
  assert(channel >= 0 && channel < num_channels_);
  return split_channels_[channel].synthesis_filter_state2;
}

void AudioBuffer::set_activity(AudioFrame::VADActivity activity) {
  activity_ = activity;
}

AudioFrame::VADActivity AudioBuffer::activity() const {
  return activity_;
}

bool AudioBuffer::is_muted() const {
  return is_muted_;
}

int AudioBuffer::num_channels() const {
  return num_channels_;
}

int AudioBuffer::samples_per_channel() const {
  return samples_per_channel_;
}

int AudioBuffer::samples_per_split_channel() const {
  return samples_per_split_channel_;
}

// TODO(andrew): Do deinterleaving and mixing in one step?
void AudioBuffer::DeinterleaveFrom(AudioFrame* frame) {
  assert(frame->num_channels_ <= max_num_channels_);
  assert(frame->samples_per_channel_ ==  samples_per_channel_);

  num_channels_ = frame->num_channels_;
  data_was_mixed_ = false;
  num_mixed_channels_ = 0;
  num_mixed_low_pass_channels_ = 0;
  reference_copied_ = false;
  activity_ = frame->vad_activity_;
  is_muted_ = false;
  if (frame->energy_ == 0) {
    is_muted_ = true;
  }

  if (num_channels_ == 1) {
    // We can get away with a pointer assignment in this case.
    data_ = frame->data_;
    return;
  }

  int16_t* interleaved = frame->data_;
  for (int i = 0; i < num_channels_; i++) {
    int16_t* deinterleaved = channels_[i].data;
    int interleaved_idx = i;
    for (int j = 0; j < samples_per_channel_; j++) {
      deinterleaved[j] = interleaved[interleaved_idx];
      interleaved_idx += num_channels_;
    }
  }
}

void AudioBuffer::InterleaveTo(AudioFrame* frame, bool data_changed) const {
  assert(frame->num_channels_ == num_channels_);
  assert(frame->samples_per_channel_ == samples_per_channel_);
  frame->vad_activity_ = activity_;

  if (!data_changed) {
    return;
  }

  if (num_channels_ == 1) {
    if (data_was_mixed_) {
      memcpy(frame->data_,
             channels_[0].data,
             sizeof(int16_t) * samples_per_channel_);
    } else {
      // These should point to the same buffer in this case.
      assert(data_ == frame->data_);
    }

    return;
  }

  int16_t* interleaved = frame->data_;
  for (int i = 0; i < num_channels_; i++) {
    int16_t* deinterleaved = channels_[i].data;
    int interleaved_idx = i;
    for (int j = 0; j < samples_per_channel_; j++) {
      interleaved[interleaved_idx] = deinterleaved[j];
      interleaved_idx += num_channels_;
    }
  }
}

// TODO(andrew): would be good to support the no-mix case with pointer
// assignment.
// TODO(andrew): handle mixing to multiple channels?
void AudioBuffer::Mix(int num_mixed_channels) {
  // We currently only support the stereo to mono case.
  assert(num_channels_ == 2);
  assert(num_mixed_channels == 1);

  StereoToMono(channels_[0].data,
               channels_[1].data,
               channels_[0].data,
               samples_per_channel_);

  num_channels_ = num_mixed_channels;
  data_was_mixed_ = true;
}

void AudioBuffer::CopyAndMix(int num_mixed_channels) {
  // We currently only support the stereo to mono case.
  assert(num_channels_ == 2);
  assert(num_mixed_channels == 1);

  StereoToMono(channels_[0].data,
               channels_[1].data,
               mixed_channels_[0].data,
               samples_per_channel_);

  num_mixed_channels_ = num_mixed_channels;
}

void AudioBuffer::CopyAndMixLowPass(int num_mixed_channels) {
  // We currently only support the stereo to mono case.
  assert(num_channels_ == 2);
  assert(num_mixed_channels == 1);

  StereoToMono(low_pass_split_data(0),
               low_pass_split_data(1),
               mixed_low_pass_channels_[0].data,
               samples_per_split_channel_);

  num_mixed_low_pass_channels_ = num_mixed_channels;
}

void AudioBuffer::CopyLowPassToReference() {
  reference_copied_ = true;
  for (int i = 0; i < num_channels_; i++) {
    memcpy(low_pass_reference_channels_[i].data,
           low_pass_split_data(i),
           sizeof(int16_t) * samples_per_split_channel_);
  }
}
}  // namespace webrtc
