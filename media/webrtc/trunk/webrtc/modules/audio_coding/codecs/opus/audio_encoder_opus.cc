/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/codecs/opus/interface/audio_encoder_opus.h"

#include "webrtc/modules/audio_coding/codecs/opus/interface/opus_interface.h"

namespace webrtc {

namespace {

// We always encode at 48 kHz.
const int kSampleRateHz = 48000;

int DivExact(int a, int b) {
  CHECK_EQ(a % b, 0);
  return a / b;
}

int16_t ClampInt16(size_t x) {
  return static_cast<int16_t>(
      std::min(x, static_cast<size_t>(std::numeric_limits<int16_t>::max())));
}

int16_t CastInt16(size_t x) {
  DCHECK_LE(x, static_cast<size_t>(std::numeric_limits<int16_t>::max()));
  return static_cast<int16_t>(x);
}

}  // namespace

AudioEncoderOpus::Config::Config() : frame_size_ms(20), num_channels(1) {}

bool AudioEncoderOpus::Config::IsOk() const {
  if (frame_size_ms <= 0 || frame_size_ms % 10 != 0)
    return false;
  if (num_channels <= 0)
    return false;
  return true;
}

AudioEncoderOpus::AudioEncoderOpus(const Config& config)
    : num_10ms_frames_per_packet_(DivExact(config.frame_size_ms, 10)),
      num_channels_(config.num_channels),
      samples_per_10ms_frame_(DivExact(kSampleRateHz, 100) * num_channels_) {
  CHECK(config.IsOk());
  input_buffer_.reserve(num_10ms_frames_per_packet_ * samples_per_10ms_frame_);
  CHECK_EQ(0, WebRtcOpus_EncoderCreate(&inst_, num_channels_));
}

AudioEncoderOpus::~AudioEncoderOpus() {
  CHECK_EQ(0, WebRtcOpus_EncoderFree(inst_));
}

int AudioEncoderOpus::sample_rate_hz() const {
  return kSampleRateHz;
}

int AudioEncoderOpus::num_channels() const {
  return num_channels_;
}

int AudioEncoderOpus::Num10MsFramesInNextPacket() const {
  return num_10ms_frames_per_packet_;
}

bool AudioEncoderOpus::Encode(uint32_t timestamp,
                              const int16_t* audio,
                              size_t max_encoded_bytes,
                              uint8_t* encoded,
                              size_t* encoded_bytes,
                              uint32_t* encoded_timestamp) {
  if (input_buffer_.empty())
    first_timestamp_in_buffer_ = timestamp;
  input_buffer_.insert(input_buffer_.end(), audio,
                       audio + samples_per_10ms_frame_);
  if (input_buffer_.size() < (static_cast<size_t>(num_10ms_frames_per_packet_) *
                              samples_per_10ms_frame_)) {
    *encoded_bytes = 0;
    return true;
  }
  CHECK_EQ(input_buffer_.size(),
           static_cast<size_t>(num_10ms_frames_per_packet_) *
           samples_per_10ms_frame_);
  int16_t r = WebRtcOpus_Encode(
      inst_, &input_buffer_[0],
      DivExact(CastInt16(input_buffer_.size()), num_channels_),
      ClampInt16(max_encoded_bytes), encoded);
  input_buffer_.clear();
  if (r < 0)
    return false;
  *encoded_bytes = r;
  *encoded_timestamp = first_timestamp_in_buffer_;
  return true;
}

}  // namespace webrtc
