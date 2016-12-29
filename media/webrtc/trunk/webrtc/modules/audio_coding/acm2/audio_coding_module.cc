/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/include/audio_coding_module.h"

#include "webrtc/base/checks.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/audio_coding/acm2/audio_coding_module_impl.h"
#include "webrtc/modules/audio_coding/acm2/rent_a_codec.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/system_wrappers/include/trace.h"

namespace webrtc {

// Create module
AudioCodingModule* AudioCodingModule::Create(int id) {
  Config config;
  config.id = id;
  config.clock = Clock::GetRealTimeClock();
  return Create(config);
}

AudioCodingModule* AudioCodingModule::Create(int id, Clock* clock) {
  Config config;
  config.id = id;
  config.clock = clock;
  return Create(config);
}

AudioCodingModule* AudioCodingModule::Create(const Config& config) {
  return new acm2::AudioCodingModuleImpl(config);
}

int AudioCodingModule::NumberOfCodecs() {
  return static_cast<int>(acm2::RentACodec::NumberOfCodecs());
}

int AudioCodingModule::Codec(int list_id, CodecInst* codec) {
  auto codec_id = acm2::RentACodec::CodecIdFromIndex(list_id);
  if (!codec_id)
    return -1;
  auto ci = acm2::RentACodec::CodecInstById(*codec_id);
  if (!ci)
    return -1;
  *codec = *ci;
  return 0;
}

int AudioCodingModule::Codec(const char* payload_name,
                             CodecInst* codec,
                             int sampling_freq_hz,
                             size_t channels) {
  rtc::Optional<CodecInst> ci = acm2::RentACodec::CodecInstByParams(
      payload_name, sampling_freq_hz, channels);
  if (ci) {
    *codec = *ci;
    return 0;
  } else {
    // We couldn't find a matching codec, so set the parameters to unacceptable
    // values and return.
    codec->plname[0] = '\0';
    codec->pltype = -1;
    codec->pacsize = 0;
    codec->rate = 0;
    codec->plfreq = 0;
    return -1;
  }
}

int AudioCodingModule::Codec(const char* payload_name,
                             int sampling_freq_hz,
                             size_t channels) {
  rtc::Optional<acm2::RentACodec::CodecId> ci =
      acm2::RentACodec::CodecIdByParams(payload_name, sampling_freq_hz,
                                        channels);
  if (!ci)
    return -1;
  rtc::Optional<int> i = acm2::RentACodec::CodecIndexFromId(*ci);
  return i ? *i : -1;
}

// Checks the validity of the parameters of the given codec
bool AudioCodingModule::IsCodecValid(const CodecInst& codec) {
  bool valid = acm2::RentACodec::IsCodecValid(codec);
  if (!valid)
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, -1,
                 "Invalid codec setting");
  return valid;
}

}  // namespace webrtc
