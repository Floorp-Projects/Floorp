/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_NEW_INCLUDE_VIDEO_SEND_STREAM_H_
#define WEBRTC_VIDEO_ENGINE_NEW_INCLUDE_VIDEO_SEND_STREAM_H_

#include <string>
#include <vector>

#include "webrtc/common_types.h"
#include "webrtc/video_engine/new_include/common.h"

namespace webrtc {

class VideoEncoder;

namespace newapi {


struct SendStreamState;

struct SendStatistics {
  RtpStatistics rtp;
  int input_frame_rate;
  int encode_frame;
  uint32_t key_frames;
  uint32_t delta_frames;
  uint32_t video_packets;
  uint32_t retransmitted_packets;
  uint32_t fec_packets;
  uint32_t padding_packets;
  int32_t send_bitrate_bps;
  int delay_ms;
};

// Class to deliver captured frame to the video send stream.
class VideoSendStreamInput {
 public:
  // TODO(mflodman) Replace time_since_capture_ms when I420VideoFrame uses NTP
  // time.
  virtual void PutFrame(const I420VideoFrame& video_frame,
                        int time_since_capture_ms) = 0;

 protected:
  virtual ~VideoSendStreamInput() {}
};

struct RtpSendConfig {
  RtcpMode mode;

  std::vector<uint32_t> ssrcs;

  // Max RTP packet size delivered to send transport from VideoEngine.
  size_t max_packet_size;

  // RTP header extensions to use for this send stream.
  std::vector<RtpExtension> rtp_extensions;

  // 'NULL' disables NACK.
  NackConfig* nack;

  // 'NULL' disables FEC.
  FecConfig* fec;

  // 'NULL' disables RTX.
  RtxConfig* rtx;

  // RTCP CNAME, see RFC 3550.
  std::string c_name;
};

struct VideoSendStreamConfig {
  VideoCodec codec;

  RtpSendConfig rtp;

  // Called for each I420 frame before encoding the frame. Can be used for
  // effects, snapshots etc. 'NULL' disables the callback.
  I420FrameCallback* pre_encode_callback;

  // Called for each encoded frame, e.g. used for file storage. 'NULL' disables
  // the callback.
  EncodedFrameObserver* encoded_callback;

  // Renderer for local preview. The local renderer will be called even if
  // sending hasn't started. 'NULL' disables local rendering.
  VideoRenderer* local_renderer;

  // Expected delay needed by the renderer, i.e. the frame will be delivered
  // this many milliseconds, if possible, earlier than expected render time.
  // Only valid if |renderer| is set.
  int render_delay_ms;

  // TODO(mflodman) Move VideoEncoder to common_types.h and redefine.
  // External encoding. 'encoder' is the external encoder instance and
  // 'internal_source' is set to true if the encoder also captures the video
  // frames.
  VideoEncoder* encoder;
  bool internal_source;

  // Target delay in milliseconds. A positive value indicates this stream is
  // used for streaming instead of a real-time call.
  int target_delay_ms;

  // Set to resume a previously destroyed send stream.
  SendStreamState* start_state;
};

class VideoSendStream {
 public:
  // Gets interface used to insert captured frames. Valid as long as the
  // VideoSendStream is valid.
  virtual VideoSendStreamInput* Input() = 0;

  virtual void StartSend() = 0;
  virtual void StopSend() = 0;

  // Gets the current statistics for the send stream.
  virtual void GetSendStatistics(std::vector<SendStatistics>* statistics) = 0;

  // TODO(mflodman) Change VideoCodec struct and use here.
  virtual bool SetTargetBitrate(
      int min_bitrate, int max_bitrate,
      const std::vector<SimulcastStream>& streams) = 0;

  virtual void GetSendCodec(VideoCodec* send_codec) = 0;

 protected:
  virtual ~VideoSendStream() {}
};

}  // namespace newapi
}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_NEW_INCLUDE_VIDEO_SEND_STREAM_H_
