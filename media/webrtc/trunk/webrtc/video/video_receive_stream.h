/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_VIDEO_RECEIVE_STREAM_H_
#define WEBRTC_VIDEO_VIDEO_RECEIVE_STREAM_H_

#include <vector>

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/call.h"
#include "webrtc/call/transport_adapter.h"
#include "webrtc/common_video/include/incoming_video_stream.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/video_render/video_render_defines.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/video/encoded_frame_callback_adapter.h"
#include "webrtc/video/receive_statistics_proxy.h"
#include "webrtc/video/vie_channel.h"
#include "webrtc/video/vie_encoder.h"
#include "webrtc/video_encoder.h"
#include "webrtc/video_receive_stream.h"

namespace webrtc {

class CallStats;
class CongestionController;
class VoiceEngine;

namespace internal {

class VideoReceiveStream : public webrtc::VideoReceiveStream,
                           public I420FrameCallback,
                           public VideoRenderCallback,
                           public EncodedImageCallback {
 public:
  VideoReceiveStream(int num_cpu_cores,
                     CongestionController* congestion_controller,
                     const VideoReceiveStream::Config& config,
                     webrtc::VoiceEngine* voice_engine,
                     ProcessThread* process_thread,
                     CallStats* call_stats);
  ~VideoReceiveStream() override;

  // webrtc::ReceiveStream implementation.
  void Start() override;
  void Stop() override;
  void SignalNetworkState(NetworkState state) override;
  bool DeliverRtcp(const uint8_t* packet, size_t length) override;
  bool DeliverRtp(const uint8_t* packet,
                  size_t length,
                  const PacketTime& packet_time) override;

  // webrtc::VideoReceiveStream implementation.
  webrtc::VideoReceiveStream::Stats GetStats() const override;

  // Overrides I420FrameCallback.
  void FrameCallback(VideoFrame* video_frame) override;

  // Overrides VideoRenderCallback.
  int RenderFrame(const uint32_t /*stream_id*/,
                  const VideoFrame& video_frame) override;

  // Overrides EncodedImageCallback.
  int32_t Encoded(const EncodedImage& encoded_image,
                  const CodecSpecificInfo* codec_specific_info,
                  const RTPFragmentationHeader* fragmentation) override;

  const Config& config() const { return config_; }

  void SetSyncChannel(VoiceEngine* voice_engine, int audio_channel_id);

 private:
  TransportAdapter transport_adapter_;
  EncodedFrameCallbackAdapter encoded_frame_proxy_;
  const VideoReceiveStream::Config config_;
  Clock* const clock_;

  CongestionController* const congestion_controller_;
  CallStats* const call_stats_;

  rtc::scoped_ptr<IncomingVideoStream> incoming_video_stream_;
  rtc::scoped_ptr<ReceiveStatisticsProxy> stats_proxy_;
  rtc::scoped_ptr<ViEChannel> vie_channel_;
};
}  // namespace internal
}  // namespace webrtc

#endif  // WEBRTC_VIDEO_VIDEO_RECEIVE_STREAM_H_
