/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_INTERFACE_AUDIO_ENCODER_ILBC_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_INTERFACE_AUDIO_ENCODER_ILBC_H_

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/audio_coding/codecs/audio_encoder.h"
#include "webrtc/modules/audio_coding/codecs/ilbc/interface/ilbc.h"

namespace webrtc {

class AudioEncoderIlbc : public AudioEncoder {
 public:
  struct Config {
    Config() : payload_type(102), frame_size_ms(30) {}

    int payload_type;
    int frame_size_ms;  // Valid values are 20, 30, 40, and 60 ms.
    // Note that frame size 40 ms produces encodings with two 20 ms frames in
    // them, and frame size 60 ms consists of two 30 ms frames.
  };

  explicit AudioEncoderIlbc(const Config& config);
  ~AudioEncoderIlbc() override;

  int SampleRateHz() const override;
  int NumChannels() const override;
  size_t MaxEncodedBytes() const override;
  int Num10MsFramesInNextPacket() const override;
  int Max10MsFramesInAPacket() const override;

 protected:
  EncodedInfo EncodeInternal(uint32_t rtp_timestamp,
                             const int16_t* audio,
                             size_t max_encoded_bytes,
                             uint8_t* encoded) override;

 private:
  size_t RequiredOutputSizeBytes() const;

  static const int kMaxSamplesPerPacket = 480;
  const int payload_type_;
  const int num_10ms_frames_per_packet_;
  int num_10ms_frames_buffered_;
  uint32_t first_timestamp_in_buffer_;
  int16_t input_buffer_[kMaxSamplesPerPacket];
  IlbcEncoderInstance* encoder_;
};

}  // namespace webrtc
#endif  // WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_INTERFACE_AUDIO_ENCODER_ILBC_H_
