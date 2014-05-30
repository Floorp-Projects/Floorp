/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video/receive_statistics_proxy.h"

#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"

namespace webrtc {
namespace internal {

ReceiveStatisticsProxy::ReceiveStatisticsProxy(uint32_t ssrc,
                                               Clock* clock,
                                               ViERTP_RTCP* rtp_rtcp,
                                               ViECodec* codec,
                                               int channel)
    : channel_(channel),
      lock_(CriticalSectionWrapper::CreateCriticalSection()),
      clock_(clock),
      // 1000ms window, scale 1000 for ms to s.
      decode_fps_estimator_(1000, 1000),
      renders_fps_estimator_(1000, 1000),
      codec_(codec),
      rtp_rtcp_(rtp_rtcp) {
  stats_.ssrc = ssrc;
}

ReceiveStatisticsProxy::~ReceiveStatisticsProxy() {}

VideoReceiveStream::Stats ReceiveStatisticsProxy::GetStats() const {
  VideoReceiveStream::Stats stats;
  {
    CriticalSectionScoped cs(lock_.get());
    stats = stats_;
  }
  stats.c_name = GetCName();
  codec_->GetReceiveSideDelay(channel_, &stats.avg_delay_ms);
  stats.discarded_packets = codec_->GetDiscardedPackets(channel_);
  codec_->GetReceiveCodecStastistics(
      channel_, stats.key_frames, stats.delta_frames);

  return stats;
}

std::string ReceiveStatisticsProxy::GetCName() const {
  char rtcp_cname[ViERTP_RTCP::KMaxRTCPCNameLength];
  if (rtp_rtcp_->GetRemoteRTCPCName(channel_, rtcp_cname) != 0)
    rtcp_cname[0] = '\0';
  return rtcp_cname;
}

void ReceiveStatisticsProxy::IncomingRate(const int video_channel,
                                          const unsigned int framerate,
                                          const unsigned int bitrate) {
  CriticalSectionScoped cs(lock_.get());
  stats_.network_frame_rate = framerate;
  stats_.bitrate_bps = bitrate;
}

void ReceiveStatisticsProxy::StatisticsUpdated(
    const webrtc::RtcpStatistics& statistics,
    uint32_t ssrc) {
  CriticalSectionScoped cs(lock_.get());

  stats_.rtcp_stats = statistics;
}

void ReceiveStatisticsProxy::DataCountersUpdated(
    const webrtc::StreamDataCounters& counters,
    uint32_t ssrc) {
  CriticalSectionScoped cs(lock_.get());

  stats_.rtp_stats = counters;
}

void ReceiveStatisticsProxy::OnDecodedFrame() {
  uint64_t now = clock_->TimeInMilliseconds();

  CriticalSectionScoped cs(lock_.get());
  decode_fps_estimator_.Update(1, now);
  stats_.decode_frame_rate = decode_fps_estimator_.Rate(now);
}

void ReceiveStatisticsProxy::OnRenderedFrame() {
  uint64_t now = clock_->TimeInMilliseconds();

  CriticalSectionScoped cs(lock_.get());
  renders_fps_estimator_.Update(1, now);
  stats_.render_frame_rate = renders_fps_estimator_.Rate(now);
}

}  // namespace internal
}  // namespace webrtc
