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

#include "webrtc/common_audio/include/audio_util.h"
#include "webrtc/common_audio/resampler/push_sinc_resampler.h"
#include "webrtc/common_audio/signal_processing/include/signal_processing_library.h"

namespace webrtc {
namespace {

enum {
  kSamplesPer8kHzChannel = 80,
  kSamplesPer16kHzChannel = 160,
  kSamplesPer32kHzChannel = 320
};

bool HasKeyboardChannel(AudioProcessing::ChannelLayout layout) {
  switch (layout) {
    case AudioProcessing::kMono:
    case AudioProcessing::kStereo:
      return false;
    case AudioProcessing::kMonoAndKeyboard:
    case AudioProcessing::kStereoAndKeyboard:
      return true;
  }
  assert(false);
  return false;
}

int KeyboardChannelIndex(AudioProcessing::ChannelLayout layout) {
  switch (layout) {
    case AudioProcessing::kMono:
    case AudioProcessing::kStereo:
      assert(false);
      return -1;
    case AudioProcessing::kMonoAndKeyboard:
      return 1;
    case AudioProcessing::kStereoAndKeyboard:
      return 2;
  }
  assert(false);
  return -1;
}

template <typename T>
void StereoToMono(const T* left, const T* right, T* out,
                  int samples_per_channel) {
  for (int i = 0; i < samples_per_channel; ++i)
    out[i] = (left[i] + right[i]) / 2;
}

}  // namespace

// One int16_t and one float ChannelBuffer that are kept in sync. The sync is
// broken when someone requests write access to either ChannelBuffer, and
// reestablished when someone requests the outdated ChannelBuffer. It is
// therefore safe to use the return value of ibuf_const() and fbuf_const()
// until the next call to ibuf() or fbuf(), and the return value of ibuf() and
// fbuf() until the next call to any of the other functions.
class IFChannelBuffer {
 public:
  IFChannelBuffer(int samples_per_channel, int num_channels)
      : ivalid_(true),
        ibuf_(samples_per_channel, num_channels),
        fvalid_(true),
        fbuf_(samples_per_channel, num_channels) {}

  ChannelBuffer<int16_t>* ibuf() { return ibuf(false); }
  ChannelBuffer<float>* fbuf() { return fbuf(false); }
  const ChannelBuffer<int16_t>* ibuf_const() { return ibuf(true); }
  const ChannelBuffer<float>* fbuf_const() { return fbuf(true); }

 private:
  ChannelBuffer<int16_t>* ibuf(bool readonly) {
    RefreshI();
    fvalid_ = readonly;
    return &ibuf_;
  }

  ChannelBuffer<float>* fbuf(bool readonly) {
    RefreshF();
    ivalid_ = readonly;
    return &fbuf_;
  }

  void RefreshF() {
    if (!fvalid_) {
      assert(ivalid_);
      const int16_t* const int_data = ibuf_.data();
      float* const float_data = fbuf_.data();
      const int length = fbuf_.length();
      for (int i = 0; i < length; ++i)
        float_data[i] = int_data[i];
      fvalid_ = true;
    }
  }

  void RefreshI() {
    if (!ivalid_) {
      assert(fvalid_);
      FloatS16ToS16(fbuf_.data(), ibuf_.length(), ibuf_.data());
      ivalid_ = true;
    }
  }

  bool ivalid_;
  ChannelBuffer<int16_t> ibuf_;
  bool fvalid_;
  ChannelBuffer<float> fbuf_;
};

AudioBuffer::AudioBuffer(int input_samples_per_channel,
                         int num_input_channels,
                         int process_samples_per_channel,
                         int num_process_channels,
                         int output_samples_per_channel)
  : input_samples_per_channel_(input_samples_per_channel),
    num_input_channels_(num_input_channels),
    proc_samples_per_channel_(process_samples_per_channel),
    num_proc_channels_(num_process_channels),
    output_samples_per_channel_(output_samples_per_channel),
    samples_per_split_channel_(proc_samples_per_channel_),
    mixed_low_pass_valid_(false),
    reference_copied_(false),
    activity_(AudioFrame::kVadUnknown),
    keyboard_data_(NULL),
    channels_(new IFChannelBuffer(proc_samples_per_channel_,
                                  num_proc_channels_)) {
  assert(input_samples_per_channel_ > 0);
  assert(proc_samples_per_channel_ > 0);
  assert(output_samples_per_channel_ > 0);
  assert(num_input_channels_ > 0 && num_input_channels_ <= 2);
  assert(num_proc_channels_ <= num_input_channels);

  if (num_input_channels_ == 2 && num_proc_channels_ == 1) {
    input_buffer_.reset(new ChannelBuffer<float>(input_samples_per_channel_,
                                                 num_proc_channels_));
  }

  if (input_samples_per_channel_ != proc_samples_per_channel_ ||
      output_samples_per_channel_ != proc_samples_per_channel_) {
    // Create an intermediate buffer for resampling.
    process_buffer_.reset(new ChannelBuffer<float>(proc_samples_per_channel_,
                                                   num_proc_channels_));
  }

  if (input_samples_per_channel_ != proc_samples_per_channel_) {
    input_resamplers_.reserve(num_proc_channels_);
    for (int i = 0; i < num_proc_channels_; ++i) {
      input_resamplers_.push_back(
          new PushSincResampler(input_samples_per_channel_,
                                proc_samples_per_channel_));
    }
  }

  if (output_samples_per_channel_ != proc_samples_per_channel_) {
    output_resamplers_.reserve(num_proc_channels_);
    for (int i = 0; i < num_proc_channels_; ++i) {
      output_resamplers_.push_back(
          new PushSincResampler(proc_samples_per_channel_,
                                output_samples_per_channel_));
    }
  }

  if (proc_samples_per_channel_ == kSamplesPer32kHzChannel) {
    samples_per_split_channel_ = kSamplesPer16kHzChannel;
    split_channels_low_.reset(new IFChannelBuffer(samples_per_split_channel_,
                                                  num_proc_channels_));
    split_channels_high_.reset(new IFChannelBuffer(samples_per_split_channel_,
                                                   num_proc_channels_));
    filter_states_.reset(new SplitFilterStates[num_proc_channels_]);
  }
}

AudioBuffer::~AudioBuffer() {}

void AudioBuffer::CopyFrom(const float* const* data,
                           int samples_per_channel,
                           AudioProcessing::ChannelLayout layout) {
  assert(samples_per_channel == input_samples_per_channel_);
  assert(ChannelsFromLayout(layout) == num_input_channels_);
  InitForNewData();

  if (HasKeyboardChannel(layout)) {
    keyboard_data_ = data[KeyboardChannelIndex(layout)];
  }

  // Downmix.
  const float* const* data_ptr = data;
  if (num_input_channels_ == 2 && num_proc_channels_ == 1) {
    StereoToMono(data[0],
                 data[1],
                 input_buffer_->channel(0),
                 input_samples_per_channel_);
    data_ptr = input_buffer_->channels();
  }

  // Resample.
  if (input_samples_per_channel_ != proc_samples_per_channel_) {
    for (int i = 0; i < num_proc_channels_; ++i) {
      input_resamplers_[i]->Resample(data_ptr[i],
                                     input_samples_per_channel_,
                                     process_buffer_->channel(i),
                                     proc_samples_per_channel_);
    }
    data_ptr = process_buffer_->channels();
  }

  // Convert to the S16 range.
  for (int i = 0; i < num_proc_channels_; ++i) {
    FloatToFloatS16(data_ptr[i], proc_samples_per_channel_,
                    channels_->fbuf()->channel(i));
  }
}

void AudioBuffer::CopyTo(int samples_per_channel,
                         AudioProcessing::ChannelLayout layout,
                         float* const* data) {
  assert(samples_per_channel == output_samples_per_channel_);
  assert(ChannelsFromLayout(layout) == num_proc_channels_);

  // Convert to the float range.
  float* const* data_ptr = data;
  if (output_samples_per_channel_ != proc_samples_per_channel_) {
    // Convert to an intermediate buffer for subsequent resampling.
    data_ptr = process_buffer_->channels();
  }
  for (int i = 0; i < num_proc_channels_; ++i) {
    FloatS16ToFloat(channels_->fbuf()->channel(i), proc_samples_per_channel_,
                    data_ptr[i]);
  }

  // Resample.
  if (output_samples_per_channel_ != proc_samples_per_channel_) {
    for (int i = 0; i < num_proc_channels_; ++i) {
      output_resamplers_[i]->Resample(data_ptr[i],
                                      proc_samples_per_channel_,
                                      data[i],
                                      output_samples_per_channel_);
    }
  }
}

void AudioBuffer::InitForNewData() {
  keyboard_data_ = NULL;
  mixed_low_pass_valid_ = false;
  reference_copied_ = false;
  activity_ = AudioFrame::kVadUnknown;
}

const int16_t* AudioBuffer::data(int channel) const {
  return channels_->ibuf_const()->channel(channel);
}

int16_t* AudioBuffer::data(int channel) {
  mixed_low_pass_valid_ = false;
  return channels_->ibuf()->channel(channel);
}

const float* AudioBuffer::data_f(int channel) const {
  return channels_->fbuf_const()->channel(channel);
}

float* AudioBuffer::data_f(int channel) {
  mixed_low_pass_valid_ = false;
  return channels_->fbuf()->channel(channel);
}

const float* const* AudioBuffer::channels_f() const {
  return channels_->fbuf_const()->channels();
}

float* const* AudioBuffer::channels_f() {
  mixed_low_pass_valid_ = false;
  return channels_->fbuf()->channels();
}

const int16_t* AudioBuffer::low_pass_split_data(int channel) const {
  return split_channels_low_.get()
      ? split_channels_low_->ibuf_const()->channel(channel)
      : data(channel);
}

int16_t* AudioBuffer::low_pass_split_data(int channel) {
  mixed_low_pass_valid_ = false;
  return split_channels_low_.get()
      ? split_channels_low_->ibuf()->channel(channel)
      : data(channel);
}

const float* AudioBuffer::low_pass_split_data_f(int channel) const {
  return split_channels_low_.get()
      ? split_channels_low_->fbuf_const()->channel(channel)
      : data_f(channel);
}

float* AudioBuffer::low_pass_split_data_f(int channel) {
  mixed_low_pass_valid_ = false;
  return split_channels_low_.get()
      ? split_channels_low_->fbuf()->channel(channel)
      : data_f(channel);
}

const float* const* AudioBuffer::low_pass_split_channels_f() const {
  return split_channels_low_.get()
      ? split_channels_low_->fbuf_const()->channels()
      : channels_f();
}

float* const* AudioBuffer::low_pass_split_channels_f() {
  mixed_low_pass_valid_ = false;
  return split_channels_low_.get()
      ? split_channels_low_->fbuf()->channels()
      : channels_f();
}

const int16_t* AudioBuffer::high_pass_split_data(int channel) const {
  return split_channels_high_.get()
      ? split_channels_high_->ibuf_const()->channel(channel)
      : NULL;
}

int16_t* AudioBuffer::high_pass_split_data(int channel) {
  return split_channels_high_.get()
      ? split_channels_high_->ibuf()->channel(channel)
      : NULL;
}

const float* AudioBuffer::high_pass_split_data_f(int channel) const {
  return split_channels_high_.get()
      ? split_channels_high_->fbuf_const()->channel(channel)
      : NULL;
}

float* AudioBuffer::high_pass_split_data_f(int channel) {
  return split_channels_high_.get()
      ? split_channels_high_->fbuf()->channel(channel)
      : NULL;
}

const float* const* AudioBuffer::high_pass_split_channels_f() const {
  return split_channels_high_.get()
      ? split_channels_high_->fbuf_const()->channels()
      : NULL;
}

float* const* AudioBuffer::high_pass_split_channels_f() {
  return split_channels_high_.get()
      ? split_channels_high_->fbuf()->channels()
      : NULL;
}

const int16_t* AudioBuffer::mixed_low_pass_data() {
  // Currently only mixing stereo to mono is supported.
  assert(num_proc_channels_ == 1 || num_proc_channels_ == 2);

  if (num_proc_channels_ == 1) {
    return low_pass_split_data(0);
  }

  if (!mixed_low_pass_valid_) {
    if (!mixed_low_pass_channels_.get()) {
      mixed_low_pass_channels_.reset(
          new ChannelBuffer<int16_t>(samples_per_split_channel_, 1));
    }
    StereoToMono(low_pass_split_data(0),
                 low_pass_split_data(1),
                 mixed_low_pass_channels_->data(),
                 samples_per_split_channel_);
    mixed_low_pass_valid_ = true;
  }
  return mixed_low_pass_channels_->data();
}

const int16_t* AudioBuffer::low_pass_reference(int channel) const {
  if (!reference_copied_) {
    return NULL;
  }

  return low_pass_reference_channels_->channel(channel);
}

const float* AudioBuffer::keyboard_data() const {
  return keyboard_data_;
}

SplitFilterStates* AudioBuffer::filter_states(int channel) {
  assert(channel >= 0 && channel < num_proc_channels_);
  return &filter_states_[channel];
}

void AudioBuffer::set_activity(AudioFrame::VADActivity activity) {
  activity_ = activity;
}

AudioFrame::VADActivity AudioBuffer::activity() const {
  return activity_;
}

int AudioBuffer::num_channels() const {
  return num_proc_channels_;
}

int AudioBuffer::samples_per_channel() const {
  return proc_samples_per_channel_;
}

int AudioBuffer::samples_per_split_channel() const {
  return samples_per_split_channel_;
}

int AudioBuffer::samples_per_keyboard_channel() const {
  // We don't resample the keyboard channel.
  return input_samples_per_channel_;
}

// TODO(andrew): Do deinterleaving and mixing in one step?
void AudioBuffer::DeinterleaveFrom(AudioFrame* frame) {
  assert(proc_samples_per_channel_ == input_samples_per_channel_);
  assert(frame->num_channels_ == num_input_channels_);
  assert(frame->samples_per_channel_ ==  proc_samples_per_channel_);
  InitForNewData();
  activity_ = frame->vad_activity_;

  if (num_input_channels_ == 2 && num_proc_channels_ == 1) {
    // Downmix directly; no explicit deinterleaving needed.
    int16_t* downmixed = channels_->ibuf()->channel(0);
    for (int i = 0; i < input_samples_per_channel_; ++i) {
      downmixed[i] = (frame->data_[i * 2] + frame->data_[i * 2 + 1]) / 2;
    }
  } else {
    assert(num_proc_channels_ == num_input_channels_);
    int16_t* interleaved = frame->data_;
    for (int i = 0; i < num_proc_channels_; ++i) {
      int16_t* deinterleaved = channels_->ibuf()->channel(i);
      int interleaved_idx = i;
      for (int j = 0; j < proc_samples_per_channel_; ++j) {
        deinterleaved[j] = interleaved[interleaved_idx];
        interleaved_idx += num_proc_channels_;
      }
    }
  }
}

void AudioBuffer::InterleaveTo(AudioFrame* frame, bool data_changed) const {
  assert(proc_samples_per_channel_ == output_samples_per_channel_);
  assert(num_proc_channels_ == num_input_channels_);
  assert(frame->num_channels_ == num_proc_channels_);
  assert(frame->samples_per_channel_ == proc_samples_per_channel_);
  frame->vad_activity_ = activity_;

  if (!data_changed) {
    return;
  }

  int16_t* interleaved = frame->data_;
  for (int i = 0; i < num_proc_channels_; i++) {
    int16_t* deinterleaved = channels_->ibuf()->channel(i);
    int interleaved_idx = i;
    for (int j = 0; j < proc_samples_per_channel_; j++) {
      interleaved[interleaved_idx] = deinterleaved[j];
      interleaved_idx += num_proc_channels_;
    }
  }
}

void AudioBuffer::CopyLowPassToReference() {
  reference_copied_ = true;
  if (!low_pass_reference_channels_.get()) {
    low_pass_reference_channels_.reset(
        new ChannelBuffer<int16_t>(samples_per_split_channel_,
                                   num_proc_channels_));
  }
  for (int i = 0; i < num_proc_channels_; i++) {
    low_pass_reference_channels_->CopyFrom(low_pass_split_data(i), i);
  }
}

}  // namespace webrtc
