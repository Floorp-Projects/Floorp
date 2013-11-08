/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_VIDEO_ENGINE_NEW_INCLUDE_CALL_H_
#define WEBRTC_VIDEO_ENGINE_NEW_INCLUDE_CALL_H_

#include <string>
#include <vector>

#include "webrtc/common_types.h"
#include "webrtc/video_engine/new_include/video_receive_stream.h"
#include "webrtc/video_engine/new_include/video_send_stream.h"

namespace webrtc {

class VoiceEngine;

const char* Version();

class PacketReceiver {
 public:
  virtual bool DeliverPacket(const uint8_t* packet, size_t length) = 0;

 protected:
  virtual ~PacketReceiver() {}
};

// A Call instance can contain several send and/or receive streams. All streams
// are assumed to have the same remote endpoint and will share bitrate estimates
// etc.
class Call {
 public:
  struct Config {
    explicit Config(newapi::Transport* send_transport)
        : send_transport(send_transport),
          overuse_detection(false),
          voice_engine(NULL),
          trace_callback(NULL),
          trace_filter(kTraceNone) {}

    newapi::Transport* send_transport;
    bool overuse_detection;

    // VoiceEngine used for audio/video synchronization for this Call.
    VoiceEngine* voice_engine;

    TraceCallback* trace_callback;
    uint32_t trace_filter;
  };

  static Call* Create(const Call::Config& config);

  virtual std::vector<VideoCodec> GetVideoCodecs() = 0;

  virtual VideoSendStream::Config GetDefaultSendConfig() = 0;

  virtual VideoSendStream* CreateSendStream(
      const VideoSendStream::Config& config) = 0;

  // Returns the internal state of the send stream, for resume sending with a
  // new stream with different settings.
  // Note: Only the last returned send-stream state is valid.
  virtual SendStreamState* DestroySendStream(VideoSendStream* send_stream) = 0;

  virtual VideoReceiveStream::Config GetDefaultReceiveConfig() = 0;

  virtual VideoReceiveStream* CreateReceiveStream(
      const VideoReceiveStream::Config& config) = 0;
  virtual void DestroyReceiveStream(VideoReceiveStream* receive_stream) = 0;

  // All received RTP and RTCP packets for the call should be inserted to this
  // PacketReceiver. The PacketReceiver pointer is valid as long as the
  // Call instance exists.
  virtual PacketReceiver* Receiver() = 0;

  // Returns the estimated total send bandwidth. Note: this can differ from the
  // actual encoded bitrate.
  virtual uint32_t SendBitrateEstimate() = 0;

  // Returns the total estimated receive bandwidth for the call. Note: this can
  // differ from the actual receive bitrate.
  virtual uint32_t ReceiveBitrateEstimate() = 0;

  virtual ~Call() {}
};
}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_NEW_INCLUDE_CALL_H_
