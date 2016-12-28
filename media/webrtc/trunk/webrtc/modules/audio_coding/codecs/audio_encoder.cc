/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/codecs/audio_encoder.h"

#include "webrtc/base/checks.h"
#include "webrtc/base/trace_event.h"

namespace webrtc {

AudioEncoder::EncodedInfo::EncodedInfo() = default;

AudioEncoder::EncodedInfo::~EncodedInfo() = default;

int AudioEncoder::RtpTimestampRateHz() const {
  return SampleRateHz();
}

AudioEncoder::EncodedInfo AudioEncoder::Encode(
    uint32_t rtp_timestamp,
    rtc::ArrayView<const int16_t> audio,
    size_t max_encoded_bytes,
    uint8_t* encoded) {
  TRACE_EVENT0("webrtc", "AudioEncoder::Encode");
  RTC_CHECK_EQ(audio.size(),
               static_cast<size_t>(NumChannels() * SampleRateHz() / 100));
  EncodedInfo info =
      EncodeInternal(rtp_timestamp, audio, max_encoded_bytes, encoded);
  RTC_CHECK_LE(info.encoded_bytes, max_encoded_bytes);
  return info;
}

bool AudioEncoder::SetFec(bool enable) {
  return !enable;
}

bool AudioEncoder::SetDtx(bool enable) {
  return !enable;
}

bool AudioEncoder::SetApplication(Application application) {
  return false;
}

void AudioEncoder::SetMaxPlaybackRate(int frequency_hz) {}

void AudioEncoder::SetProjectedPacketLossRate(double fraction) {}

void AudioEncoder::SetTargetBitrate(int target_bps) {}

}  // namespace webrtc
