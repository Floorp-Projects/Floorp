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
#include "webrtc/transport.h"
#include "webrtc/video_renderer.h"

namespace webrtc {

namespace newapi {
// RTCP mode to use. Compound mode is described by RFC 4585 and reduced-size
// RTCP mode is described by RFC 5506.
enum RtcpMode { kRtcpCompound, kRtcpReducedSize };
}  // namespace newapi

class VideoDecoder;

// TODO(mflodman) Move all these settings to VideoDecoder and move the
// declaration to common_types.h.
struct ExternalVideoDecoder {
  ExternalVideoDecoder()
      : decoder(NULL), payload_type(0), renderer(false), expected_delay_ms(0) {}
  // The actual decoder.
  VideoDecoder* decoder;

  // Received RTP packets with this payload type will be sent to this decoder
  // instance.
  int payload_type;

  // 'true' if the decoder handles rendering as well.
  bool renderer;

  // The expected delay for decoding and rendering, i.e. the frame will be
  // delivered this many milliseconds, if possible, earlier than the ideal
  // render time.
  // Note: Ignored if 'renderer' is false.
  int expected_delay_ms;
};

class VideoReceiveStream {
 public:
  struct Stats : public StreamStats {
    Stats()
        : network_frame_rate(0),
          decode_frame_rate(0),
          render_frame_rate(0),
          avg_delay_ms(0),
          discarded_packets(0),
          ssrc(0) {}

    int network_frame_rate;
    int decode_frame_rate;
    int render_frame_rate;
    int avg_delay_ms;
    uint32_t discarded_packets;
    uint32_t ssrc;
    std::string c_name;
  };

  struct Config {
    Config()
        : renderer(NULL),
          render_delay_ms(0),
          audio_channel_id(0),
          pre_decode_callback(NULL),
          pre_render_callback(NULL),
          target_delay_ms(0) {}
    // Codecs the receive stream can receive.
    std::vector<VideoCodec> codecs;

    // Receive-stream specific RTP settings.
    struct Rtp {
      Rtp()
          : remote_ssrc(0),
            local_ssrc(0),
            rtcp_mode(newapi::kRtcpReducedSize),
            remb(false) {}

      // Synchronization source (stream identifier) to be received.
      uint32_t remote_ssrc;
      // Sender SSRC used for sending RTCP (such as receiver reports).
      uint32_t local_ssrc;

      // See RtcpMode for description.
      newapi::RtcpMode rtcp_mode;

      // Extended RTCP settings.
      struct RtcpXr {
        RtcpXr() : receiver_reference_time_report(false) {}

        // True if RTCP Receiver Reference Time Report Block extension
        // (RFC 3611) should be enabled.
        bool receiver_reference_time_report;
      } rtcp_xr;

      // See draft-alvestrand-rmcat-remb for information.
      bool remb;

      // See NackConfig for description.
      NackConfig nack;

      // See FecConfig for description.
      FecConfig fec;

      // RTX settings for incoming video payloads that may be received. RTX is
      // disabled if there's no config present.
      struct Rtx {
        Rtx() : ssrc(0), payload_type(0) {}

        // SSRCs to use for the RTX streams.
        uint32_t ssrc;

        // Payload type to use for the RTX stream.
        int payload_type;
      };

      // Map from video RTP payload type -> RTX config.
      typedef std::map<int, Rtx> RtxMap;
      RtxMap rtx;

      // RTP header extensions used for the received stream.
      std::vector<RtpExtension> extensions;
    } rtp;

    // VideoRenderer will be called for each decoded frame. 'NULL' disables
    // rendering of this stream.
    VideoRenderer* renderer;

    // Expected delay needed by the renderer, i.e. the frame will be delivered
    // this many milliseconds, if possible, earlier than the ideal render time.
    // Only valid if 'renderer' is set.
    int render_delay_ms;

    // Audio channel corresponding to this video stream, used for audio/video
    // synchronization. 'audio_channel_id' is ignored if no VoiceEngine is set
    // when creating the VideoEngine instance. '-1' disables a/v sync.
    int audio_channel_id;

    // Called for each incoming video frame, i.e. in encoded state. E.g. used
    // when
    // saving the stream to a file. 'NULL' disables the callback.
    EncodedFrameObserver* pre_decode_callback;

    // Called for each decoded frame. E.g. used when adding effects to the
    // decoded
    // stream. 'NULL' disables the callback.
    I420FrameCallback* pre_render_callback;

    // External video decoders to be used if incoming payload type matches the
    // registered type for an external decoder.
    std::vector<ExternalVideoDecoder> external_decoders;

    // Target delay in milliseconds. A positive value indicates this stream is
    // used for streaming instead of a real-time call.
    int target_delay_ms;
  };

  virtual void StartReceiving() = 0;
  virtual void StopReceiving() = 0;
  virtual Stats GetStats() const = 0;

  // TODO(mflodman) Replace this with callback.
  virtual void GetCurrentReceiveCodec(VideoCodec* receive_codec) = 0;

 protected:
  virtual ~VideoReceiveStream() {}
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_RECEIVE_STREAM_H_
