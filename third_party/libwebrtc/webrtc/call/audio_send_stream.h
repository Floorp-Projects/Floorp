/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_AUDIO_SEND_STREAM_H_
#define CALL_AUDIO_SEND_STREAM_H_

#include <memory>
#include <string>
#include <vector>

#include "api/audio_codecs/audio_encoder.h"
#include "api/audio_codecs/audio_encoder_factory.h"
#include "api/audio_codecs/audio_format.h"
#include "api/call/transport.h"
#include "api/optional.h"
#include "api/rtpparameters.h"
#include "call/rtp_config.h"
#include "modules/audio_processing/include/audio_processing_statistics.h"
#include "rtc_base/scoped_ref_ptr.h"
#include "typedefs.h"  // NOLINT(build/include)

namespace webrtc {

// WORK IN PROGRESS
// This class is under development and is not yet intended for for use outside
// of WebRtc/Libjingle. Please use the VoiceEngine API instead.
// See: https://bugs.chromium.org/p/webrtc/issues/detail?id=4690

class AudioSendStream {
 public:
  struct Stats {
    Stats();
    ~Stats();

    // TODO(solenberg): Harmonize naming and defaults with receive stream stats.
    uint32_t local_ssrc = 0;
    int64_t bytes_sent = 0;
    int32_t packets_sent = 0;
    int32_t packets_lost = -1;
    float fraction_lost = -1.0f;
    std::string codec_name;
    rtc::Optional<int> codec_payload_type;
    int32_t ext_seqnum = -1;
    int32_t jitter_ms = -1;
    int64_t rtt_ms = -1;
    int32_t audio_level = -1;
    // See description of "totalAudioEnergy" in the WebRTC stats spec:
    // https://w3c.github.io/webrtc-stats/#dom-rtcmediastreamtrackstats-totalaudioenergy
    double total_input_energy = 0.0;
    double total_input_duration = 0.0;
    bool typing_noise_detected = false;

    ANAStats ana_statistics;
    AudioProcessingStats apm_statistics;
  };

  struct Config {
    Config() = delete;
    explicit Config(Transport* send_transport);
    ~Config();
    std::string ToString() const;

    // Send-stream specific RTP settings.
    struct Rtp {
      Rtp();
      ~Rtp();
      std::string ToString() const;

      // Sender SSRC.
      uint32_t ssrc = 0;

      // RTP header extensions used for the sent stream.
      std::vector<RtpExtension> extensions;

      // See NackConfig for description.
      NackConfig nack;

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

    // Bitrate limits used for variable audio bitrate streams. Set both to -1 to
    // disable audio bitrate adaptation.
    // Note: This is still an experimental feature and not ready for real usage.
    int min_bitrate_bps = -1;
    int max_bitrate_bps = -1;

    // Defines whether to turn on audio network adaptor, and defines its config
    // string.
    rtc::Optional<std::string> audio_network_adaptor_config;

    struct SendCodecSpec {
      SendCodecSpec(int payload_type, const SdpAudioFormat& format);
      ~SendCodecSpec();
      std::string ToString() const;

      bool operator==(const SendCodecSpec& rhs) const;
      bool operator!=(const SendCodecSpec& rhs) const {
        return !(*this == rhs);
      }

      int payload_type;
      SdpAudioFormat format;
      bool nack_enabled = false;
      bool transport_cc_enabled = false;
      rtc::Optional<int> cng_payload_type;
      // If unset, use the encoder's default target bitrate.
      rtc::Optional<int> target_bitrate_bps;
    };

    rtc::Optional<SendCodecSpec> send_codec_spec;
    rtc::scoped_refptr<AudioEncoderFactory> encoder_factory;

    // Track ID as specified during track creation.
    std::string track_id;
  };

  virtual ~AudioSendStream() = default;

  virtual const webrtc::AudioSendStream::Config& GetConfig() const = 0;

  // Reconfigure the stream according to the Configuration.
  virtual void Reconfigure(const Config& config) = 0;

  // Starts stream activity.
  // When a stream is active, it can receive, process and deliver packets.
  virtual void Start() = 0;
  // Stops stream activity.
  // When a stream is stopped, it can't receive, process or deliver packets.
  virtual void Stop() = 0;

  // TODO(solenberg): Make payload_type a config property instead.
  virtual bool SendTelephoneEvent(int payload_type, int payload_frequency,
                                  int event, int duration_ms) = 0;

  virtual void SetMuted(bool muted) = 0;

  virtual Stats GetStats() const = 0;
  virtual Stats GetStats(bool has_remote_tracks) const = 0;
};
}  // namespace webrtc

#endif  // CALL_AUDIO_SEND_STREAM_H_
