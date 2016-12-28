/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_RECEIVE_STREAM_H_
#define WEBRTC_VIDEO_RECEIVE_STREAM_H_

#include <map>
#include <string>
#include <vector>

#include "webrtc/common_types.h"
#include "webrtc/config.h"
#include "webrtc/frame_callback.h"
#include "webrtc/stream.h"
#include "webrtc/transport.h"
#include "webrtc/video_renderer.h"

namespace webrtc {

class VideoDecoder;

class VideoReceiveStream : public ReceiveStream {
 public:
  // TODO(mflodman) Move all these settings to VideoDecoder and move the
  // declaration to common_types.h.
  struct Decoder {
    std::string ToString() const;

    // The actual decoder instance.
    VideoDecoder* decoder = nullptr;

    // Received RTP packets with this payload type will be sent to this decoder
    // instance.
    int payload_type = 0;

    // Name of the decoded payload (such as VP8). Maps back to the depacketizer
    // used to unpack incoming packets.
    std::string payload_name;
  };

  struct Stats {
    int network_frame_rate = 0;
    int decode_frame_rate = 0;
    int render_frame_rate = 0;

    // Decoder stats.
    std::string decoder_implementation_name = "unknown";
    FrameCounts frame_counts;
    int decode_ms = 0;
    int max_decode_ms = 0;
    int current_delay_ms = 0;
    int target_delay_ms = 0;
    int jitter_buffer_ms = 0;
    int min_playout_delay_ms = 0;
    int render_delay_ms = 10;

    int current_payload_type = -1;

    int total_bitrate_bps = 0;
    int discarded_packets = 0;

    uint32_t ssrc = 0;
    std::string c_name;
    StreamDataCounters rtp_stats;
    RtcpPacketTypeCounter rtcp_packet_type_counts;
    RtcpStatistics rtcp_stats;
  };

  struct Config {
    Config() = delete;
    explicit Config(Transport* rtcp_send_transport)
        : rtcp_send_transport(rtcp_send_transport) {}

    std::string ToString() const;

    // Decoders for every payload that we can receive.
    std::vector<Decoder> decoders;

    // Receive-stream specific RTP settings.
    struct Rtp {
      std::string ToString() const;

      // Synchronization source (stream identifier) to be received.
      uint32_t remote_ssrc = 0;
      // Sender SSRC used for sending RTCP (such as receiver reports).
      uint32_t local_ssrc = 0;

      // See RtcpMode for description.
      RtcpMode rtcp_mode = RtcpMode::kCompound;

      // Extended RTCP settings.
      struct RtcpXr {
        // True if RTCP Receiver Reference Time Report Block extension
        // (RFC 3611) should be enabled.
        bool receiver_reference_time_report = false;
      } rtcp_xr;

      // See draft-alvestrand-rmcat-remb for information.
      bool remb = false;

      // See draft-holmer-rmcat-transport-wide-cc-extensions for details.
      bool transport_cc = false;

      // See NackConfig for description.
      NackConfig nack;

      // See FecConfig for description.
      FecConfig fec;

      // RTX settings for incoming video payloads that may be received. RTX is
      // disabled if there's no config present.
      struct Rtx {
        // SSRCs to use for the RTX streams.
        uint32_t ssrc = 0;

        // Payload type to use for the RTX stream.
        int payload_type = 0;
      };

      // Map from video RTP payload type -> RTX config.
      typedef std::map<int, Rtx> RtxMap;
      RtxMap rtx;

      // If set to true, the RTX payload type mapping supplied in |rtx| will be
      // used when restoring RTX packets. Without it, RTX packets will always be
      // restored to the last non-RTX packet payload type received.
      bool use_rtx_payload_mapping_on_restore = false;

      // RTP header extensions used for the received stream.
      std::vector<RtpExtension> extensions;
    } rtp;

    // Transport for outgoing packets (RTCP).
    Transport* rtcp_send_transport = nullptr;

    // VideoRenderer will be called for each decoded frame. 'nullptr' disables
    // rendering of this stream.
    VideoRenderer* renderer = nullptr;

    // Expected delay needed by the renderer, i.e. the frame will be delivered
    // this many milliseconds, if possible, earlier than the ideal render time.
    // Only valid if 'renderer' is set.
    int render_delay_ms = 10;

    // Identifier for an A/V synchronization group. Empty string to disable.
    // TODO(pbos): Synchronize streams in a sync group, not just video streams
    // to one of the audio streams.
    std::string sync_group;

    // Called for each incoming video frame, i.e. in encoded state. E.g. used
    // when
    // saving the stream to a file. 'nullptr' disables the callback.
    EncodedFrameObserver* pre_decode_callback = nullptr;

    // Called for each decoded frame. E.g. used when adding effects to the
    // decoded
    // stream. 'nullptr' disables the callback.
    I420FrameCallback* pre_render_callback = nullptr;

    // Target delay in milliseconds. A positive value indicates this stream is
    // used for streaming instead of a real-time call.
    int target_delay_ms = 0;
  };

  // TODO(pbos): Add info on currently-received codec to Stats.
  virtual Stats GetStats() const = 0;
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_RECEIVE_STREAM_H_
