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

#include "webrtc/common_audio/resampler/push_sinc_resampler.h"
#include "webrtc/common_audio/signal_processing/include/signal_processing_library.h"
#include "webrtc/common_audio/channel_buffer.h"
#include "webrtc/modules/audio_processing/common.h"

namespace webrtc {
namespace {

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
                  int num_frames) {
  for (int i = 0; i < num_frames; ++i)
    out[i] = (left[i] + right[i]) / 2;
}

int NumBandsFromSamplesPerChannel(int num_frames) {
  int num_bands = 1;
  if (num_frames == kSamplesPer32kHzChannel ||
      num_frames == kSamplesPer48kHzChannel) {
    num_bands = rtc::CheckedDivExact(num_frames,
                                     static_cast<int>(kSamplesPer16kHzChannel));
  }
  return num_bands;
}

}  // namespace

AudioBuffer::AudioBuffer(int input_num_frames,
                         int num_input_channels,
                         int process_num_frames,
                         int num_process_channels,
                         int output_num_frames)
  : input_num_frames_(input_num_frames),
    num_input_channels_(num_input_channels),
    proc_num_frames_(process_num_frames),
    num_proc_channels_(num_process_channels),
    output_num_frames_(output_num_frames),
    num_channels_(num_process_channels),
    num_bands_(NumBandsFromSamplesPerChannel(proc_num_frames_)),
    num_split_frames_(rtc::CheckedDivExact(
        proc_num_frames_, num_bands_)),
    mixed_low_pass_valid_(false),
    reference_copied_(false),
    activity_(AudioFrame::kVadUnknown),
    keyboard_data_(NULL),
    data_(new IFChannelBuffer(proc_num_frames_, num_proc_channels_)) {
  assert(input_num_frames_ > 0);
  assert(proc_num_frames_ > 0);
  assert(output_num_frames_ > 0);
  assert(num_input_channels_ > 0 && num_input_channels_ <= 2);
  assert(num_proc_channels_ > 0 && num_proc_channels_ <= num_input_channels_);

  if (num_input_channels_ == 2 && num_proc_channels_ == 1) {
    input_buffer_.reset(new ChannelBuffer<float>(input_num_frames_,
                                                 num_proc_channels_));
  }

  if (input_num_frames_ != proc_num_frames_ ||
      output_num_frames_ != proc_num_frames_) {
    // Create an intermediate buffer for resampling.
    process_buffer_.reset(new ChannelBuffer<float>(proc_num_frames_,
                                                   num_proc_channels_));

    if (input_num_frames_ != proc_num_frames_) {
      for (int i = 0; i < num_proc_channels_; ++i) {
        input_resamplers_.push_back(
            new PushSincResampler(input_num_frames_,
                                  proc_num_frames_));
      }
    }

    if (output_num_frames_ != proc_num_frames_) {
      for (int i = 0; i < num_proc_channels_; ++i) {
        output_resamplers_.push_back(
            new PushSincResampler(proc_num_frames_,
                                  output_num_frames_));
      }
    }
  }

  if (num_bands_ > 1) {
    split_data_.reset(new IFChannelBuffer(proc_num_frames_,
                                          num_proc_channels_,
                                          num_bands_));
    splitting_filter_.reset(new SplittingFilter(num_proc_channels_));
  }
}

AudioBuffer::~AudioBuffer() {}

void AudioBuffer::CopyFrom(const float* const* data,
                           int num_frames,
                           AudioProcessing::ChannelLayout layout) {
  assert(num_frames == input_num_frames_);
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
                 input_buffer_->channels()[0],
                 input_num_frames_);
    data_ptr = input_buffer_->channels();
  }

  // Resample.
  if (input_num_frames_ != proc_num_frames_) {
    for (int i = 0; i < num_proc_channels_; ++i) {
      input_resamplers_[i]->Resample(data_ptr[i],
                                     input_num_frames_,
                                     process_buffer_->channels()[i],
                                     proc_num_frames_);
    }
    data_ptr = process_buffer_->channels();
  }

  // Convert to the S16 range.
  for (int i = 0; i < num_proc_channels_; ++i) {
    FloatToFloatS16(data_ptr[i],
                    proc_num_frames_,
                    data_->fbuf()->channels()[i]);
  }
}

void AudioBuffer::CopyTo(int num_frames,
                         AudioProcessing::ChannelLayout layout,
                         float* const* data) {
  assert(num_frames == output_num_frames_);
  assert(ChannelsFromLayout(layout) == num_channels_);

  // Convert to the float range.
  float* const* data_ptr = data;
  if (output_num_frames_ != proc_num_frames_) {
    // Convert to an intermediate buffer for subsequent resampling.
    data_ptr = process_buffer_->channels();
  }
  for (int i = 0; i < num_channels_; ++i) {
    FloatS16ToFloat(data_->fbuf()->channels()[i],
                    proc_num_frames_,
                    data_ptr[i]);
  }

  // Resample.
  if (output_num_frames_ != proc_num_frames_) {
    for (int i = 0; i < num_channels_; ++i) {
      output_resamplers_[i]->Resample(data_ptr[i],
                                      proc_num_frames_,
                                      data[i],
                                      output_num_frames_);
    }
  }
}

void AudioBuffer::InitForNewData() {
  keyboard_data_ = NULL;
  mixed_low_pass_valid_ = false;
  reference_copied_ = false;
  activity_ = AudioFrame::kVadUnknown;
  num_channels_ = num_proc_channels_;
}

const int16_t* const* AudioBuffer::channels_const() const {
  return data_->ibuf_const()->channels();
}

int16_t* const* AudioBuffer::channels() {
  mixed_low_pass_valid_ = false;
  return data_->ibuf()->channels();
}

const int16_t* const* AudioBuffer::split_bands_const(int channel) const {
  return split_data_.get() ?
         split_data_->ibuf_const()->bands(channel) :
         data_->ibuf_const()->bands(channel);
}

int16_t* const* AudioBuffer::split_bands(int channel) {
  mixed_low_pass_valid_ = false;
  return split_data_.get() ?
         split_data_->ibuf()->bands(channel) :
         data_->ibuf()->bands(channel);
}

const int16_t* const* AudioBuffer::split_channels_const(Band band) const {
  if (split_data_.get()) {
    return split_data_->ibuf_const()->channels(band);
  } else {
    return band == kBand0To8kHz ? data_->ibuf_const()->channels() : nullptr;
  }
}

int16_t* const* AudioBuffer::split_channels(Band band) {
  mixed_low_pass_valid_ = false;
  if (split_data_.get()) {
    return split_data_->ibuf()->channels(band);
  } else {
    return band == kBand0To8kHz ? data_->ibuf()->channels() : nullptr;
  }
}

ChannelBuffer<int16_t>* AudioBuffer::data() {
  mixed_low_pass_valid_ = false;
  return data_->ibuf();
}

const ChannelBuffer<int16_t>* AudioBuffer::data() const {
  return data_->ibuf_const();
}

ChannelBuffer<int16_t>* AudioBuffer::split_data() {
  mixed_low_pass_valid_ = false;
  return split_data_.get() ? split_data_->ibuf() : data_->ibuf();
}

const ChannelBuffer<int16_t>* AudioBuffer::split_data() const {
  return split_data_.get() ? split_data_->ibuf_const() : data_->ibuf_const();
}

const float* const* AudioBuffer::channels_const_f() const {
  return data_->fbuf_const()->channels();
}

float* const* AudioBuffer::channels_f() {
  mixed_low_pass_valid_ = false;
  return data_->fbuf()->channels();
}

const float* const* AudioBuffer::split_bands_const_f(int channel) const {
  return split_data_.get() ?
         split_data_->fbuf_const()->bands(channel) :
         data_->fbuf_const()->bands(channel);
}

float* const* AudioBuffer::split_bands_f(int channel) {
  mixed_low_pass_valid_ = false;
  return split_data_.get() ?
         split_data_->fbuf()->bands(channel) :
         data_->fbuf()->bands(channel);
}

const float* const* AudioBuffer::split_channels_const_f(Band band) const {
  if (split_data_.get()) {
    return split_data_->fbuf_const()->channels(band);
  } else {
    return band == kBand0To8kHz ? data_->fbuf_const()->channels() : nullptr;
  }
}

float* const* AudioBuffer::split_channels_f(Band band) {
  mixed_low_pass_valid_ = false;
  if (split_data_.get()) {
    return split_data_->fbuf()->channels(band);
  } else {
    return band == kBand0To8kHz ? data_->fbuf()->channels() : nullptr;
  }
}

ChannelBuffer<float>* AudioBuffer::data_f() {
  mixed_low_pass_valid_ = false;
  return data_->fbuf();
}

const ChannelBuffer<float>* AudioBuffer::data_f() const {
  return data_->fbuf_const();
}

ChannelBuffer<float>* AudioBuffer::split_data_f() {
  mixed_low_pass_valid_ = false;
  return split_data_.get() ? split_data_->fbuf() : data_->fbuf();
}

const ChannelBuffer<float>* AudioBuffer::split_data_f() const {
  return split_data_.get() ? split_data_->fbuf_const() : data_->fbuf_const();
}

const int16_t* AudioBuffer::mixed_low_pass_data() {
  // Currently only mixing stereo to mono is supported.
  assert(num_proc_channels_ == 1 || num_proc_channels_ == 2);

  if (num_proc_channels_ == 1) {
    return split_bands_const(0)[kBand0To8kHz];
  }

  if (!mixed_low_pass_valid_) {
    if (!mixed_low_pass_channels_.get()) {
      mixed_low_pass_channels_.reset(
          new ChannelBuffer<int16_t>(num_split_frames_, 1));
    }
    StereoToMono(split_bands_const(0)[kBand0To8kHz],
                 split_bands_const(1)[kBand0To8kHz],
                 mixed_low_pass_channels_->channels()[0],
                 num_split_frames_);
    mixed_low_pass_valid_ = true;
  }
  return mixed_low_pass_channels_->channels()[0];
}

const int16_t* AudioBuffer::low_pass_reference(int channel) const {
  if (!reference_copied_) {
    return NULL;
  }

  return low_pass_reference_channels_->channels()[channel];
}

const float* AudioBuffer::keyboard_data() const {
  return keyboard_data_;
}

void AudioBuffer::set_activity(AudioFrame::VADActivity activity) {
  activity_ = activity;
}

AudioFrame::VADActivity AudioBuffer::activity() const {
  return activity_;
}

int AudioBuffer::num_channels() const {
  return num_channels_;
}

void AudioBuffer::set_num_channels(int num_channels) {
  num_channels_ = num_channels;
}

int AudioBuffer::num_frames() const {
  return proc_num_frames_;
}

int AudioBuffer::num_frames_per_band() const {
  return num_split_frames_;
}

int AudioBuffer::num_keyboard_frames() const {
  // We don't resample the keyboard channel.
  return input_num_frames_;
}

int AudioBuffer::num_bands() const {
  return num_bands_;
}

// TODO(andrew): Do deinterleaving and mixing in one step?
void AudioBuffer::DeinterleaveFrom(AudioFrame* frame) {
  assert(proc_num_frames_ == input_num_frames_);
  assert(frame->num_channels_ == num_input_channels_);
  assert(frame->samples_per_channel_ ==  proc_num_frames_);
  InitForNewData();
  activity_ = frame->vad_activity_;

  if (num_input_channels_ == 2 && num_proc_channels_ == 1) {
    // Downmix directly; no explicit deinterleaving needed.
    int16_t* downmixed = data_->ibuf()->channels()[0];
    for (int i = 0; i < input_num_frames_; ++i) {
      downmixed[i] = (frame->data_[i * 2] + frame->data_[i * 2 + 1]) / 2;
    }
  } else {
    assert(num_proc_channels_ == num_input_channels_);
    int16_t* interleaved = frame->data_;
    for (int i = 0; i < num_proc_channels_; ++i) {
      int16_t* deinterleaved = data_->ibuf()->channels()[i];
      int interleaved_idx = i;
      for (int j = 0; j < proc_num_frames_; ++j) {
        deinterleaved[j] = interleaved[interleaved_idx];
        interleaved_idx += num_proc_channels_;
      }
    }
  }
}

void AudioBuffer::InterleaveTo(AudioFrame* frame, bool data_changed) const {
  assert(proc_num_frames_ == output_num_frames_);
  assert(num_channels_ == num_input_channels_);
  assert(frame->num_channels_ == num_channels_);
  assert(frame->samples_per_channel_ == proc_num_frames_);
  frame->vad_activity_ = activity_;

  if (!data_changed) {
    return;
  }

  int16_t* interleaved = frame->data_;
  for (int i = 0; i < num_channels_; i++) {
    int16_t* deinterleaved = data_->ibuf()->channels()[i];
    int interleaved_idx = i;
    for (int j = 0; j < proc_num_frames_; j++) {
      interleaved[interleaved_idx] = deinterleaved[j];
      interleaved_idx += num_channels_;
    }
  }
}

void AudioBuffer::CopyLowPassToReference() {
  reference_copied_ = true;
  if (!low_pass_reference_channels_.get() ||
      low_pass_reference_channels_->num_channels() != num_channels_) {
    low_pass_reference_channels_.reset(
        new ChannelBuffer<int16_t>(num_split_frames_,
                                   num_proc_channels_));
  }
  for (int i = 0; i < num_proc_channels_; i++) {
    memcpy(low_pass_reference_channels_->channels()[i],
           split_bands_const(i)[kBand0To8kHz],
           low_pass_reference_channels_->num_frames_per_band() *
               sizeof(split_bands_const(i)[kBand0To8kHz][0]));
  }
}

void AudioBuffer::SplitIntoFrequencyBands() {
  splitting_filter_->Analysis(data_.get(), split_data_.get());
}

void AudioBuffer::MergeFrequencyBands() {
  splitting_filter_->Synthesis(split_data_.get(), data_.get());
}

}  // namespace webrtc
