/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/codecs/g711/include/audio_encoder_pcm.h"

#include <limits>

#include "webrtc/base/checks.h"
#include "webrtc/modules/audio_coding/codecs/g711/include/g711_interface.h"

namespace webrtc {

namespace {
int16_t NumSamplesPerFrame(int num_channels,
                           int frame_size_ms,
                           int sample_rate_hz) {
  int samples_per_frame = num_channels * frame_size_ms * sample_rate_hz / 1000;
  CHECK_LE(samples_per_frame, std::numeric_limits<int16_t>::max())
      << "Frame size too large.";
  return static_cast<int16_t>(samples_per_frame);
}
}  // namespace

AudioEncoderPcm::AudioEncoderPcm(const Config& config, int sample_rate_hz)
    : sample_rate_hz_(sample_rate_hz),
      num_channels_(config.num_channels),
      payload_type_(config.payload_type),
      num_10ms_frames_per_packet_(config.frame_size_ms / 10),
      full_frame_samples_(NumSamplesPerFrame(config.num_channels,
                                             config.frame_size_ms,
                                             sample_rate_hz_)),
      first_timestamp_in_buffer_(0) {
  CHECK_GT(sample_rate_hz, 0) << "Sample rate must be larger than 0 Hz";
  CHECK_EQ(config.frame_size_ms % 10, 0)
      << "Frame size must be an integer multiple of 10 ms.";
  speech_buffer_.reserve(full_frame_samples_);
}

AudioEncoderPcm::~AudioEncoderPcm() {
}

int AudioEncoderPcm::SampleRateHz() const {
  return sample_rate_hz_;
}

int AudioEncoderPcm::NumChannels() const {
  return num_channels_;
}

size_t AudioEncoderPcm::MaxEncodedBytes() const {
  return full_frame_samples_;
}

int AudioEncoderPcm::Num10MsFramesInNextPacket() const {
  return num_10ms_frames_per_packet_;
}

int AudioEncoderPcm::Max10MsFramesInAPacket() const {
  return num_10ms_frames_per_packet_;
}

AudioEncoder::EncodedInfo AudioEncoderPcm::EncodeInternal(
    uint32_t rtp_timestamp,
    const int16_t* audio,
    size_t max_encoded_bytes,
    uint8_t* encoded) {
  const int num_samples = SampleRateHz() / 100 * NumChannels();
  if (speech_buffer_.empty()) {
    first_timestamp_in_buffer_ = rtp_timestamp;
  }
  for (int i = 0; i < num_samples; ++i) {
    speech_buffer_.push_back(audio[i]);
  }
  if (speech_buffer_.size() < full_frame_samples_) {
    return EncodedInfo();
  }
  CHECK_EQ(speech_buffer_.size(), full_frame_samples_);
  CHECK_GE(max_encoded_bytes, full_frame_samples_);
  int16_t ret = EncodeCall(&speech_buffer_[0], full_frame_samples_, encoded);
  CHECK_GE(ret, 0);
  speech_buffer_.clear();
  EncodedInfo info;
  info.encoded_timestamp = first_timestamp_in_buffer_;
  info.payload_type = payload_type_;
  info.encoded_bytes = static_cast<size_t>(ret);
  return info;
}

int16_t AudioEncoderPcmA::EncodeCall(const int16_t* audio,
                                     size_t input_len,
                                     uint8_t* encoded) {
  return WebRtcG711_EncodeA(audio, static_cast<int16_t>(input_len), encoded);
}

int16_t AudioEncoderPcmU::EncodeCall(const int16_t* audio,
                                     size_t input_len,
                                     uint8_t* encoded) {
  return WebRtcG711_EncodeU(audio, static_cast<int16_t>(input_len), encoded);
}

}  // namespace webrtc
