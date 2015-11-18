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
   public:
    int frame_size_ms;
    int num_channels;
    int payload_type;

   protected:
    explicit Config(int pt)
        : frame_size_ms(20), num_channels(1), payload_type(pt) {}
  };

  ~AudioEncoderPcm() override;

  int SampleRateHz() const override;
  int NumChannels() const override;
  size_t MaxEncodedBytes() const override;
  int Num10MsFramesInNextPacket() const override;
  int Max10MsFramesInAPacket() const override;

 protected:
  AudioEncoderPcm(const Config& config, int sample_rate_hz);

  EncodedInfo EncodeInternal(uint32_t rtp_timestamp,
                             const int16_t* audio,
                             size_t max_encoded_bytes,
                             uint8_t* encoded) override;

  virtual int16_t EncodeCall(const int16_t* audio,
                             size_t input_len,
                             uint8_t* encoded) = 0;

 private:
  const int sample_rate_hz_;
  const int num_channels_;
  const int payload_type_;
  const int num_10ms_frames_per_packet_;
  const size_t full_frame_samples_;
  std::vector<int16_t> speech_buffer_;
  uint32_t first_timestamp_in_buffer_;
};

class AudioEncoderPcmA : public AudioEncoderPcm {
 public:
  struct Config : public AudioEncoderPcm::Config {
    Config() : AudioEncoderPcm::Config(8) {}
  };

  explicit AudioEncoderPcmA(const Config& config)
      : AudioEncoderPcm(config, kSampleRateHz) {}

 protected:
  int16_t EncodeCall(const int16_t* audio,
                     size_t input_len,
                     uint8_t* encoded) override;

 private:
  static const int kSampleRateHz = 8000;
};

class AudioEncoderPcmU : public AudioEncoderPcm {
 public:
  struct Config : public AudioEncoderPcm::Config {
    Config() : AudioEncoderPcm::Config(0) {}
  };

  explicit AudioEncoderPcmU(const Config& config)
      : AudioEncoderPcm(config, kSampleRateHz) {}

 protected:
  int16_t EncodeCall(const int16_t* audio,
                     size_t input_len,
                     uint8_t* encoded) override;

 private:
  static const int kSampleRateHz = 8000;
};

}  // namespace webrtc
#endif  // WEBRTC_MODULES_AUDIO_CODING_CODECS_G711_INCLUDE_AUDIO_ENCODER_PCM_H_
