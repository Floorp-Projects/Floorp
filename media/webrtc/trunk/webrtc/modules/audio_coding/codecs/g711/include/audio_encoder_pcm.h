/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_G711_INCLUDE_AUDIO_ENCODER_PCM_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_G711_INCLUDE_AUDIO_ENCODER_PCM_H_

#include <vector>

#include "webrtc/modules/audio_coding/codecs/audio_encoder.h"

namespace webrtc {

class AudioEncoderPcm : public AudioEncoder {
 public:
  struct Config {
    Config() : frame_size_ms(20), num_channels(1) {}

    int frame_size_ms;
    int num_channels;
  };

  explicit AudioEncoderPcm(const Config& config);

  virtual ~AudioEncoderPcm();

  virtual int sample_rate_hz() const OVERRIDE;
  virtual int num_channels() const OVERRIDE;
  virtual int Num10MsFramesInNextPacket() const OVERRIDE;

 protected:
  virtual bool Encode(uint32_t timestamp,
                      const int16_t* audio,
                      size_t max_encoded_bytes,
                      uint8_t* encoded,
                      size_t* encoded_bytes,
                      uint32_t* encoded_timestamp) OVERRIDE;

  virtual int16_t EncodeCall(const int16_t* audio,
                             size_t input_len,
                             uint8_t* encoded) = 0;

 private:
  static const int kSampleRateHz = 8000;
  const int num_channels_;
  const int num_10ms_frames_per_packet_;
  const int16_t full_frame_samples_;
  std::vector<int16_t> speech_buffer_;
  uint32_t first_timestamp_in_buffer_;
};

class AudioEncoderPcmA : public AudioEncoderPcm {
 public:
  explicit AudioEncoderPcmA(const Config& config) : AudioEncoderPcm(config) {}

 protected:
  virtual int16_t EncodeCall(const int16_t* audio,
                             size_t input_len,
                             uint8_t* encoded) OVERRIDE;
};

class AudioEncoderPcmU : public AudioEncoderPcm {
 public:
  explicit AudioEncoderPcmU(const Config& config) : AudioEncoderPcm(config) {}

 protected:
  virtual int16_t EncodeCall(const int16_t* audio,
                             size_t input_len,
                             uint8_t* encoded) OVERRIDE;
};

}  // namespace webrtc
#endif  // WEBRTC_MODULES_AUDIO_CODING_CODECS_G711_INCLUDE_AUDIO_ENCODER_PCM_H_
