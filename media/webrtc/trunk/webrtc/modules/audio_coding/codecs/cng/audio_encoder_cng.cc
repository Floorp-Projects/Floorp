/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/codecs/cng/include/audio_encoder_cng.h"

#include <algorithm>
#include <limits>

namespace webrtc {

namespace {

const int kMaxFrameSizeMs = 60;

}  // namespace

AudioEncoderCng::Config::Config()
    : num_channels(1),
      payload_type(13),
      speech_encoder(NULL),
      vad_mode(Vad::kVadNormal),
      sid_frame_interval_ms(100),
      num_cng_coefficients(8),
      vad(NULL) {
}

bool AudioEncoderCng::Config::IsOk() const {
  if (num_channels != 1)
    return false;
  if (!speech_encoder)
    return false;
  if (num_channels != speech_encoder->NumChannels())
    return false;
  if (sid_frame_interval_ms < speech_encoder->Max10MsFramesInAPacket() * 10)
    return false;
  if (num_cng_coefficients > WEBRTC_CNG_MAX_LPC_ORDER ||
      num_cng_coefficients <= 0)
    return false;
  return true;
}

AudioEncoderCng::AudioEncoderCng(const Config& config)
    : speech_encoder_(config.speech_encoder),
      cng_payload_type_(config.payload_type),
      num_cng_coefficients_(config.num_cng_coefficients),
      first_timestamp_in_buffer_(0),
      frames_in_buffer_(0),
      last_frame_active_(true),
      vad_(new Vad(config.vad_mode)) {
  if (config.vad) {
    // Replace default Vad object with user-provided one.
    vad_.reset(config.vad);
  }
  CHECK(config.IsOk()) << "Invalid configuration.";
  CNG_enc_inst* cng_inst;
  CHECK_EQ(WebRtcCng_CreateEnc(&cng_inst), 0) << "WebRtcCng_CreateEnc failed.";
  cng_inst_.reset(cng_inst);  // Transfer ownership to scoped_ptr.
  CHECK_EQ(WebRtcCng_InitEnc(cng_inst_.get(), SampleRateHz(),
                             config.sid_frame_interval_ms,
                             config.num_cng_coefficients),
           0)
      << "WebRtcCng_InitEnc failed";
}

AudioEncoderCng::~AudioEncoderCng() {
}

int AudioEncoderCng::SampleRateHz() const {
  return speech_encoder_->SampleRateHz();
}

int AudioEncoderCng::RtpTimestampRateHz() const {
  return speech_encoder_->RtpTimestampRateHz();
}

int AudioEncoderCng::NumChannels() const {
  return 1;
}

size_t AudioEncoderCng::MaxEncodedBytes() const {
  const size_t max_encoded_bytes_active = speech_encoder_->MaxEncodedBytes();
  const size_t max_encoded_bytes_passive =
      rtc::CheckedDivExact(kMaxFrameSizeMs, 10) * SamplesPer10msFrame();
  return std::max(max_encoded_bytes_active, max_encoded_bytes_passive);
}

int AudioEncoderCng::Num10MsFramesInNextPacket() const {
  return speech_encoder_->Num10MsFramesInNextPacket();
}

int AudioEncoderCng::Max10MsFramesInAPacket() const {
  return speech_encoder_->Max10MsFramesInAPacket();
}

void AudioEncoderCng::SetTargetBitrate(int bits_per_second) {
  speech_encoder_->SetTargetBitrate(bits_per_second);
}

void AudioEncoderCng::SetProjectedPacketLossRate(double fraction) {
  DCHECK_GE(fraction, 0.0);
  DCHECK_LE(fraction, 1.0);
  speech_encoder_->SetProjectedPacketLossRate(fraction);
}

AudioEncoder::EncodedInfo AudioEncoderCng::EncodeInternal(
    uint32_t rtp_timestamp,
    const int16_t* audio,
    size_t max_encoded_bytes,
    uint8_t* encoded) {
  CHECK_GE(max_encoded_bytes, static_cast<size_t>(num_cng_coefficients_ + 1));
  const int num_samples = SampleRateHz() / 100 * NumChannels();
  if (speech_buffer_.empty()) {
    CHECK_EQ(frames_in_buffer_, 0);
    first_timestamp_in_buffer_ = rtp_timestamp;
  }
  for (int i = 0; i < num_samples; ++i) {
    speech_buffer_.push_back(audio[i]);
  }
  ++frames_in_buffer_;
  if (frames_in_buffer_ < speech_encoder_->Num10MsFramesInNextPacket()) {
    return EncodedInfo();
  }
  CHECK_LE(frames_in_buffer_ * 10, kMaxFrameSizeMs)
      << "Frame size cannot be larger than " << kMaxFrameSizeMs
      << " ms when using VAD/CNG.";
  const size_t samples_per_10ms_frame = 10 * SampleRateHz() / 1000;
  CHECK_EQ(speech_buffer_.size(),
           static_cast<size_t>(frames_in_buffer_) * samples_per_10ms_frame);

  // Group several 10 ms blocks per VAD call. Call VAD once or twice using the
  // following split sizes:
  // 10 ms = 10 + 0 ms; 20 ms = 20 + 0 ms; 30 ms = 30 + 0 ms;
  // 40 ms = 20 + 20 ms; 50 ms = 30 + 20 ms; 60 ms = 30 + 30 ms.
  int blocks_in_first_vad_call =
      (frames_in_buffer_ > 3 ? 3 : frames_in_buffer_);
  if (frames_in_buffer_ == 4)
    blocks_in_first_vad_call = 2;
  const int blocks_in_second_vad_call =
      frames_in_buffer_ - blocks_in_first_vad_call;
  CHECK_GE(blocks_in_second_vad_call, 0);

  // Check if all of the buffer is passive speech. Start with checking the first
  // block.
  Vad::Activity activity = vad_->VoiceActivity(
      &speech_buffer_[0], samples_per_10ms_frame * blocks_in_first_vad_call,
      SampleRateHz());
  if (activity == Vad::kPassive && blocks_in_second_vad_call > 0) {
    // Only check the second block if the first was passive.
    activity = vad_->VoiceActivity(
        &speech_buffer_[samples_per_10ms_frame * blocks_in_first_vad_call],
        samples_per_10ms_frame * blocks_in_second_vad_call, SampleRateHz());
  }

  EncodedInfo info;
  switch (activity) {
    case Vad::kPassive: {
      info = EncodePassive(max_encoded_bytes, encoded);
      last_frame_active_ = false;
      break;
    }
    case Vad::kActive: {
      info = EncodeActive(max_encoded_bytes, encoded);
      last_frame_active_ = true;
      break;
    }
    case Vad::kError: {
      FATAL();  // Fails only if fed invalid data.
      break;
    }
  }

  speech_buffer_.clear();
  frames_in_buffer_ = 0;
  return info;
}

AudioEncoder::EncodedInfo AudioEncoderCng::EncodePassive(
    size_t max_encoded_bytes,
    uint8_t* encoded) {
  bool force_sid = last_frame_active_;
  bool output_produced = false;
  const size_t samples_per_10ms_frame = SamplesPer10msFrame();
  CHECK_GE(max_encoded_bytes, frames_in_buffer_ * samples_per_10ms_frame);
  AudioEncoder::EncodedInfo info;
  for (int i = 0; i < frames_in_buffer_; ++i) {
    int16_t encoded_bytes_tmp = 0;
    CHECK_GE(WebRtcCng_Encode(cng_inst_.get(),
                              &speech_buffer_[i * samples_per_10ms_frame],
                              static_cast<int16_t>(samples_per_10ms_frame),
                              encoded, &encoded_bytes_tmp, force_sid), 0);
    if (encoded_bytes_tmp > 0) {
      CHECK(!output_produced);
      info.encoded_bytes = static_cast<size_t>(encoded_bytes_tmp);
      output_produced = true;
      force_sid = false;
    }
  }
  info.encoded_timestamp = first_timestamp_in_buffer_;
  info.payload_type = cng_payload_type_;
  info.send_even_if_empty = true;
  info.speech = false;
  return info;
}

AudioEncoder::EncodedInfo AudioEncoderCng::EncodeActive(
    size_t max_encoded_bytes,
    uint8_t* encoded) {
  const size_t samples_per_10ms_frame = SamplesPer10msFrame();
  AudioEncoder::EncodedInfo info;
  for (int i = 0; i < frames_in_buffer_; ++i) {
    info = speech_encoder_->Encode(
        first_timestamp_in_buffer_, &speech_buffer_[i * samples_per_10ms_frame],
        samples_per_10ms_frame, max_encoded_bytes, encoded);
    if (i < frames_in_buffer_ - 1) {
      CHECK_EQ(info.encoded_bytes, 0u) << "Encoder delivered data too early.";
    }
  }
  return info;
}

size_t AudioEncoderCng::SamplesPer10msFrame() const {
  return rtc::CheckedDivExact(10 * SampleRateHz(), 1000);
}

}  // namespace webrtc
