/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_AUDIO_SEND_STREAM_H_
#define WEBRTC_AUDIO_SEND_STREAM_H_

#include <string>
#include <vector>

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/config.h"
#include "webrtc/modules/audio_coding/codecs/audio_encoder.h"
#include "webrtc/stream.h"
#include "webrtc/transport.h"
#include "webrtc/typedefs.h"

namespace webrtc {

// WORK IN PROGRESS
// This class is under development and is not yet intended for for use outside
// of WebRtc/Libjingle. Please use the VoiceEngine API instead.
// See: https://bugs.chromium.org/p/webrtc/issues/detail?id=4690

class AudioSendStream : public SendStream {
 public:
  struct Stats {
    // TODO(solenberg): Harmonize naming and defaults with receive stream stats.
    uint32_t local_ssrc = 0;
    int64_t bytes_sent = 0;
    int32_t packets_sent = 0;
    int32_t packets_lost = -1;
    float fraction_lost = -1.0f;
    std::string codec_name;
    int32_t ext_seqnum = -1;
    int32_t jitter_ms = -1;
    int64_t rtt_ms = -1;
    int32_t audio_level = -1;
    float aec_quality_min = -1.0f;
    int32_t echo_delay_median_ms = -1;
    int32_t echo_delay_std_ms = -1;
    int32_t echo_return_loss = -100;
    int32_t echo_return_loss_enhancement = -100;
    bool typing_noise_detected = false;
  };

  struct Config {
    Config() = delete;
    explicit Config(Transport* send_transport)
        : send_transport(send_transport) {}

    std::string ToString() const;

    // Receive-stream specific RTP settings.
    struct Rtp {
      std::string ToString() const;

      // Sender SSRC.
      uint32_t ssrc = 0;

      // RTP header extensions used for the sent stream.
      std::vector<RtpExtension> extensions;

      // RTCP CNAME, see RFC 3550.
      std::string c_name;
    } rtp;

    // Transport for outgoing packets. The transport is expected to exist for
    // the entire life of the AudioSendStream and is owned by the API client.
    Transport* send_transport = nullptr;

    // Underlying VoiceEngine handle, used to map AudioSendStream to lower-level
    // components.
    // TODO(solenberg): Remove when VoiceEngine channels are created outside
    // of Call.
    int voe_channel_id = -1;

    // Ownership of the encoder object is transferred to Call when the config is
    // passed to Call::CreateAudioSendStream().
    // TODO(solenberg): Implement, once we configure codecs through the new API.
    // rtc::scoped_ptr<AudioEncoder> encoder;
    int cng_payload_type = -1;  // pt, or -1 to disable Comfort Noise Generator.
    int red_payload_type = -1;  // pt, or -1 to disable REDundant coding.
  };

  // TODO(solenberg): Make payload_type a config property instead.
  virtual bool SendTelephoneEvent(int payload_type, uint8_t event,
                                  uint32_t duration_ms) = 0;
  virtual Stats GetStats() const = 0;
};
}  // namespace webrtc

#endif  // WEBRTC_AUDIO_SEND_STREAM_H_
