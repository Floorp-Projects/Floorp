/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// TODO(pbos): Move Config from common.h to here.

#ifndef WEBRTC_CONFIG_H_
#define WEBRTC_CONFIG_H_

#include <string>
#include <vector>

#include "webrtc/common.h"
#include "webrtc/common_types.h"
#include "webrtc/typedefs.h"

namespace webrtc {

// Settings for NACK, see RFC 4585 for details.
struct NackConfig {
  NackConfig() : rtp_history_ms(0) {}
  // Send side: the time RTP packets are stored for retransmissions.
  // Receive side: the time the receiver is prepared to wait for
  // retransmissions.
  // Set to '0' to disable.
  int rtp_history_ms;
};

// Settings for forward error correction, see RFC 5109 for details. Set the
// payload types to '-1' to disable.
struct FecConfig {
  FecConfig()
      : ulpfec_payload_type(-1),
        red_payload_type(-1),
        red_rtx_payload_type(-1) {}
  std::string ToString() const;
  // Payload type used for ULPFEC packets.
  int ulpfec_payload_type;

  // Payload type used for RED packets.
  int red_payload_type;

  // RTX payload type for RED payload.
  int red_rtx_payload_type;
};

// RTP header extension, see RFC 5285.
struct RtpExtension {
  RtpExtension(const std::string& name, int id) : name(name), id(id) {}
  std::string ToString() const;
  bool operator==(const RtpExtension& rhs) const {
    return name == rhs.name && id == rhs.id;
  }
  static bool IsSupportedForAudio(const std::string& name);
  static bool IsSupportedForVideo(const std::string& name);

  static const char* kTOffset;
  static const char* kAbsSendTime;
  static const char* kVideoRotation;
  static const char* kAudioLevel;
  static const char* kTransportSequenceNumber;
  std::string name;
  int id;
};

struct VideoStream {
  VideoStream();
  ~VideoStream();
  std::string ToString() const;

  size_t width;
  size_t height;
  int max_framerate;

  int min_bitrate_bps;
  int target_bitrate_bps;
  int max_bitrate_bps;

  int max_qp;

  // Bitrate thresholds for enabling additional temporal layers. Since these are
  // thresholds in between layers, we have one additional layer. One threshold
  // gives two temporal layers, one below the threshold and one above, two give
  // three, and so on.
  // The VideoEncoder may redistribute bitrates over the temporal layers so a
  // bitrate threshold of 100k and an estimate of 105k does not imply that we
  // get 100k in one temporal layer and 5k in the other, just that the bitrate
  // in the first temporal layer should not exceed 100k.
  // TODO(pbos): Apart from a special case for two-layer screencast these
  // thresholds are not propagated to the VideoEncoder. To be implemented.
  std::vector<int> temporal_layer_thresholds_bps;
};

struct VideoEncoderConfig {
  enum class ContentType {
    kRealtimeVideo,
    kScreen,
  };

  VideoEncoderConfig();
  ~VideoEncoderConfig();
  std::string ToString() const;

  std::vector<VideoStream> streams;
  std::vector<SpatialLayer> spatial_layers;
  ContentType content_type;
  void* encoder_specific_settings;

  // Padding will be used up to this bitrate regardless of the bitrate produced
  // by the encoder. Padding above what's actually produced by the encoder helps
  // maintaining a higher bitrate estimate. Padding will however not be sent
  // unless the estimated bandwidth indicates that the link can handle it.
  int min_transmit_bitrate_bps;
};

// Controls the capacity of the packet buffer in NetEq. The capacity is the
// maximum number of packets that the buffer can contain. If the limit is
// exceeded, the buffer will be flushed. The capacity does not affect the actual
// audio delay in the general case, since this is governed by the target buffer
// level (calculated from the jitter profile). It is only in the rare case of
// severe network freezes that a higher capacity will lead to a (transient)
// increase in audio delay.
struct NetEqCapacityConfig {
  NetEqCapacityConfig() : enabled(false), capacity(0) {}
  explicit NetEqCapacityConfig(int value) : enabled(true), capacity(value) {}
  static const ConfigOptionID identifier = ConfigOptionID::kNetEqCapacityConfig;
  bool enabled;
  int capacity;
};

struct NetEqFastAccelerate {
  NetEqFastAccelerate() : enabled(false) {}
  explicit NetEqFastAccelerate(bool value) : enabled(value) {}
  static const ConfigOptionID identifier = ConfigOptionID::kNetEqFastAccelerate;
  bool enabled;
};

struct VoicePacing {
  VoicePacing() : enabled(false) {}
  explicit VoicePacing(bool value) : enabled(value) {}
  static const ConfigOptionID identifier = ConfigOptionID::kVoicePacing;
  bool enabled;
};

}  // namespace webrtc

#endif  // WEBRTC_CONFIG_H_
