/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_NEW_INCLUDE_VIDEO_RECEIVE_STREAM_H_
#define WEBRTC_VIDEO_ENGINE_NEW_INCLUDE_VIDEO_RECEIVE_STREAM_H_

#include <string>
#include <vector>

#include "webrtc/common_types.h"
#include "webrtc/video_engine/new_include/common.h"

namespace webrtc {

class VideoDecoder;

namespace newapi {

struct ReceiveStatistics {
  RtpStatistics rtp_stats;
  int network_frame_rate;
  int decode_frame_rate;
  int render_frame_rate;
  uint32_t key_frames;
  uint32_t delta_frames;
  uint32_t video_packets;
  uint32_t retransmitted_packets;
  uint32_t fec_packets;
  uint32_t padding_packets;
  uint32_t discarded_packets;
  int32_t received_bitrate_bps;
  int receive_side_delay_ms;
};

// Receive stream specific RTP settings.
struct RtpReceiveConfig {
  // TODO(mflodman) Do we require a set ssrc? What happens if the ssrc changes?
  uint32_t ssrc;

  // See NackConfig for description, 'NULL' disables NACK.
  NackConfig* nack;

  // See FecConfig for description, 'NULL' disables FEC.
  FecConfig* fec;

  // RTX settings for possible payloads. RTX is disabled if the vector is empty.
  std::vector<RtxConfig> rtx;

  // RTP header extensions used for the received stream.
  std::vector<RtpExtension> rtp_extensions;
};

// TODO(mflodman) Move all these settings to VideoDecoder and move the
// declaration to common_types.h.
struct ExternalVideoDecoder {
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

struct VideoReceiveStreamConfig {
  // Codecs the receive stream
  std::vector<VideoCodec> codecs;

  RtpReceiveConfig rtp;

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

  // Called for each incoming video frame, i.e. in encoded state. E.g. used when
  // saving the stream to a file. 'NULL' disables the callback.
  EncodedFrameObserver* pre_decode_callback;

  // Called for each decoded frame. E.g. used when adding effects to the decoded
  // stream. 'NULL' disables the callback.
  I420FrameCallback* post_decode_callback;

  // External video decoders to be used if incoming payload type matches the
  // registered type for an external decoder.
  std::vector<ExternalVideoDecoder> external_decoders;

  // Target delay in milliseconds. A positive value indicates this stream is
  // used for streaming instead of a real-time call.
  int target_delay_ms;
};

class VideoReceiveStream {
 public:
  virtual void StartReceive() = 0;
  virtual void StopReceive() = 0;

  // TODO(mflodman) Replace this with callback.
  virtual void GetCurrentReceiveCodec(VideoCodec* receive_codec) = 0;

  virtual void GetReceiveStatistics(ReceiveStatistics* statistics) = 0;

 protected:
  virtual ~VideoReceiveStream() {}
};

}  // namespace newapi
}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_NEW_INCLUDE_VIDEO_RECEIVE_STREAM_H_
