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

ReceiveStatisticsProxy::ReceiveStatisticsProxy(uint32_t ssrc, Clock* clock)
    : clock_(clock),
      crit_(CriticalSectionWrapper::CreateCriticalSection()),
      // 1000ms window, scale 1000 for ms to s.
      decode_fps_estimator_(1000, 1000),
      renders_fps_estimator_(1000, 1000),
      receive_state_(kReceiveStateInitial) {
  stats_.ssrc = ssrc;
}

ReceiveStatisticsProxy::~ReceiveStatisticsProxy() {}

VideoReceiveStream::Stats ReceiveStatisticsProxy::GetStats() const {
  CriticalSectionScoped lock(crit_.get());
  return stats_;
}

void ReceiveStatisticsProxy::IncomingRate(const int video_channel,
                                          const unsigned int framerate,
                                          const unsigned int bitrate_bps) {
  CriticalSectionScoped lock(crit_.get());
  stats_.network_frame_rate = framerate;
  stats_.total_bitrate_bps = bitrate_bps;
}

void ReceiveStatisticsProxy::ReceiveStateChange(const int video_channel,
                                                VideoReceiveState state) {
  CriticalSectionScoped cs(lock_.get());
  receive_state_ = state;
}

void ReceiveStatisticsProxy::DecoderTiming(int decode_ms,
                                           int max_decode_ms,
                                           int current_delay_ms,
                                           int target_delay_ms,
                                           int jitter_buffer_ms,
                                           int min_playout_delay_ms,
                                           int render_delay_ms) {
  CriticalSectionScoped lock(crit_.get());
  stats_.decode_ms = decode_ms;
  stats_.max_decode_ms = max_decode_ms;
  stats_.current_delay_ms = current_delay_ms;
  stats_.target_delay_ms = target_delay_ms;
  stats_.jitter_buffer_ms = jitter_buffer_ms;
  stats_.min_playout_delay_ms = min_playout_delay_ms;
  stats_.render_delay_ms = render_delay_ms;
}

void ReceiveStatisticsProxy::RtcpPacketTypesCounterUpdated(
    uint32_t ssrc,
    const RtcpPacketTypeCounter& packet_counter) {
  CriticalSectionScoped lock(crit_.get());
  if (stats_.ssrc != ssrc)
    return;
  stats_.rtcp_packet_type_counts = packet_counter;
}

void ReceiveStatisticsProxy::StatisticsUpdated(
    const webrtc::RtcpStatistics& statistics,
    uint32_t ssrc) {
  CriticalSectionScoped lock(crit_.get());
  // TODO(pbos): Handle both local and remote ssrcs here and DCHECK that we
  // receive stats from one of them.
  if (stats_.ssrc != ssrc)
    return;
  stats_.rtcp_stats = statistics;
}

void ReceiveStatisticsProxy::CNameChanged(const char* cname, uint32_t ssrc) {
  CriticalSectionScoped lock(crit_.get());
  // TODO(pbos): Handle both local and remote ssrcs here and DCHECK that we
  // receive stats from one of them.
  if (stats_.ssrc != ssrc)
    return;
  stats_.c_name = cname;
}

void ReceiveStatisticsProxy::DataCountersUpdated(
    const webrtc::StreamDataCounters& counters,
    uint32_t ssrc) {
  CriticalSectionScoped lock(crit_.get());
  if (stats_.ssrc != ssrc)
    return;
  stats_.rtp_stats = counters;
}

void ReceiveStatisticsProxy::OnDecodedFrame() {
  uint64_t now = clock_->TimeInMilliseconds();

  CriticalSectionScoped lock(crit_.get());
  decode_fps_estimator_.Update(1, now);
  stats_.decode_frame_rate = decode_fps_estimator_.Rate(now);
}

void ReceiveStatisticsProxy::OnRenderedFrame() {
  uint64_t now = clock_->TimeInMilliseconds();

  CriticalSectionScoped lock(crit_.get());
  renders_fps_estimator_.Update(1, now);
  stats_.render_frame_rate = renders_fps_estimator_.Rate(now);
}

void ReceiveStatisticsProxy::OnReceiveRatesUpdated(uint32_t bitRate,
                                                   uint32_t frameRate) {
}

void ReceiveStatisticsProxy::OnFrameCountsUpdated(
    const FrameCounts& frame_counts) {
  CriticalSectionScoped lock(crit_.get());
  stats_.frame_counts = frame_counts;
}

void ReceiveStatisticsProxy::OnDiscardedPacketsUpdated(int discarded_packets) {
  CriticalSectionScoped lock(crit_.get());
  stats_.discarded_packets = discarded_packets;
}

}  // namespace webrtc
