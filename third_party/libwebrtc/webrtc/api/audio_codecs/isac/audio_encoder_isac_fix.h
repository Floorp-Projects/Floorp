/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_AUDIO_CODECS_ISAC_AUDIO_ENCODER_ISAC_FIX_H_
#define API_AUDIO_CODECS_ISAC_AUDIO_ENCODER_ISAC_FIX_H_

#include <memory>
#include <vector>

#include "api/audio_codecs/audio_encoder.h"
#include "api/audio_codecs/audio_format.h"
#include "api/optional.h"

namespace webrtc {

// iSAC encoder API (fixed-point implementation) for use as a template
// parameter to CreateAudioEncoderFactory<...>().
//
// NOTE: This struct is still under development and may change without notice.
struct AudioEncoderIsacFix {
  struct Config {
    bool IsOk() const { return frame_size_ms == 30 || frame_size_ms == 60; }
    int frame_size_ms = 30;
  };
  static rtc::Optional<Config> SdpToConfig(const SdpAudioFormat& audio_format);
  static void AppendSupportedEncoders(std::vector<AudioCodecSpec>* specs);
  static AudioCodecInfo QueryAudioEncoder(Config config);
  static std::unique_ptr<AudioEncoder> MakeAudioEncoder(Config config,
                                                        int payload_type);
};

}  // namespace webrtc

#endif  // API_AUDIO_CODECS_ISAC_AUDIO_ENCODER_ISAC_FIX_H_
