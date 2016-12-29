/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/codecs/cng/audio_encoder_cng.h"

#include <algorithm>
#include <limits>

namespace webrtc {

namespace {

const int kMaxFrameSizeMs = 60;

rtc::scoped_ptr<CNG_enc_inst, CngInstDeleter> CreateCngInst(
    int sample_rate_hz,
    int sid_frame_interval_ms,
    int num_cng_coefficients) {
  rtc::scoped_ptr<CNG_enc_inst, CngInstDeleter> cng_inst;
  RTC_CHECK_EQ(0, WebRtcCng_CreateEnc(cng_inst.accept()));
  RTC_CHECK_EQ(0,
               WebRtcCng_InitEnc(cng_inst.get(), sample_rate_hz,
                                 sid_frame_interval_ms, num_cng_coefficients));
  return cng_inst;
}

}  // namespace

bool AudioEncoderCng::Config::IsOk() const {
  if (num_channels != 1)
    return false;
  if (!speech_encoder)
    return false;
  if (num_channels != speech_encoder->NumChannels())
    return false;
  if (sid_frame_interval_ms <
      static_cast<int>(speech_encoder->Max10MsFramesInAPacket() * 10))
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
      sid_frame_interval_ms_(config.sid_frame_interval_ms),
      last_frame_active_(true),
      vad_(config.vad ? rtc_make_scoped_ptr(config.vad)
                      : CreateVad(config.vad_mode)) {
  RTC_CHECK(config.IsOk()) << "Invalid configuration.";
  cng_inst_ = CreateCngInst(SampleRateHz(), sid_frame_interval_ms_,
                            num_cng_coefficients_);
}

AudioEncoderCng::~AudioEncoderCng() = default;

size_t AudioEncoderCng::MaxEncodedBytes() const {
  const size_t max_encoded_bytes_active = speech_encoder_->MaxEncodedBytes();
  const size_t max_encoded_bytes_passive =
      rtc::CheckedDivExact(kMaxFrameSizeMs, 10) * SamplesPer10msFrame();
  return std::max(max_encoded_bytes_active, max_encoded_bytes_passive);
}

int AudioEncoderCng::SampleRateHz() const {
  return speech_encoder_->SampleRateHz();
}

size_t AudioEncoderCng::NumChannels() const {
  return 1;
}

int AudioEncoderCng::RtpTimestampRateHz() const {
  return speech_encoder_->RtpTimestampRateHz();
}

size_t AudioEncoderCng::Num10MsFramesInNextPacket() const {
  return speech_encoder_->Num10MsFramesInNextPacket();
}

size_t AudioEncoderCng::Max10MsFramesInAPacket() const {
  return speech_encoder_->Max10MsFramesInAPacket();
}

int AudioEncoderCng::GetTargetBitrate() const {
  return speech_encoder_->GetTargetBitrate();
}

AudioEncoder::EncodedInfo AudioEncoderCng::EncodeInternal(
    uint32_t rtp_timestamp,
    rtc::ArrayView<const int16_t> audio,
    size_t max_encoded_bytes,
    uint8_t* encoded) {
  RTC_CHECK_GE(max_encoded_bytes,
               static_cast<size_t>(num_cng_coefficients_ + 1));
  const size_t samples_per_10ms_frame = SamplesPer10msFrame();
  RTC_CHECK_EQ(speech_buffer_.size(),
               rtp_timestamps_.size() * samples_per_10ms_frame);
  rtp_timestamps_.push_back(rtp_timestamp);
  RTC_DCHECK_EQ(samples_per_10ms_frame, audio.size());
  speech_buffer_.insert(speech_buffer_.end(), audio.cbegin(), audio.cend());
  const size_t frames_to_encode = speech_encoder_->Num10MsFramesInNextPacket();
  if (rtp_timestamps_.size() < frames_to_encode) {
    return EncodedInfo();
  }
  RTC_CHECK_LE(static_cast<int>(frames_to_encode * 10), kMaxFrameSizeMs)
      << "Frame size cannot be larger than " << kMaxFrameSizeMs
      << " ms when using VAD/CNG.";

  // Group several 10 ms blocks per VAD call. Call VAD once or twice using the
  // following split sizes:
  // 10 ms = 10 + 0 ms; 20 ms = 20 + 0 ms; 30 ms = 30 + 0 ms;
  // 40 ms = 20 + 20 ms; 50 ms = 30 + 20 ms; 60 ms = 30 + 30 ms.
  size_t blocks_in_first_vad_call =
      (frames_to_encode > 3 ? 3 : frames_to_encode);
  if (frames_to_encode == 4)
    blocks_in_first_vad_call = 2;
  RTC_CHECK_GE(frames_to_encode, blocks_in_first_vad_call);
  const size_t blocks_in_second_vad_call =
      frames_to_encode - blocks_in_first_vad_call;

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
      info = EncodePassive(frames_to_encode, max_encoded_bytes, encoded);
      last_frame_active_ = false;
      break;
    }
    case Vad::kActive: {
      info = EncodeActive(frames_to_encode, max_encoded_bytes, encoded);
      last_frame_active_ = true;
      break;
    }
    case Vad::kError: {
      FATAL();  // Fails only if fed invalid data.
      break;
    }
  }

  speech_buffer_.erase(
      speech_buffer_.begin(),
      speech_buffer_.begin() + frames_to_encode * samples_per_10ms_frame);
  rtp_timestamps_.erase(rtp_timestamps_.begin(),
                        rtp_timestamps_.begin() + frames_to_encode);
  return info;
}

void AudioEncoderCng::Reset() {
  speech_encoder_->Reset();
  speech_buffer_.clear();
  rtp_timestamps_.clear();
  last_frame_active_ = true;
  vad_->Reset();
  cng_inst_ = CreateCngInst(SampleRateHz(), sid_frame_interval_ms_,
                            num_cng_coefficients_);
}

bool AudioEncoderCng::SetFec(bool enable) {
  return speech_encoder_->SetFec(enable);
}

bool AudioEncoderCng::SetDtx(bool enable) {
  return speech_encoder_->SetDtx(enable);
}

bool AudioEncoderCng::SetApplication(Application application) {
  return speech_encoder_->SetApplication(application);
}

void AudioEncoderCng::SetMaxPlaybackRate(int frequency_hz) {
  speech_encoder_->SetMaxPlaybackRate(frequency_hz);
}

void AudioEncoderCng::SetProjectedPacketLossRate(double fraction) {
  speech_encoder_->SetProjectedPacketLossRate(fraction);
}

void AudioEncoderCng::SetTargetBitrate(int bits_per_second) {
  speech_encoder_->SetTargetBitrate(bits_per_second);
}

AudioEncoder::EncodedInfo AudioEncoderCng::EncodePassive(
    size_t frames_to_encode,
    size_t max_encoded_bytes,
    uint8_t* encoded) {
  bool force_sid = last_frame_active_;
  bool output_produced = false;
  const size_t samples_per_10ms_frame = SamplesPer10msFrame();
  RTC_CHECK_GE(max_encoded_bytes, frames_to_encode * samples_per_10ms_frame);
  AudioEncoder::EncodedInfo info;
  for (size_t i = 0; i < frames_to_encode; ++i) {
    // It's important not to pass &info.encoded_bytes directly to
    // WebRtcCng_Encode(), since later loop iterations may return zero in that
    // value, in which case we don't want to overwrite any value from an earlier
    // iteration.
    size_t encoded_bytes_tmp = 0;
    RTC_CHECK_GE(WebRtcCng_Encode(cng_inst_.get(),
                                  &speech_buffer_[i * samples_per_10ms_frame],
                                  samples_per_10ms_frame, encoded,
                                  &encoded_bytes_tmp, force_sid),
                 0);
    if (encoded_bytes_tmp > 0) {
      RTC_CHECK(!output_produced);
      info.encoded_bytes = encoded_bytes_tmp;
      output_produced = true;
      force_sid = false;
    }
  }
  info.encoded_timestamp = rtp_timestamps_.front();
  info.payload_type = cng_payload_type_;
  info.send_even_if_empty = true;
  info.speech = false;
  return info;
}

AudioEncoder::EncodedInfo AudioEncoderCng::EncodeActive(
    size_t frames_to_encode,
    size_t max_encoded_bytes,
    uint8_t* encoded) {
  const size_t samples_per_10ms_frame = SamplesPer10msFrame();
  AudioEncoder::EncodedInfo info;
  for (size_t i = 0; i < frames_to_encode; ++i) {
    info =
        speech_encoder_->Encode(rtp_timestamps_.front(),
                                rtc::ArrayView<const int16_t>(
                                    &speech_buffer_[i * samples_per_10ms_frame],
                                    samples_per_10ms_frame),
                                max_encoded_bytes, encoded);
    if (i + 1 == frames_to_encode) {
      RTC_CHECK_GT(info.encoded_bytes, 0u) << "Encoder didn't deliver data.";
    } else {
      RTC_CHECK_EQ(info.encoded_bytes, 0u)
          << "Encoder delivered data too early.";
    }
  }
  return info;
}

size_t AudioEncoderCng::SamplesPer10msFrame() const {
  return rtc::CheckedDivExact(10 * SampleRateHz(), 1000);
}

}  // namespace webrtc
