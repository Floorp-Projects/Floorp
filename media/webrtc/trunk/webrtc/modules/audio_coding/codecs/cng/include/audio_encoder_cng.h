/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_CNG_INCLUDE_AUDIO_ENCODER_CNG_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_CNG_INCLUDE_AUDIO_ENCODER_CNG_H_

#include <vector>

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/common_audio/vad/include/vad.h"
#include "webrtc/modules/audio_coding/codecs/audio_encoder.h"
#include "webrtc/modules/audio_coding/codecs/cng/include/webrtc_cng.h"

namespace webrtc {

class Vad;

class AudioEncoderCng final : public AudioEncoder {
 public:
  struct Config {
    Config();
    bool IsOk() const;

    int num_channels;
    int payload_type;
    // Caller keeps ownership of the AudioEncoder object.
    AudioEncoder* speech_encoder;
    Vad::Aggressiveness vad_mode;
    int sid_frame_interval_ms;
    int num_cng_coefficients;
    // The Vad pointer is mainly for testing. If a NULL pointer is passed, the
    // AudioEncoderCng creates (and destroys) a Vad object internally. If an
    // object is passed, the AudioEncoderCng assumes ownership of the Vad
    // object.
    Vad* vad;
  };

  explicit AudioEncoderCng(const Config& config);

  ~AudioEncoderCng() override;

  int SampleRateHz() const override;
  int NumChannels() const override;
  size_t MaxEncodedBytes() const override;
  int RtpTimestampRateHz() const override;
  int Num10MsFramesInNextPacket() const override;
  int Max10MsFramesInAPacket() const override;
  void SetTargetBitrate(int bits_per_second) override;
  void SetProjectedPacketLossRate(double fraction) override;

 protected:
  EncodedInfo EncodeInternal(uint32_t rtp_timestamp,
                             const int16_t* audio,
                             size_t max_encoded_bytes,
                             uint8_t* encoded) override;

 private:
  // Deleter for use with scoped_ptr. E.g., use as
  //   rtc::scoped_ptr<CNG_enc_inst, CngInstDeleter> cng_inst_;
  struct CngInstDeleter {
    inline void operator()(CNG_enc_inst* ptr) const { WebRtcCng_FreeEnc(ptr); }
  };

  EncodedInfo EncodePassive(size_t max_encoded_bytes, uint8_t* encoded);
  EncodedInfo EncodeActive(size_t max_encoded_bytes, uint8_t* encoded);
  size_t SamplesPer10msFrame() const;

  AudioEncoder* speech_encoder_;
  const int cng_payload_type_;
  const int num_cng_coefficients_;
  std::vector<int16_t> speech_buffer_;
  uint32_t first_timestamp_in_buffer_;
  int frames_in_buffer_;
  bool last_frame_active_;
  rtc::scoped_ptr<Vad> vad_;
  rtc::scoped_ptr<CNG_enc_inst, CngInstDeleter> cng_inst_;
};

}  // namespace webrtc
#endif  // WEBRTC_MODULES_AUDIO_CODING_CODECS_CNG_INCLUDE_AUDIO_ENCODER_CNG_H_
