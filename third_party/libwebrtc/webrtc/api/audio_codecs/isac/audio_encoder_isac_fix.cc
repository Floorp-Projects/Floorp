/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/audio_codecs/isac/audio_encoder_isac_fix.h"

#include "common_types.h"  // NOLINT(build/include)
#include "modules/audio_coding/codecs/isac/fix/include/audio_encoder_isacfix.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/string_to_number.h"

namespace webrtc {

rtc::Optional<AudioEncoderIsacFix::Config> AudioEncoderIsacFix::SdpToConfig(
    const SdpAudioFormat& format) {
  if (STR_CASE_CMP(format.name.c_str(), "ISAC") == 0 &&
      format.clockrate_hz == 16000 && format.num_channels == 1) {
    Config config;
    const auto ptime_iter = format.parameters.find("ptime");
    if (ptime_iter != format.parameters.end()) {
      const auto ptime = rtc::StringToNumber<int>(ptime_iter->second);
      if (ptime && *ptime >= 60) {
        config.frame_size_ms = 60;
      }
    }
    return config;
  } else {
    return rtc::nullopt;
  }
}

void AudioEncoderIsacFix::AppendSupportedEncoders(
    std::vector<AudioCodecSpec>* specs) {
  const SdpAudioFormat fmt = {"ISAC", 16000, 1};
  const AudioCodecInfo info = QueryAudioEncoder(*SdpToConfig(fmt));
  specs->push_back({fmt, info});
}

AudioCodecInfo AudioEncoderIsacFix::QueryAudioEncoder(
    AudioEncoderIsacFix::Config config) {
  RTC_DCHECK(config.IsOk());
  return {16000, 1, 32000, 10000, 32000};
}

std::unique_ptr<AudioEncoder> AudioEncoderIsacFix::MakeAudioEncoder(
    AudioEncoderIsacFix::Config config,
    int payload_type) {
  RTC_DCHECK(config.IsOk());
  AudioEncoderIsacFixImpl::Config c;
  c.frame_size_ms = config.frame_size_ms;
  c.payload_type = payload_type;
  return rtc::MakeUnique<AudioEncoderIsacFixImpl>(c);
}

}  // namespace webrtc
