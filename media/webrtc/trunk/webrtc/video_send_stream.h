/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_SEND_STREAM_H_
#define WEBRTC_VIDEO_SEND_STREAM_H_

#include <map>
#include <string>

#include "webrtc/common_types.h"
#include "webrtc/config.h"
#include "webrtc/frame_callback.h"
#include "webrtc/video_renderer.h"

namespace webrtc {

class VideoEncoder;

// Class to deliver captured frame to the video send stream.
class VideoSendStreamInput {
 public:
  // These methods do not lock internally and must be called sequentially.
  // If your application switches input sources synchronization must be done
  // externally to make sure that any old frames are not delivered concurrently.
  virtual void IncomingCapturedFrame(const I420VideoFrame& video_frame) = 0;

 protected:
  virtual ~VideoSendStreamInput() {}
};

class VideoSendStream {
 public:
  struct StreamStats {
    FrameCounts frame_counts;
    int width = 0;
    int height = 0;
    // TODO(holmer): Move bitrate_bps out to the webrtc::Call layer.
    int total_bitrate_bps = 0;
    int retransmit_bitrate_bps = 0;
    int avg_delay_ms = 0;
    int max_delay_ms = 0;
    StreamDataCounters rtp_stats;
    RtcpPacketTypeCounter rtcp_packet_type_counts;
    RtcpStatistics rtcp_stats;
  };

  struct Stats {
    Stats()
        : input_frame_rate(0),
          encode_frame_rate(0),
          avg_encode_time_ms(0),
          encode_usage_percent(0),
          target_media_bitrate_bps(0),
          media_bitrate_bps(0),
          suspended(false) {}
    int input_frame_rate;
    int encode_frame_rate;
    int avg_encode_time_ms;
    int encode_usage_percent;
    int target_media_bitrate_bps;
    int media_bitrate_bps;
    bool suspended;
    std::map<uint32_t, StreamStats> substreams;
  };

  struct Config {
    Config()
        : pre_encode_callback(NULL),
          post_encode_callback(NULL),
          local_renderer(NULL),
          render_delay_ms(0),
          target_delay_ms(0),
          suspend_below_min_bitrate(false) {}
    std::string ToString() const;

    struct EncoderSettings {
      EncoderSettings() : payload_type(-1), encoder(NULL) {}

      std::string ToString() const;

      std::string payload_name;
      int payload_type;

      // Uninitialized VideoEncoder instance to be used for encoding. Will be
      // initialized from inside the VideoSendStream.
      VideoEncoder* encoder;
    } encoder_settings;

    static const size_t kDefaultMaxPacketSize = 1500 - 40;  // TCP over IPv4.
    struct Rtp {
      Rtp() : max_packet_size(kDefaultMaxPacketSize) {}
      std::string ToString() const;

      std::vector<uint32_t> ssrcs;

      // Max RTP packet size delivered to send transport from VideoEngine.
      size_t max_packet_size;

      // RTP header extensions to use for this send stream.
      std::vector<RtpExtension> extensions;

      // See NackConfig for description.
      NackConfig nack;

      // See FecConfig for description.
      FecConfig fec;

      // Settings for RTP retransmission payload format, see RFC 4588 for
      // details.
      struct Rtx {
        Rtx() : payload_type(-1) {}
        std::string ToString() const;
        // SSRCs to use for the RTX streams.
        std::vector<uint32_t> ssrcs;

        // Payload type to use for the RTX stream.
        int payload_type;
      } rtx;

      // RTCP CNAME, see RFC 3550.
      std::string c_name;
    } rtp;

    // Called for each I420 frame before encoding the frame. Can be used for
    // effects, snapshots etc. 'NULL' disables the callback.
    I420FrameCallback* pre_encode_callback;

    // Called for each encoded frame, e.g. used for file storage. 'NULL'
    // disables the callback.
    EncodedFrameObserver* post_encode_callback;

    // Renderer for local preview. The local renderer will be called even if
    // sending hasn't started. 'NULL' disables local rendering.
    VideoRenderer* local_renderer;

    // Expected delay needed by the renderer, i.e. the frame will be delivered
    // this many milliseconds, if possible, earlier than expected render time.
    // Only valid if |local_renderer| is set.
    int render_delay_ms;

    // Target delay in milliseconds. A positive value indicates this stream is
    // used for streaming instead of a real-time call.
    int target_delay_ms;

    // True if the stream should be suspended when the available bitrate fall
    // below the minimum configured bitrate. If this variable is false, the
    // stream may send at a rate higher than the estimated available bitrate.
    bool suspend_below_min_bitrate;
  };

  // Gets interface used to insert captured frames. Valid as long as the
  // VideoSendStream is valid.
  virtual VideoSendStreamInput* Input() = 0;

  virtual void Start() = 0;
  virtual void Stop() = 0;

  // Set which streams to send. Must have at least as many SSRCs as configured
  // in the config. Encoder settings are passed on to the encoder instance along
  // with the VideoStream settings.
  virtual bool ReconfigureVideoEncoder(const VideoEncoderConfig& config) = 0;

  virtual Stats GetStats() = 0;

 protected:
  virtual ~VideoSendStream() {}
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_SEND_STREAM_H_
