/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_NEW_INCLUDE_COMMON_H_
#define WEBRTC_VIDEO_ENGINE_NEW_INCLUDE_COMMON_H_

#include <string>

#include "webrtc/common_types.h"

namespace webrtc {

class I420VideoFrame;

namespace newapi {

struct EncodedFrame;

class I420FrameCallback {
 public:
  // This function is called with a I420 frame allowing the user to modify the
  // frame content.
  virtual void FrameCallback(I420VideoFrame* video_frame) = 0;

 protected:
  virtual ~I420FrameCallback() {}
};

class EncodedFrameObserver {
 public:
  virtual void EncodedFrameCallback(const EncodedFrame& encoded_frame) = 0;

 protected:
  virtual ~EncodedFrameObserver() {}
};

class VideoRenderer {
 public:
  // This function should return as soon as possible and not block until it's
  // time to render the frame.
  // TODO(mflodman) Remove time_to_render_ms when I420VideoFrame contains NTP.
  virtual void RenderFrame(const I420VideoFrame& video_frame,
                           int time_to_render_ms) = 0;

 protected:
  virtual ~VideoRenderer() {}
};

class Transport {
 public:
  virtual bool SendRTP(const void* packet, size_t length) = 0;
  virtual bool SendRTCP(const void* packet, size_t length) = 0;

 protected:
  virtual ~Transport() {}
};

struct RtpStatistics {
  uint32_t ssrc;
  int fraction_loss;
  int cumulative_loss;
  int extended_max_sequence_number;
  std::string c_name;
};

// RTCP mode to use. Compound mode is described by RFC 4585 and reduced sized
// RTCP mode is described by RFC 5506.
enum RtcpMode {
  kRtcpCompound,
  kRtcpReducedSize
};

// Settings for NACK, see RFC 4585 for details.
struct NackConfig {
  // Send side: the time RTP packets are stored for retransmissions.
  // Receive side: the time the receiver is prepared to wait for
  // retransmissions.
  // Set to '0' to disable
  int rtp_history_ms;
};

// Settings for forward error correction, see RFC 5109 for details. Set the
// payload types to '-1' to disable.
struct FecConfig {
  // Payload type used for ULPFEC packets.
  int ulpfec_payload_type;

  // Payload type used for RED packets.
  int red_payload_type;
};

// Settings for RTP retransmission payload format, see RFC 4588 for details.
struct RtxConfig {
  // SSRC to use for the RTX stream, set to '0' for a random generated SSRC.
  uint32_t ssrc;

  // Payload type to use for the RTX stream.
  int rtx_payload_type;

  // Original video payload this RTX stream is used for.
  int video_payload_type;
};

// RTP header extension to use for the video stream, see RFC 5285.
struct RtpExtension {
  // TODO(mflodman) Add API to query supported extensions.
  std::string name;
  int id;
};

}  // namespace newapi
}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_NEW_INCLUDE_COMMON_H_
