/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_AUDIO_ENCODER_ISAC_T_IMPL_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_AUDIO_ENCODER_ISAC_T_IMPL_H_

#include "webrtc/modules/audio_coding/codecs/isac/main/interface/audio_encoder_isac.h"

#include <algorithm>

#include "webrtc/base/checks.h"
#include "webrtc/modules/audio_coding/codecs/isac/main/interface/isac.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"

namespace webrtc {

const int kIsacPayloadType = 103;
const int kDefaultBitRate = 32000;

template <typename T>
AudioEncoderDecoderIsacT<T>::Config::Config()
    : payload_type(kIsacPayloadType),
      sample_rate_hz(16000),
      frame_size_ms(30),
      bit_rate(kDefaultBitRate),
      max_bit_rate(-1),
      max_payload_size_bytes(-1) {
}

template <typename T>
bool AudioEncoderDecoderIsacT<T>::Config::IsOk() const {
  if (max_bit_rate < 32000 && max_bit_rate != -1)
    return false;
  if (max_payload_size_bytes < 120 && max_payload_size_bytes != -1)
    return false;
  switch (sample_rate_hz) {
    case 16000:
      if (max_bit_rate > 53400)
        return false;
      if (max_payload_size_bytes > 400)
        return false;
      return (frame_size_ms == 30 || frame_size_ms == 60) &&
             ((bit_rate >= 10000 && bit_rate <= 32000) || bit_rate == 0);
    case 32000:
    case 48000:
      if (max_bit_rate > 160000)
        return false;
      if (max_payload_size_bytes > 600)
        return false;
      return T::has_swb &&
             (frame_size_ms == 30 &&
              ((bit_rate >= 10000 && bit_rate <= 56000) || bit_rate == 0));
    default:
      return false;
  }
}

template <typename T>
AudioEncoderDecoderIsacT<T>::ConfigAdaptive::ConfigAdaptive()
    : payload_type(kIsacPayloadType),
      sample_rate_hz(16000),
      initial_frame_size_ms(30),
      initial_bit_rate(kDefaultBitRate),
      max_bit_rate(-1),
      enforce_frame_size(false),
      max_payload_size_bytes(-1) {
}

template <typename T>
bool AudioEncoderDecoderIsacT<T>::ConfigAdaptive::IsOk() const {
  if (max_bit_rate < 32000 && max_bit_rate != -1)
    return false;
  if (max_payload_size_bytes < 120 && max_payload_size_bytes != -1)
    return false;
  switch (sample_rate_hz) {
    case 16000:
      if (max_bit_rate > 53400)
        return false;
      if (max_payload_size_bytes > 400)
        return false;
      return (initial_frame_size_ms == 30 || initial_frame_size_ms == 60) &&
             initial_bit_rate >= 10000 && initial_bit_rate <= 32000;
    case 32000:
    case 48000:
      if (max_bit_rate > 160000)
        return false;
      if (max_payload_size_bytes > 600)
        return false;
      return T::has_swb &&
             (initial_frame_size_ms == 30 && initial_bit_rate >= 10000 &&
              initial_bit_rate <= 56000);
    default:
      return false;
  }
}

template <typename T>
AudioEncoderDecoderIsacT<T>::AudioEncoderDecoderIsacT(const Config& config)
    : payload_type_(config.payload_type),
      state_lock_(CriticalSectionWrapper::CreateCriticalSection()),
      decoder_sample_rate_hz_(0),
      lock_(CriticalSectionWrapper::CreateCriticalSection()),
      packet_in_progress_(false) {
  CHECK(config.IsOk());
  CHECK_EQ(0, T::Create(&isac_state_));
  CHECK_EQ(0, T::EncoderInit(isac_state_, 1));
  CHECK_EQ(0, T::SetEncSampRate(isac_state_, config.sample_rate_hz));
  CHECK_EQ(0, T::Control(isac_state_, config.bit_rate == 0 ? kDefaultBitRate
                                                           : config.bit_rate,
                         config.frame_size_ms));
  // When config.sample_rate_hz is set to 48000 Hz (iSAC-fb), the decoder is
  // still set to 32000 Hz, since there is no full-band mode in the decoder.
  CHECK_EQ(0, T::SetDecSampRate(isac_state_,
                                std::min(config.sample_rate_hz, 32000)));
  if (config.max_payload_size_bytes != -1)
    CHECK_EQ(0,
             T::SetMaxPayloadSize(isac_state_, config.max_payload_size_bytes));
  if (config.max_bit_rate != -1)
    CHECK_EQ(0, T::SetMaxRate(isac_state_, config.max_bit_rate));
}

template <typename T>
AudioEncoderDecoderIsacT<T>::AudioEncoderDecoderIsacT(
    const ConfigAdaptive& config)
    : payload_type_(config.payload_type),
      state_lock_(CriticalSectionWrapper::CreateCriticalSection()),
      decoder_sample_rate_hz_(0),
      lock_(CriticalSectionWrapper::CreateCriticalSection()),
      packet_in_progress_(false) {
  CHECK(config.IsOk());
  CHECK_EQ(0, T::Create(&isac_state_));
  CHECK_EQ(0, T::EncoderInit(isac_state_, 0));
  CHECK_EQ(0, T::SetEncSampRate(isac_state_, config.sample_rate_hz));
  CHECK_EQ(0, T::ControlBwe(isac_state_, config.initial_bit_rate,
                            config.initial_frame_size_ms,
                            config.enforce_frame_size));
  CHECK_EQ(0, T::SetDecSampRate(isac_state_, config.sample_rate_hz));
  if (config.max_payload_size_bytes != -1)
    CHECK_EQ(0,
             T::SetMaxPayloadSize(isac_state_, config.max_payload_size_bytes));
  if (config.max_bit_rate != -1)
    CHECK_EQ(0, T::SetMaxRate(isac_state_, config.max_bit_rate));
}

template <typename T>
AudioEncoderDecoderIsacT<T>::~AudioEncoderDecoderIsacT() {
  CHECK_EQ(0, T::Free(isac_state_));
}

template <typename T>
int AudioEncoderDecoderIsacT<T>::SampleRateHz() const {
  CriticalSectionScoped cs(state_lock_.get());
  return T::EncSampRate(isac_state_);
}

template <typename T>
int AudioEncoderDecoderIsacT<T>::NumChannels() const {
  return 1;
}

template <typename T>
size_t AudioEncoderDecoderIsacT<T>::MaxEncodedBytes() const {
  return kSufficientEncodeBufferSizeBytes;
}

template <typename T>
int AudioEncoderDecoderIsacT<T>::Num10MsFramesInNextPacket() const {
  CriticalSectionScoped cs(state_lock_.get());
  const int samples_in_next_packet = T::GetNewFrameLen(isac_state_);
  return rtc::CheckedDivExact(samples_in_next_packet,
                              rtc::CheckedDivExact(SampleRateHz(), 100));
}

template <typename T>
int AudioEncoderDecoderIsacT<T>::Max10MsFramesInAPacket() const {
  return 6;  // iSAC puts at most 60 ms in a packet.
}

template <typename T>
AudioEncoder::EncodedInfo AudioEncoderDecoderIsacT<T>::EncodeInternal(
    uint32_t rtp_timestamp,
    const int16_t* audio,
    size_t max_encoded_bytes,
    uint8_t* encoded) {
  CriticalSectionScoped cs_lock(lock_.get());
  if (!packet_in_progress_) {
    // Starting a new packet; remember the timestamp for later.
    packet_in_progress_ = true;
    packet_timestamp_ = rtp_timestamp;
  }
  int r;
  {
    CriticalSectionScoped cs(state_lock_.get());
    r = T::Encode(isac_state_, audio, encoded);
  }
  CHECK_GE(r, 0);

  // T::Encode doesn't allow us to tell it the size of the output
  // buffer. All we can do is check for an overrun after the fact.
  CHECK(static_cast<size_t>(r) <= max_encoded_bytes);

  if (r == 0)
    return EncodedInfo();

  // Got enough input to produce a packet. Return the saved timestamp from
  // the first chunk of input that went into the packet.
  packet_in_progress_ = false;
  EncodedInfo info;
  info.encoded_bytes = r;
  info.encoded_timestamp = packet_timestamp_;
  info.payload_type = payload_type_;
  return info;
}

template <typename T>
int AudioEncoderDecoderIsacT<T>::DecodeInternal(const uint8_t* encoded,
                                                size_t encoded_len,
                                                int sample_rate_hz,
                                                int16_t* decoded,
                                                SpeechType* speech_type) {
  CriticalSectionScoped cs(state_lock_.get());
  // We want to crate the illusion that iSAC supports 48000 Hz decoding, while
  // in fact it outputs 32000 Hz. This is the iSAC fullband mode.
  if (sample_rate_hz == 48000)
    sample_rate_hz = 32000;
  CHECK(sample_rate_hz == 16000 || sample_rate_hz == 32000)
      << "Unsupported sample rate " << sample_rate_hz;
  if (sample_rate_hz != decoder_sample_rate_hz_) {
    CHECK_EQ(0, T::SetDecSampRate(isac_state_, sample_rate_hz));
    decoder_sample_rate_hz_ = sample_rate_hz;
  }
  int16_t temp_type = 1;  // Default is speech.
  int16_t ret =
      T::DecodeInternal(isac_state_, encoded, static_cast<int16_t>(encoded_len),
                        decoded, &temp_type);
  *speech_type = ConvertSpeechType(temp_type);
  return ret;
}

template <typename T>
bool AudioEncoderDecoderIsacT<T>::HasDecodePlc() const {
  return false;
}

template <typename T>
int AudioEncoderDecoderIsacT<T>::DecodePlc(int num_frames, int16_t* decoded) {
  CriticalSectionScoped cs(state_lock_.get());
  return T::DecodePlc(isac_state_, decoded, num_frames);
}

template <typename T>
int AudioEncoderDecoderIsacT<T>::Init() {
  CriticalSectionScoped cs(state_lock_.get());
  return T::DecoderInit(isac_state_);
}

template <typename T>
int AudioEncoderDecoderIsacT<T>::IncomingPacket(const uint8_t* payload,
                                                size_t payload_len,
                                                uint16_t rtp_sequence_number,
                                                uint32_t rtp_timestamp,
                                                uint32_t arrival_timestamp) {
  CriticalSectionScoped cs(state_lock_.get());
  return T::UpdateBwEstimate(
      isac_state_, payload, static_cast<int32_t>(payload_len),
      rtp_sequence_number, rtp_timestamp, arrival_timestamp);
}

template <typename T>
int AudioEncoderDecoderIsacT<T>::ErrorCode() {
  CriticalSectionScoped cs(state_lock_.get());
  return T::GetErrorCode(isac_state_);
}

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_AUDIO_ENCODER_ISAC_T_IMPL_H_
