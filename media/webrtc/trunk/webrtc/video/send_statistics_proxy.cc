/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video/send_statistics_proxy.h"

#include <map>

#include "webrtc/base/checks.h"

#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/logging.h"

namespace webrtc {

const int SendStatisticsProxy::kStatsTimeoutMs = 5000;

SendStatisticsProxy::SendStatisticsProxy(Clock* clock,
                                         const VideoSendStream::Config& config)
    : clock_(clock),
      config_(config),
      crit_(CriticalSectionWrapper::CreateCriticalSection()) {
}

SendStatisticsProxy::~SendStatisticsProxy() {}

void SendStatisticsProxy::OutgoingRate(const int video_channel,
                                       const unsigned int framerate,
                                       const unsigned int bitrate) {
  CriticalSectionScoped lock(crit_.get());
  stats_.encode_frame_rate = framerate;
  stats_.media_bitrate_bps = bitrate;
}

void SendStatisticsProxy::CpuOveruseMetricsUpdated(
    const CpuOveruseMetrics& metrics) {
  CriticalSectionScoped lock(crit_.get());
  stats_.avg_encode_time_ms = metrics.avg_encode_time_ms;
  stats_.encode_usage_percent = metrics.encode_usage_percent;
}

void SendStatisticsProxy::SuspendChange(int video_channel, bool is_suspended) {
  CriticalSectionScoped lock(crit_.get());
  stats_.suspended = is_suspended;
}

VideoSendStream::Stats SendStatisticsProxy::GetStats() {
  CriticalSectionScoped lock(crit_.get());
  PurgeOldStats();
  stats_.input_frame_rate =
      static_cast<int>(input_frame_rate_tracker_.units_second());
  return stats_;
}

void SendStatisticsProxy::PurgeOldStats() {
  int64_t current_time_ms = clock_->TimeInMilliseconds();
  for (std::map<uint32_t, VideoSendStream::StreamStats>::iterator it =
           stats_.substreams.begin();
       it != stats_.substreams.end(); ++it) {
    uint32_t ssrc = it->first;
    if (update_times_[ssrc].resolution_update_ms + kStatsTimeoutMs >
        current_time_ms)
      continue;

    it->second.width = 0;
    it->second.height = 0;
  }
}

VideoSendStream::StreamStats* SendStatisticsProxy::GetStatsEntry(
    uint32_t ssrc) {
  std::map<uint32_t, VideoSendStream::StreamStats>::iterator it =
      stats_.substreams.find(ssrc);
  if (it != stats_.substreams.end())
    return &it->second;

  if (std::find(config_.rtp.ssrcs.begin(), config_.rtp.ssrcs.end(), ssrc) ==
          config_.rtp.ssrcs.end() &&
      std::find(config_.rtp.rtx.ssrcs.begin(),
                config_.rtp.rtx.ssrcs.end(),
                ssrc) == config_.rtp.rtx.ssrcs.end()) {
    return nullptr;
  }

  return &stats_.substreams[ssrc];  // Insert new entry and return ptr.
}

void SendStatisticsProxy::OnSetRates(uint32_t bitrate_bps, int framerate) {
  CriticalSectionScoped lock(crit_.get());
  stats_.target_media_bitrate_bps = bitrate_bps;
}

void SendStatisticsProxy::OnSendEncodedImage(
    const EncodedImage& encoded_image,
    const RTPVideoHeader* rtp_video_header) {
  size_t simulcast_idx =
      rtp_video_header != nullptr ? rtp_video_header->simulcastIdx : 0;
  if (simulcast_idx >= config_.rtp.ssrcs.size()) {
    LOG(LS_ERROR) << "Encoded image outside simulcast range (" << simulcast_idx
                  << " >= " << config_.rtp.ssrcs.size() << ").";
    return;
  }
  uint32_t ssrc = config_.rtp.ssrcs[simulcast_idx];

  CriticalSectionScoped lock(crit_.get());
  VideoSendStream::StreamStats* stats = GetStatsEntry(ssrc);
  if (stats == nullptr)
    return;

  stats->width = encoded_image._encodedWidth;
  stats->height = encoded_image._encodedHeight;
  update_times_[ssrc].resolution_update_ms = clock_->TimeInMilliseconds();
}

void SendStatisticsProxy::OnIncomingFrame() {
  CriticalSectionScoped lock(crit_.get());
  input_frame_rate_tracker_.Update(1);
}

void SendStatisticsProxy::RtcpPacketTypesCounterUpdated(
    uint32_t ssrc,
    const RtcpPacketTypeCounter& packet_counter) {
  CriticalSectionScoped lock(crit_.get());
  VideoSendStream::StreamStats* stats = GetStatsEntry(ssrc);
  if (stats == nullptr)
    return;

  stats->rtcp_packet_type_counts = packet_counter;
}

void SendStatisticsProxy::StatisticsUpdated(const RtcpStatistics& statistics,
                                            uint32_t ssrc) {
  CriticalSectionScoped lock(crit_.get());
  VideoSendStream::StreamStats* stats = GetStatsEntry(ssrc);
  if (stats == nullptr)
    return;

  stats->rtcp_stats = statistics;
}

void SendStatisticsProxy::CNameChanged(const char* cname, uint32_t ssrc) {
}

void SendStatisticsProxy::DataCountersUpdated(
    const StreamDataCounters& counters,
    uint32_t ssrc) {
  CriticalSectionScoped lock(crit_.get());
  VideoSendStream::StreamStats* stats = GetStatsEntry(ssrc);
  DCHECK(stats != nullptr) << "DataCountersUpdated reported for unknown ssrc: "
                           << ssrc;

  stats->rtp_stats = counters;
}

void SendStatisticsProxy::Notify(const BitrateStatistics& total_stats,
                                 const BitrateStatistics& retransmit_stats,
                                 uint32_t ssrc) {
  CriticalSectionScoped lock(crit_.get());
  VideoSendStream::StreamStats* stats = GetStatsEntry(ssrc);
  if (stats == nullptr)
    return;

  stats->total_bitrate_bps = total_stats.bitrate_bps;
  stats->retransmit_bitrate_bps = retransmit_stats.bitrate_bps;
}

void SendStatisticsProxy::FrameCountUpdated(const FrameCounts& frame_counts,
                                            uint32_t ssrc) {
  CriticalSectionScoped lock(crit_.get());
  VideoSendStream::StreamStats* stats = GetStatsEntry(ssrc);
  if (stats == nullptr)
    return;

  stats->frame_counts = frame_counts;
}

void SendStatisticsProxy::SendSideDelayUpdated(int avg_delay_ms,
                                               int max_delay_ms,
                                               uint32_t ssrc) {
  CriticalSectionScoped lock(crit_.get());
  VideoSendStream::StreamStats* stats = GetStatsEntry(ssrc);
  if (stats == nullptr)
    return;
  stats->avg_delay_ms = avg_delay_ms;
  stats->max_delay_ms = max_delay_ms;
}

}  // namespace webrtc
