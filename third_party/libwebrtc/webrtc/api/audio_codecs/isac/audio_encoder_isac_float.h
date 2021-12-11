/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_AUDIO_CODECS_ISAC_AUDIO_ENCODER_ISAC_FLOAT_H_
#define API_AUDIO_CODECS_ISAC_AUDIO_ENCODER_ISAC_FLOAT_H_

#include <memory>
#include <vector>

#include "api/audio_codecs/audio_encoder.h"
#include "api/audio_codecs/audio_format.h"
#include "api/optional.h"

namespace webrtc {

// iSAC encoder API (floating-point implementation) for use as a template
// parameter to CreateAudioEncoderFactory<...>().
//
// NOTE: This struct is still under development and may change without notice.
struct AudioEncoderIsacFloat {
  struct Config {
    bool IsOk() const {
      return (sample_rate_hz == 16000 &&
              (frame_size_ms == 30 || frame_size_ms == 60)) ||
             (sample_rate_hz == 32000 && frame_size_ms == 30);
    }
    int sample_rate_hz = 16000;
    int frame_size_ms = 30;
  };
  static rtc::Optional<Config> SdpToConfig(const SdpAudioFormat& audio_format);
  static void AppendSupportedEncoders(std::vector<AudioCodecSpec>* specs);
  static AudioCodecInfo QueryAudioEncoder(const Config& config);
  static std::unique_ptr<AudioEncoder> MakeAudioEncoder(const Config& config,
                                                        int payload_type);
};

}  // namespace webrtc

#endif  // API_AUDIO_CODECS_ISAC_AUDIO_ENCODER_ISAC_FLOAT_H_
