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

#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"

namespace webrtc {

SendStatisticsProxy::SendStatisticsProxy(
    const VideoSendStream::Config& config)
    : config_(config),
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

void SendStatisticsProxy::SuspendChange(int video_channel, bool is_suspended) {
  CriticalSectionScoped lock(crit_.get());
  stats_.suspended = is_suspended;
}

void SendStatisticsProxy::CapturedFrameRate(const int capture_id,
                                            const unsigned char frame_rate) {
  CriticalSectionScoped lock(crit_.get());
  stats_.input_frame_rate = frame_rate;
}

VideoSendStream::Stats SendStatisticsProxy::GetStats() const {
  CriticalSectionScoped lock(crit_.get());
  return stats_;
}

SsrcStats* SendStatisticsProxy::GetStatsEntry(uint32_t ssrc) {
  std::map<uint32_t, SsrcStats>::iterator it = stats_.substreams.find(ssrc);
  if (it != stats_.substreams.end())
    return &it->second;

  if (std::find(config_.rtp.ssrcs.begin(), config_.rtp.ssrcs.end(), ssrc) ==
          config_.rtp.ssrcs.end() &&
      std::find(config_.rtp.rtx.ssrcs.begin(),
                config_.rtp.rtx.ssrcs.end(),
                ssrc) == config_.rtp.rtx.ssrcs.end()) {
    return NULL;
  }

  return &stats_.substreams[ssrc];  // Insert new entry and return ptr.
}

void SendStatisticsProxy::StatisticsUpdated(const RtcpStatistics& statistics,
                                            uint32_t ssrc) {
  CriticalSectionScoped lock(crit_.get());
  SsrcStats* stats = GetStatsEntry(ssrc);
  if (stats == NULL)
    return;

  stats->rtcp_stats = statistics;
}

void SendStatisticsProxy::DataCountersUpdated(
    const StreamDataCounters& counters,
    uint32_t ssrc) {
  CriticalSectionScoped lock(crit_.get());
  SsrcStats* stats = GetStatsEntry(ssrc);
  if (stats == NULL)
    return;

  stats->rtp_stats = counters;
}

void SendStatisticsProxy::Notify(const BitrateStatistics& total_stats,
                                 const BitrateStatistics& retransmit_stats,
                                 uint32_t ssrc) {
  CriticalSectionScoped lock(crit_.get());
  SsrcStats* stats = GetStatsEntry(ssrc);
  if (stats == NULL)
    return;

  stats->total_bitrate_bps = total_stats.bitrate_bps;
  stats->retransmit_bitrate_bps = retransmit_stats.bitrate_bps;
}

void SendStatisticsProxy::FrameCountUpdated(FrameType frame_type,
                                            uint32_t frame_count,
                                            const unsigned int ssrc) {
  CriticalSectionScoped lock(crit_.get());
  SsrcStats* stats = GetStatsEntry(ssrc);
  if (stats == NULL)
    return;

  switch (frame_type) {
    case kVideoFrameDelta:
      stats->delta_frames = frame_count;
      break;
    case kVideoFrameKey:
      stats->key_frames = frame_count;
      break;
    case kFrameEmpty:
    case kAudioFrameSpeech:
    case kAudioFrameCN:
      break;
  }
}

void SendStatisticsProxy::SendSideDelayUpdated(int avg_delay_ms,
                                               int max_delay_ms,
                                               uint32_t ssrc) {
  CriticalSectionScoped lock(crit_.get());
  SsrcStats* stats = GetStatsEntry(ssrc);
  if (stats == NULL)
    return;
  stats->avg_delay_ms = avg_delay_ms;
  stats->max_delay_ms = max_delay_ms;
}

}  // namespace webrtc
