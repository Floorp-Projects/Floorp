/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/codecs/g722/include/audio_encoder_g722.h"

#include <limits>
#include "webrtc/base/checks.h"
#include "webrtc/modules/audio_coding/codecs/g722/include/g722_interface.h"

namespace webrtc {

namespace {

const int kSampleRateHz = 16000;

}  // namespace

AudioEncoderG722::EncoderState::EncoderState() {
  CHECK_EQ(0, WebRtcG722_CreateEncoder(&encoder));
  CHECK_EQ(0, WebRtcG722_EncoderInit(encoder));
}

AudioEncoderG722::EncoderState::~EncoderState() {
  CHECK_EQ(0, WebRtcG722_FreeEncoder(encoder));
}

AudioEncoderG722::AudioEncoderG722(const Config& config)
    : num_channels_(config.num_channels),
      payload_type_(config.payload_type),
      num_10ms_frames_per_packet_(config.frame_size_ms / 10),
      num_10ms_frames_buffered_(0),
      first_timestamp_in_buffer_(0),
      encoders_(new EncoderState[num_channels_]),
      interleave_buffer_(new uint8_t[2 * num_channels_]) {
  CHECK_EQ(config.frame_size_ms % 10, 0)
      << "Frame size must be an integer multiple of 10 ms.";
  const int samples_per_channel =
      kSampleRateHz / 100 * num_10ms_frames_per_packet_;
  for (int i = 0; i < num_channels_; ++i) {
    encoders_[i].speech_buffer.reset(new int16_t[samples_per_channel]);
    encoders_[i].encoded_buffer.reset(new uint8_t[samples_per_channel / 2]);
  }
}

AudioEncoderG722::~AudioEncoderG722() {}

int AudioEncoderG722::SampleRateHz() const {
  return kSampleRateHz;
}

int AudioEncoderG722::RtpTimestampRateHz() const {
  // The RTP timestamp rate for G.722 is 8000 Hz, even though it is a 16 kHz
  // codec.
  return kSampleRateHz / 2;
}

int AudioEncoderG722::NumChannels() const {
  return num_channels_;
}

size_t AudioEncoderG722::MaxEncodedBytes() const {
  return static_cast<size_t>(SamplesPerChannel() / 2 * num_channels_);
}

int AudioEncoderG722::Num10MsFramesInNextPacket() const {
  return num_10ms_frames_per_packet_;
}

int AudioEncoderG722::Max10MsFramesInAPacket() const {
  return num_10ms_frames_per_packet_;
}

AudioEncoder::EncodedInfo AudioEncoderG722::EncodeInternal(
    uint32_t rtp_timestamp,
    const int16_t* audio,
    size_t max_encoded_bytes,
    uint8_t* encoded) {
  CHECK_GE(max_encoded_bytes, MaxEncodedBytes());

  if (num_10ms_frames_buffered_ == 0)
    first_timestamp_in_buffer_ = rtp_timestamp;

  // Deinterleave samples and save them in each channel's buffer.
  const int start = kSampleRateHz / 100 * num_10ms_frames_buffered_;
  for (int i = 0; i < kSampleRateHz / 100; ++i)
    for (int j = 0; j < num_channels_; ++j)
      encoders_[j].speech_buffer[start + i] = audio[i * num_channels_ + j];

  // If we don't yet have enough samples for a packet, we're done for now.
  if (++num_10ms_frames_buffered_ < num_10ms_frames_per_packet_) {
    return EncodedInfo();
  }

  // Encode each channel separately.
  CHECK_EQ(num_10ms_frames_buffered_, num_10ms_frames_per_packet_);
  num_10ms_frames_buffered_ = 0;
  const int samples_per_channel = SamplesPerChannel();
  for (int i = 0; i < num_channels_; ++i) {
    const int encoded = WebRtcG722_Encode(
        encoders_[i].encoder, encoders_[i].speech_buffer.get(),
        samples_per_channel, encoders_[i].encoded_buffer.get());
    CHECK_GE(encoded, 0);
    CHECK_EQ(encoded, samples_per_channel / 2);
  }

  // Interleave the encoded bytes of the different channels. Each separate
  // channel and the interleaved stream encodes two samples per byte, most
  // significant half first.
  for (int i = 0; i < samples_per_channel / 2; ++i) {
    for (int j = 0; j < num_channels_; ++j) {
      uint8_t two_samples = encoders_[j].encoded_buffer[i];
      interleave_buffer_[j] = two_samples >> 4;
      interleave_buffer_[num_channels_ + j] = two_samples & 0xf;
    }
    for (int j = 0; j < num_channels_; ++j)
      encoded[i * num_channels_ + j] =
          interleave_buffer_[2 * j] << 4 | interleave_buffer_[2 * j + 1];
  }
  EncodedInfo info;
  info.encoded_bytes = samples_per_channel / 2 * num_channels_;
  info.encoded_timestamp = first_timestamp_in_buffer_;
  info.payload_type = payload_type_;
  return info;
}

int AudioEncoderG722::SamplesPerChannel() const {
  return kSampleRateHz / 100 * num_10ms_frames_per_packet_;
}

}  // namespace webrtc
