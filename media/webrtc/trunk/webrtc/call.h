/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_CALL_H_
#define WEBRTC_CALL_H_

#include <string>
#include <vector>

#include "webrtc/common_types.h"
#include "webrtc/audio_receive_stream.h"
#include "webrtc/audio_send_stream.h"
#include "webrtc/audio_state.h"
#include "webrtc/base/socket.h"
#include "webrtc/video_receive_stream.h"
#include "webrtc/video_send_stream.h"

namespace webrtc {

class AudioProcessing;

const char* Version();

enum class MediaType {
  ANY,
  AUDIO,
  VIDEO,
  DATA
};

class PacketReceiver {
 public:
  enum DeliveryStatus {
    DELIVERY_OK,
    DELIVERY_UNKNOWN_SSRC,
    DELIVERY_PACKET_ERROR,
  };

  virtual DeliveryStatus DeliverPacket(MediaType media_type,
                                       const uint8_t* packet,
                                       size_t length,
                                       const PacketTime& packet_time) = 0;

 protected:
  virtual ~PacketReceiver() {}
};

// Callback interface for reporting when a system overuse is detected.
class LoadObserver {
 public:
  enum Load { kOveruse, kUnderuse };

  // Triggered when overuse is detected or when we believe the system can take
  // more load.
  virtual void OnLoadUpdate(Load load) = 0;

 protected:
  virtual ~LoadObserver() {}
};

// A Call instance can contain several send and/or receive streams. All streams
// are assumed to have the same remote endpoint and will share bitrate estimates
// etc.
class Call {
 public:
  struct Config {
    static const int kDefaultStartBitrateBps;

    // Bitrate config used until valid bitrate estimates are calculated. Also
    // used to cap total bitrate used.
    struct BitrateConfig {
      int min_bitrate_bps = 0;
      int start_bitrate_bps = kDefaultStartBitrateBps;
      int max_bitrate_bps = -1;
    } bitrate_config;

    // AudioState which is possibly shared between multiple calls.
    // TODO(solenberg): Change this to a shared_ptr once we can use C++11.
    rtc::scoped_refptr<AudioState> audio_state;

    // Audio Processing Module to be used in this call.
    // TODO(solenberg): Change this to a shared_ptr once we can use C++11.
    AudioProcessing* audio_processing = nullptr;
  };

  struct Stats {
    int send_bandwidth_bps = 0;
    int recv_bandwidth_bps = 0;
    int64_t pacer_delay_ms = 0;
    int64_t rtt_ms = -1;
  };

  static Call* Create(const Call::Config& config);

  virtual AudioSendStream* CreateAudioSendStream(
      const AudioSendStream::Config& config) = 0;
  virtual void DestroyAudioSendStream(AudioSendStream* send_stream) = 0;

  virtual AudioReceiveStream* CreateAudioReceiveStream(
      const AudioReceiveStream::Config& config) = 0;
  virtual void DestroyAudioReceiveStream(
      AudioReceiveStream* receive_stream) = 0;

  virtual VideoSendStream* CreateVideoSendStream(
      const VideoSendStream::Config& config,
      const VideoEncoderConfig& encoder_config) = 0;
  virtual void DestroyVideoSendStream(VideoSendStream* send_stream) = 0;

  virtual VideoReceiveStream* CreateVideoReceiveStream(
      const VideoReceiveStream::Config& config) = 0;
  virtual void DestroyVideoReceiveStream(
      VideoReceiveStream* receive_stream) = 0;

  // All received RTP and RTCP packets for the call should be inserted to this
  // PacketReceiver. The PacketReceiver pointer is valid as long as the
  // Call instance exists.
  virtual PacketReceiver* Receiver() = 0;

  // Returns the call statistics, such as estimated send and receive bandwidth,
  // pacing delay, etc.
  virtual Stats GetStats() const = 0;

  // TODO(pbos): Like BitrateConfig above this is currently per-stream instead
  // of maximum for entire Call. This should be fixed along with the above.
  // Specifying a start bitrate (>0) will currently reset the current bitrate
  // estimate. This is due to how the 'x-google-start-bitrate' flag is currently
  // implemented.
  virtual void SetBitrateConfig(
      const Config::BitrateConfig& bitrate_config) = 0;
  virtual void SignalNetworkState(NetworkState state) = 0;

  virtual void OnSentPacket(const rtc::SentPacket& sent_packet) = 0;

  virtual ~Call() {}
};

}  // namespace webrtc

#endif  // WEBRTC_CALL_H_
