/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_VIDEO_SEND_STREAM_H_
#define WEBRTC_VIDEO_VIDEO_SEND_STREAM_H_

#include <map>
#include <vector>

#include "webrtc/call.h"
#include "webrtc/call/transport_adapter.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/system_wrappers/include/critical_section_wrapper.h"
#include "webrtc/video/encoded_frame_callback_adapter.h"
#include "webrtc/video/send_statistics_proxy.h"
#include "webrtc/video/video_capture_input.h"
#include "webrtc/video_receive_stream.h"
#include "webrtc/video_send_stream.h"

namespace webrtc {

class BitrateAllocator;
class CallStats;
class CongestionController;
class EncoderStateFeedback;
class ProcessThread;
class ViEChannel;
class ViEEncoder;

namespace internal {

class VideoSendStream : public webrtc::VideoSendStream,
                        public webrtc::CpuOveruseObserver {
 public:
  VideoSendStream(int num_cpu_cores,
                  ProcessThread* module_process_thread,
                  CallStats* call_stats,
                  CongestionController* congestion_controller,
                  BitrateAllocator* bitrate_allocator,
                  const VideoSendStream::Config& config,
                  const VideoEncoderConfig& encoder_config,
                  const std::map<uint32_t, RtpState>& suspended_ssrcs);

  ~VideoSendStream() override;

  // webrtc::SendStream implementation.
  void Start() override;
  void Stop() override;
  void SignalNetworkState(NetworkState state) override;
  bool DeliverRtcp(const uint8_t* packet, size_t length) override;

  // webrtc::VideoSendStream implementation.
  VideoCaptureInput* Input() override;
  CPULoadStateObserver* LoadStateObserver() override;
  bool ReconfigureVideoEncoder(const VideoEncoderConfig& config) override;
  Stats GetStats() override;

  // webrtc::CpuOveruseObserver implementation.
  void OveruseDetected() override;
  void NormalUsage() override;

  typedef std::map<uint32_t, RtpState> RtpStateMap;
  RtpStateMap GetRtpStates() const;

  int64_t GetRtt() const;
  int GetPaddingNeededBps() const;

 private:
  bool SetSendCodec(VideoCodec video_codec);
  void ConfigureSsrcs();

  SendStatisticsProxy stats_proxy_;
  TransportAdapter transport_adapter_;
  EncodedFrameCallbackAdapter encoded_frame_proxy_;
  const VideoSendStream::Config config_;
  VideoEncoderConfig encoder_config_;
  std::map<uint32_t, RtpState> suspended_ssrcs_;

  ProcessThread* const module_process_thread_;
  CallStats* const call_stats_;
  CongestionController* const congestion_controller_;

  rtc::scoped_ptr<VideoCaptureInput> input_;
  rtc::scoped_ptr<ViEChannel> vie_channel_;
  rtc::scoped_ptr<ViEEncoder> vie_encoder_;
  rtc::scoped_ptr<EncoderStateFeedback> encoder_feedback_;

  // Used as a workaround to indicate that we should be using the configured
  // start bitrate initially, instead of the one reported by VideoEngine (which
  // defaults to too high).
  bool use_config_bitrate_;
};
}  // namespace internal
}  // namespace webrtc

#endif  // WEBRTC_VIDEO_VIDEO_SEND_STREAM_H_
