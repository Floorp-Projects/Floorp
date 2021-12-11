/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/send_statistics_proxy.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

#include "common_types.h"  // NOLINT(build/include)
#include "modules/video_coding/include/video_codec_interface.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/field_trial.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {
namespace {
const float kEncodeTimeWeigthFactor = 0.5f;
const size_t kMaxEncodedFrameMapSize = 150;
const int64_t kMaxEncodedFrameWindowMs = 800;
const int64_t kBucketSizeMs = 100;
const size_t kBucketCount = 10;

const char kVp8ForcedFallbackEncoderFieldTrial[] =
    "WebRTC-VP8-Forced-Fallback-Encoder-v2";
const char kVp8SwCodecName[] = "libvpx";

// Used by histograms. Values of entries should not be changed.
enum HistogramCodecType {
  kVideoUnknown = 0,
  kVideoVp8 = 1,
  kVideoVp9 = 2,
  kVideoH264 = 3,
  kVideoMax = 64,
};

const char* kRealtimePrefix = "WebRTC.Video.";
const char* kScreenPrefix = "WebRTC.Video.Screenshare.";

const char* GetUmaPrefix(VideoEncoderConfig::ContentType content_type) {
  switch (content_type) {
    case VideoEncoderConfig::ContentType::kRealtimeVideo:
      return kRealtimePrefix;
    case VideoEncoderConfig::ContentType::kScreen:
      return kScreenPrefix;
  }
  RTC_NOTREACHED();
  return nullptr;
}

HistogramCodecType PayloadNameToHistogramCodecType(
    const std::string& payload_name) {
  VideoCodecType codecType = PayloadStringToCodecType(payload_name);
  switch (codecType) {
    case kVideoCodecVP8:
      return kVideoVp8;
    case kVideoCodecVP9:
      return kVideoVp9;
    case kVideoCodecH264:
      return kVideoH264;
    default:
      return kVideoUnknown;
  }
}

void UpdateCodecTypeHistogram(const std::string& payload_name) {
  RTC_HISTOGRAM_ENUMERATION("WebRTC.Video.Encoder.CodecType",
                            PayloadNameToHistogramCodecType(payload_name),
                            kVideoMax);
}

bool IsForcedFallbackPossible(const CodecSpecificInfo* codec_info) {
  return codec_info->codecType == kVideoCodecVP8 &&
         codec_info->codecSpecific.VP8.simulcastIdx == 0 &&
         (codec_info->codecSpecific.VP8.temporalIdx == 0 ||
          codec_info->codecSpecific.VP8.temporalIdx == kNoTemporalIdx);
}

rtc::Optional<int> GetFallbackMaxPixels(const std::string& group) {
  if (group.empty())
    return rtc::Optional<int>();

  int min_pixels;
  int max_pixels;
  int min_bps;
  if (sscanf(group.c_str(), "-%d,%d,%d", &min_pixels, &max_pixels, &min_bps) !=
      3) {
    return rtc::Optional<int>();
  }

  if (min_pixels <= 0 || max_pixels <= 0 || max_pixels < min_pixels)
    return rtc::Optional<int>();

  return rtc::Optional<int>(max_pixels);
}

rtc::Optional<int> GetFallbackMaxPixelsIfFieldTrialEnabled() {
  std::string group =
      webrtc::field_trial::FindFullName(kVp8ForcedFallbackEncoderFieldTrial);
  return (group.find("Enabled") == 0) ? GetFallbackMaxPixels(group.substr(7))
                                      : rtc::Optional<int>();
}

rtc::Optional<int> GetFallbackMaxPixelsIfFieldTrialDisabled() {
  std::string group =
      webrtc::field_trial::FindFullName(kVp8ForcedFallbackEncoderFieldTrial);
  return (group.find("Disabled") == 0) ? GetFallbackMaxPixels(group.substr(8))
                                       : rtc::Optional<int>();
}
}  // namespace


const int SendStatisticsProxy::kStatsTimeoutMs = 5000;

SendStatisticsProxy::SendStatisticsProxy(
    Clock* clock,
    const VideoSendStream::Config& config,
    VideoEncoderConfig::ContentType content_type)
    : clock_(clock),
      payload_name_(config.encoder_settings.payload_name),
      rtp_config_(config.rtp),
      fallback_max_pixels_(GetFallbackMaxPixelsIfFieldTrialEnabled()),
      fallback_max_pixels_disabled_(GetFallbackMaxPixelsIfFieldTrialDisabled()),
      content_type_(content_type),
      start_ms_(clock->TimeInMilliseconds()),
      encode_time_(kEncodeTimeWeigthFactor),
      quality_downscales_(-1),
      cpu_downscales_(-1),
      media_byte_rate_tracker_(kBucketSizeMs, kBucketCount),
      encoded_frame_rate_tracker_(kBucketSizeMs, kBucketCount),
      uma_container_(
          new UmaSamplesContainer(GetUmaPrefix(content_type_), stats_, clock)) {
}

SendStatisticsProxy::~SendStatisticsProxy() {
  rtc::CritScope lock(&crit_);
  uma_container_->UpdateHistograms(rtp_config_, stats_);

  int64_t elapsed_sec = (clock_->TimeInMilliseconds() - start_ms_) / 1000;
  RTC_HISTOGRAM_COUNTS_100000("WebRTC.Video.SendStreamLifetimeInSeconds",
                              elapsed_sec);

  if (elapsed_sec >= metrics::kMinRunTimeInSeconds)
    UpdateCodecTypeHistogram(payload_name_);
}

SendStatisticsProxy::UmaSamplesContainer::UmaSamplesContainer(
    const char* prefix,
    const VideoSendStream::Stats& stats,
    Clock* const clock)
    : uma_prefix_(prefix),
      clock_(clock),
      input_frame_rate_tracker_(100, 10u),
      input_fps_counter_(clock, nullptr, true),
      sent_fps_counter_(clock, nullptr, true),
      total_byte_counter_(clock, nullptr, true),
      media_byte_counter_(clock, nullptr, true),
      rtx_byte_counter_(clock, nullptr, true),
      padding_byte_counter_(clock, nullptr, true),
      retransmit_byte_counter_(clock, nullptr, true),
      fec_byte_counter_(clock, nullptr, true),
      first_rtcp_stats_time_ms_(-1),
      first_rtp_stats_time_ms_(-1),
      start_stats_(stats) {
  InitializeBitrateCounters(stats);
}

SendStatisticsProxy::UmaSamplesContainer::~UmaSamplesContainer() {}

void SendStatisticsProxy::UmaSamplesContainer::InitializeBitrateCounters(
    const VideoSendStream::Stats& stats) {
  for (const auto& it : stats.substreams) {
    uint32_t ssrc = it.first;
    total_byte_counter_.SetLast(it.second.rtp_stats.transmitted.TotalBytes(),
                                ssrc);
    padding_byte_counter_.SetLast(it.second.rtp_stats.transmitted.padding_bytes,
                                  ssrc);
    retransmit_byte_counter_.SetLast(
        it.second.rtp_stats.retransmitted.TotalBytes(), ssrc);
    fec_byte_counter_.SetLast(it.second.rtp_stats.fec.TotalBytes(), ssrc);
    if (it.second.is_rtx) {
      rtx_byte_counter_.SetLast(it.second.rtp_stats.transmitted.TotalBytes(),
                                ssrc);
    } else {
      media_byte_counter_.SetLast(it.second.rtp_stats.MediaPayloadBytes(),
                                  ssrc);
    }
  }
}

void SendStatisticsProxy::UmaSamplesContainer::RemoveOld(int64_t now_ms) {
  while (!encoded_frames_.empty()) {
    auto it = encoded_frames_.begin();
    if (now_ms - it->second.send_ms < kMaxEncodedFrameWindowMs)
      break;

    // Use max per timestamp.
    sent_width_counter_.Add(it->second.max_width);
    sent_height_counter_.Add(it->second.max_height);
    encoded_frames_.erase(it);
  }
}

bool SendStatisticsProxy::UmaSamplesContainer::InsertEncodedFrame(
    const EncodedImage& encoded_frame) {
  int64_t now_ms = clock_->TimeInMilliseconds();
  RemoveOld(now_ms);
  if (encoded_frames_.size() > kMaxEncodedFrameMapSize) {
    encoded_frames_.clear();
  }

  auto it = encoded_frames_.find(encoded_frame._timeStamp);
  if (it == encoded_frames_.end()) {
    // First frame with this timestamp.
    encoded_frames_.insert(std::make_pair(
        encoded_frame._timeStamp, Frame(now_ms, encoded_frame._encodedWidth,
                                        encoded_frame._encodedHeight)));
    sent_fps_counter_.Add(1);
    return true;
  }

  it->second.max_width =
      std::max(it->second.max_width, encoded_frame._encodedWidth);
  it->second.max_height =
      std::max(it->second.max_height, encoded_frame._encodedHeight);
  return false;
}

void SendStatisticsProxy::UmaSamplesContainer::UpdateHistograms(
    const VideoSendStream::Config::Rtp& rtp_config,
    const VideoSendStream::Stats& current_stats) {
  RTC_DCHECK(uma_prefix_ == kRealtimePrefix || uma_prefix_ == kScreenPrefix);
  const int kIndex = uma_prefix_ == kScreenPrefix ? 1 : 0;
  const int kMinRequiredPeriodicSamples = 6;
  int in_width = input_width_counter_.Avg(kMinRequiredMetricsSamples);
  int in_height = input_height_counter_.Avg(kMinRequiredMetricsSamples);
  if (in_width != -1) {
    RTC_HISTOGRAMS_COUNTS_10000(kIndex, uma_prefix_ + "InputWidthInPixels",
                                in_width);
    RTC_HISTOGRAMS_COUNTS_10000(kIndex, uma_prefix_ + "InputHeightInPixels",
                                in_height);
    RTC_LOG(LS_INFO) << uma_prefix_ << "InputWidthInPixels " << in_width;
    RTC_LOG(LS_INFO) << uma_prefix_ << "InputHeightInPixels " << in_height;
  }
  AggregatedStats in_fps = input_fps_counter_.GetStats();
  if (in_fps.num_samples >= kMinRequiredPeriodicSamples) {
    RTC_HISTOGRAMS_COUNTS_100(kIndex, uma_prefix_ + "InputFramesPerSecond",
                              in_fps.average);
    RTC_LOG(LS_INFO) << uma_prefix_ + "InputFramesPerSecond, "
                     << in_fps.ToString();
  }

  int sent_width = sent_width_counter_.Avg(kMinRequiredMetricsSamples);
  int sent_height = sent_height_counter_.Avg(kMinRequiredMetricsSamples);
  if (sent_width != -1) {
    RTC_HISTOGRAMS_COUNTS_10000(kIndex, uma_prefix_ + "SentWidthInPixels",
                                sent_width);
    RTC_HISTOGRAMS_COUNTS_10000(kIndex, uma_prefix_ + "SentHeightInPixels",
                                sent_height);
    RTC_LOG(LS_INFO) << uma_prefix_ << "SentWidthInPixels " << sent_width;
    RTC_LOG(LS_INFO) << uma_prefix_ << "SentHeightInPixels " << sent_height;
  }
  AggregatedStats sent_fps = sent_fps_counter_.GetStats();
  if (sent_fps.num_samples >= kMinRequiredPeriodicSamples) {
    RTC_HISTOGRAMS_COUNTS_100(kIndex, uma_prefix_ + "SentFramesPerSecond",
                              sent_fps.average);
    RTC_LOG(LS_INFO) << uma_prefix_ + "SentFramesPerSecond, "
                     << sent_fps.ToString();
  }

  if (in_fps.num_samples > kMinRequiredPeriodicSamples &&
      sent_fps.num_samples >= kMinRequiredPeriodicSamples) {
    int in_fps_avg = in_fps.average;
    if (in_fps_avg > 0) {
      int sent_fps_avg = sent_fps.average;
      int sent_to_in_fps_ratio_percent =
          (100 * sent_fps_avg + in_fps_avg / 2) / in_fps_avg;
      // If reported period is small, it may happen that sent_fps is larger than
      // input_fps briefly on average. This should be treated as 100% sent to
      // input ratio.
      if (sent_to_in_fps_ratio_percent > 100)
        sent_to_in_fps_ratio_percent = 100;
      RTC_HISTOGRAMS_PERCENTAGE(kIndex,
                                uma_prefix_ + "SentToInputFpsRatioPercent",
                                sent_to_in_fps_ratio_percent);
      RTC_LOG(LS_INFO) << uma_prefix_ << "SentToInputFpsRatioPercent "
                       << sent_to_in_fps_ratio_percent;
    }
  }

  int encode_ms = encode_time_counter_.Avg(kMinRequiredMetricsSamples);
  if (encode_ms != -1) {
    RTC_HISTOGRAMS_COUNTS_1000(kIndex, uma_prefix_ + "EncodeTimeInMs",
                               encode_ms);
    RTC_LOG(LS_INFO) << uma_prefix_ << "EncodeTimeInMs " << encode_ms;
  }
  int key_frames_permille =
      key_frame_counter_.Permille(kMinRequiredMetricsSamples);
  if (key_frames_permille != -1) {
    RTC_HISTOGRAMS_COUNTS_1000(kIndex, uma_prefix_ + "KeyFramesSentInPermille",
                               key_frames_permille);
    RTC_LOG(LS_INFO) << uma_prefix_ << "KeyFramesSentInPermille "
                     << key_frames_permille;
  }
  int quality_limited =
      quality_limited_frame_counter_.Percent(kMinRequiredMetricsSamples);
  if (quality_limited != -1) {
    RTC_HISTOGRAMS_PERCENTAGE(kIndex,
                              uma_prefix_ + "QualityLimitedResolutionInPercent",
                              quality_limited);
    RTC_LOG(LS_INFO) << uma_prefix_ << "QualityLimitedResolutionInPercent "
                     << quality_limited;
  }
  int downscales = quality_downscales_counter_.Avg(kMinRequiredMetricsSamples);
  if (downscales != -1) {
    RTC_HISTOGRAMS_ENUMERATION(
        kIndex, uma_prefix_ + "QualityLimitedResolutionDownscales", downscales,
        20);
  }
  int cpu_limited =
      cpu_limited_frame_counter_.Percent(kMinRequiredMetricsSamples);
  if (cpu_limited != -1) {
    RTC_HISTOGRAMS_PERCENTAGE(
        kIndex, uma_prefix_ + "CpuLimitedResolutionInPercent", cpu_limited);
  }
  int bw_limited =
      bw_limited_frame_counter_.Percent(kMinRequiredMetricsSamples);
  if (bw_limited != -1) {
    RTC_HISTOGRAMS_PERCENTAGE(
        kIndex, uma_prefix_ + "BandwidthLimitedResolutionInPercent",
        bw_limited);
  }
  int num_disabled =
      bw_resolutions_disabled_counter_.Avg(kMinRequiredMetricsSamples);
  if (num_disabled != -1) {
    RTC_HISTOGRAMS_ENUMERATION(
        kIndex, uma_prefix_ + "BandwidthLimitedResolutionsDisabled",
        num_disabled, 10);
  }
  int delay_ms = delay_counter_.Avg(kMinRequiredMetricsSamples);
  if (delay_ms != -1)
    RTC_HISTOGRAMS_COUNTS_100000(kIndex, uma_prefix_ + "SendSideDelayInMs",
                                 delay_ms);

  int max_delay_ms = max_delay_counter_.Avg(kMinRequiredMetricsSamples);
  if (max_delay_ms != -1) {
    RTC_HISTOGRAMS_COUNTS_100000(kIndex, uma_prefix_ + "SendSideDelayMaxInMs",
                                 max_delay_ms);
  }

  for (const auto& it : qp_counters_) {
    int qp_vp8 = it.second.vp8.Avg(kMinRequiredMetricsSamples);
    if (qp_vp8 != -1) {
      int spatial_idx = it.first;
      if (spatial_idx == -1) {
        RTC_HISTOGRAMS_COUNTS_200(kIndex, uma_prefix_ + "Encoded.Qp.Vp8",
                                  qp_vp8);
      } else if (spatial_idx == 0) {
        RTC_HISTOGRAMS_COUNTS_200(kIndex, uma_prefix_ + "Encoded.Qp.Vp8.S0",
                                  qp_vp8);
      } else if (spatial_idx == 1) {
        RTC_HISTOGRAMS_COUNTS_200(kIndex, uma_prefix_ + "Encoded.Qp.Vp8.S1",
                                  qp_vp8);
      } else if (spatial_idx == 2) {
        RTC_HISTOGRAMS_COUNTS_200(kIndex, uma_prefix_ + "Encoded.Qp.Vp8.S2",
                                  qp_vp8);
      } else {
        RTC_LOG(LS_WARNING)
            << "QP stats not recorded for VP8 spatial idx " << spatial_idx;
      }
    }
    int qp_vp9 = it.second.vp9.Avg(kMinRequiredMetricsSamples);
    if (qp_vp9 != -1) {
      int spatial_idx = it.first;
      if (spatial_idx == -1) {
        RTC_HISTOGRAMS_COUNTS_500(kIndex, uma_prefix_ + "Encoded.Qp.Vp9",
                                  qp_vp9);
      } else if (spatial_idx == 0) {
        RTC_HISTOGRAMS_COUNTS_500(kIndex, uma_prefix_ + "Encoded.Qp.Vp9.S0",
                                  qp_vp9);
      } else if (spatial_idx == 1) {
        RTC_HISTOGRAMS_COUNTS_500(kIndex, uma_prefix_ + "Encoded.Qp.Vp9.S1",
                                  qp_vp9);
      } else if (spatial_idx == 2) {
        RTC_HISTOGRAMS_COUNTS_500(kIndex, uma_prefix_ + "Encoded.Qp.Vp9.S2",
                                  qp_vp9);
      } else {
        RTC_LOG(LS_WARNING)
            << "QP stats not recorded for VP9 spatial layer " << spatial_idx;
      }
    }
    int qp_h264 = it.second.h264.Avg(kMinRequiredMetricsSamples);
    if (qp_h264 != -1) {
      int spatial_idx = it.first;
      RTC_DCHECK_EQ(-1, spatial_idx);
      RTC_HISTOGRAMS_COUNTS_100(kIndex, uma_prefix_ + "Encoded.Qp.H264",
                                qp_h264);
    }
  }

  if (first_rtp_stats_time_ms_ != -1) {
    quality_adapt_timer_.Stop(clock_->TimeInMilliseconds());
    int64_t elapsed_sec = quality_adapt_timer_.total_ms / 1000;
    if (elapsed_sec >= metrics::kMinRunTimeInSeconds) {
      int quality_changes = current_stats.number_of_quality_adapt_changes -
                            start_stats_.number_of_quality_adapt_changes;
      RTC_HISTOGRAMS_COUNTS_100(kIndex,
                                uma_prefix_ + "AdaptChangesPerMinute.Quality",
                                quality_changes * 60 / elapsed_sec);
    }
    cpu_adapt_timer_.Stop(clock_->TimeInMilliseconds());
    elapsed_sec = cpu_adapt_timer_.total_ms / 1000;
    if (elapsed_sec >= metrics::kMinRunTimeInSeconds) {
      int cpu_changes = current_stats.number_of_cpu_adapt_changes -
                        start_stats_.number_of_cpu_adapt_changes;
      RTC_HISTOGRAMS_COUNTS_100(kIndex,
                                uma_prefix_ + "AdaptChangesPerMinute.Cpu",
                                cpu_changes * 60 / elapsed_sec);
    }
  }

  if (first_rtcp_stats_time_ms_ != -1) {
    int64_t elapsed_sec =
        (clock_->TimeInMilliseconds() - first_rtcp_stats_time_ms_) / 1000;
    if (elapsed_sec >= metrics::kMinRunTimeInSeconds) {
      int fraction_lost = report_block_stats_.FractionLostInPercent();
      if (fraction_lost != -1) {
        RTC_HISTOGRAMS_PERCENTAGE(
            kIndex, uma_prefix_ + "SentPacketsLostInPercent", fraction_lost);
        RTC_LOG(LS_INFO) << uma_prefix_ << "SentPacketsLostInPercent "
                         << fraction_lost;
      }

      // The RTCP packet type counters, delivered via the
      // RtcpPacketTypeCounterObserver interface, are aggregates over the entire
      // life of the send stream and are not reset when switching content type.
      // For the purpose of these statistics though, we want new counts when
      // switching since we switch histogram name. On every reset of the
      // UmaSamplesContainer, we save the initial state of the counters, so that
      // we can calculate the delta here and aggregate over all ssrcs.
      RtcpPacketTypeCounter counters;
      for (uint32_t ssrc : rtp_config.ssrcs) {
        auto kv = current_stats.substreams.find(ssrc);
        if (kv == current_stats.substreams.end())
          continue;

        RtcpPacketTypeCounter stream_counters =
            kv->second.rtcp_packet_type_counts;
        kv = start_stats_.substreams.find(ssrc);
        if (kv != start_stats_.substreams.end())
          stream_counters.Subtract(kv->second.rtcp_packet_type_counts);

        counters.Add(stream_counters);
      }
      RTC_HISTOGRAMS_COUNTS_10000(kIndex,
                                  uma_prefix_ + "NackPacketsReceivedPerMinute",
                                  counters.nack_packets * 60 / elapsed_sec);
      RTC_HISTOGRAMS_COUNTS_10000(kIndex,
                                  uma_prefix_ + "FirPacketsReceivedPerMinute",
                                  counters.fir_packets * 60 / elapsed_sec);
      RTC_HISTOGRAMS_COUNTS_10000(kIndex,
                                  uma_prefix_ + "PliPacketsReceivedPerMinute",
                                  counters.pli_packets * 60 / elapsed_sec);
      if (counters.nack_requests > 0) {
        RTC_HISTOGRAMS_PERCENTAGE(
            kIndex, uma_prefix_ + "UniqueNackRequestsReceivedInPercent",
            counters.UniqueNackRequestsInPercent());
      }
    }
  }

  if (first_rtp_stats_time_ms_ != -1) {
    int64_t elapsed_sec =
        (clock_->TimeInMilliseconds() - first_rtp_stats_time_ms_) / 1000;
    if (elapsed_sec >= metrics::kMinRunTimeInSeconds) {
      RTC_HISTOGRAMS_COUNTS_100(kIndex, uma_prefix_ + "NumberOfPauseEvents",
                                target_rate_updates_.pause_resume_events);
      RTC_LOG(LS_INFO) << uma_prefix_ << "NumberOfPauseEvents "
                       << target_rate_updates_.pause_resume_events;

      int paused_time_percent =
          paused_time_counter_.Percent(metrics::kMinRunTimeInSeconds * 1000);
      if (paused_time_percent != -1) {
        RTC_HISTOGRAMS_PERCENTAGE(kIndex, uma_prefix_ + "PausedTimeInPercent",
                                  paused_time_percent);
        RTC_LOG(LS_INFO) << uma_prefix_ << "PausedTimeInPercent "
                         << paused_time_percent;
      }
    }
  }

  if (fallback_info_.is_possible) {
    // Double interval since there is some time before fallback may occur.
    const int kMinRunTimeMs = 2 * metrics::kMinRunTimeInSeconds * 1000;
    int64_t elapsed_ms = fallback_info_.elapsed_ms;
    int fallback_time_percent = fallback_active_counter_.Percent(kMinRunTimeMs);
    if (fallback_time_percent != -1 && elapsed_ms >= kMinRunTimeMs) {
      RTC_HISTOGRAMS_PERCENTAGE(
          kIndex, uma_prefix_ + "Encoder.ForcedSwFallbackTimeInPercent.Vp8",
          fallback_time_percent);
      RTC_HISTOGRAMS_COUNTS_100(
          kIndex, uma_prefix_ + "Encoder.ForcedSwFallbackChangesPerMinute.Vp8",
          fallback_info_.on_off_events * 60 / (elapsed_ms / 1000));
    }
  }

  AggregatedStats total_bytes_per_sec = total_byte_counter_.GetStats();
  if (total_bytes_per_sec.num_samples > kMinRequiredPeriodicSamples) {
    RTC_HISTOGRAMS_COUNTS_10000(kIndex, uma_prefix_ + "BitrateSentInKbps",
                                total_bytes_per_sec.average * 8 / 1000);
    RTC_LOG(LS_INFO) << uma_prefix_ << "BitrateSentInBps, "
                     << total_bytes_per_sec.ToStringWithMultiplier(8);
  }
  AggregatedStats media_bytes_per_sec = media_byte_counter_.GetStats();
  if (media_bytes_per_sec.num_samples > kMinRequiredPeriodicSamples) {
    RTC_HISTOGRAMS_COUNTS_10000(kIndex, uma_prefix_ + "MediaBitrateSentInKbps",
                                media_bytes_per_sec.average * 8 / 1000);
    RTC_LOG(LS_INFO) << uma_prefix_ << "MediaBitrateSentInBps, "
                     << media_bytes_per_sec.ToStringWithMultiplier(8);
  }
  AggregatedStats padding_bytes_per_sec = padding_byte_counter_.GetStats();
  if (padding_bytes_per_sec.num_samples > kMinRequiredPeriodicSamples) {
    RTC_HISTOGRAMS_COUNTS_10000(kIndex,
                                uma_prefix_ + "PaddingBitrateSentInKbps",
                                padding_bytes_per_sec.average * 8 / 1000);
    RTC_LOG(LS_INFO) << uma_prefix_ << "PaddingBitrateSentInBps, "
                     << padding_bytes_per_sec.ToStringWithMultiplier(8);
  }
  AggregatedStats retransmit_bytes_per_sec =
      retransmit_byte_counter_.GetStats();
  if (retransmit_bytes_per_sec.num_samples > kMinRequiredPeriodicSamples) {
    RTC_HISTOGRAMS_COUNTS_10000(kIndex,
                                uma_prefix_ + "RetransmittedBitrateSentInKbps",
                                retransmit_bytes_per_sec.average * 8 / 1000);
    RTC_LOG(LS_INFO) << uma_prefix_ << "RetransmittedBitrateSentInBps, "
                     << retransmit_bytes_per_sec.ToStringWithMultiplier(8);
  }
  if (!rtp_config.rtx.ssrcs.empty()) {
    AggregatedStats rtx_bytes_per_sec = rtx_byte_counter_.GetStats();
    int rtx_bytes_per_sec_avg = -1;
    if (rtx_bytes_per_sec.num_samples > kMinRequiredPeriodicSamples) {
      rtx_bytes_per_sec_avg = rtx_bytes_per_sec.average;
      RTC_LOG(LS_INFO) << uma_prefix_ << "RtxBitrateSentInBps, "
                       << rtx_bytes_per_sec.ToStringWithMultiplier(8);
    } else if (total_bytes_per_sec.num_samples > kMinRequiredPeriodicSamples) {
      rtx_bytes_per_sec_avg = 0;  // RTX enabled but no RTX data sent, record 0.
    }
    if (rtx_bytes_per_sec_avg != -1) {
      RTC_HISTOGRAMS_COUNTS_10000(kIndex, uma_prefix_ + "RtxBitrateSentInKbps",
                                  rtx_bytes_per_sec_avg * 8 / 1000);
    }
  }
  if (rtp_config.flexfec.payload_type != -1 ||
      rtp_config.ulpfec.red_payload_type != -1) {
    AggregatedStats fec_bytes_per_sec = fec_byte_counter_.GetStats();
    if (fec_bytes_per_sec.num_samples > kMinRequiredPeriodicSamples) {
      RTC_HISTOGRAMS_COUNTS_10000(kIndex, uma_prefix_ + "FecBitrateSentInKbps",
                                  fec_bytes_per_sec.average * 8 / 1000);
      RTC_LOG(LS_INFO) << uma_prefix_ << "FecBitrateSentInBps, "
                       << fec_bytes_per_sec.ToStringWithMultiplier(8);
    }
  }
  RTC_LOG(LS_INFO) << "Frames encoded " << current_stats.frames_encoded;
  RTC_LOG(LS_INFO) << uma_prefix_ << "DroppedFrames.Capturer "
                   << current_stats.frames_dropped_by_capturer;
  RTC_HISTOGRAMS_COUNTS_1000(kIndex, uma_prefix_ + "DroppedFrames.Capturer",
                             current_stats.frames_dropped_by_capturer);
  RTC_LOG(LS_INFO) << uma_prefix_ << "DroppedFrames.EncoderQueue "
                   << current_stats.frames_dropped_by_encoder_queue;
  RTC_HISTOGRAMS_COUNTS_1000(kIndex, uma_prefix_ + "DroppedFrames.EncoderQueue",
                             current_stats.frames_dropped_by_encoder_queue);
  RTC_LOG(LS_INFO) << uma_prefix_ << "DroppedFrames.Encoder "
                   << current_stats.frames_dropped_by_encoder;
  RTC_HISTOGRAMS_COUNTS_1000(kIndex, uma_prefix_ + "DroppedFrames.Encoder",
                             current_stats.frames_dropped_by_encoder);
  RTC_LOG(LS_INFO) << uma_prefix_ << "DroppedFrames.Ratelimiter "
                   << current_stats.frames_dropped_by_rate_limiter;
  RTC_HISTOGRAMS_COUNTS_1000(kIndex, uma_prefix_ + "DroppedFrames.Ratelimiter",
                             current_stats.frames_dropped_by_rate_limiter);
}

void SendStatisticsProxy::OnEncoderReconfigured(
    const VideoEncoderConfig& config,
    uint32_t preferred_bitrate_bps) {
  rtc::CritScope lock(&crit_);
  stats_.preferred_media_bitrate_bps = preferred_bitrate_bps;

  if (content_type_ != config.content_type) {
    uma_container_->UpdateHistograms(rtp_config_, stats_);
    uma_container_.reset(new UmaSamplesContainer(
        GetUmaPrefix(config.content_type), stats_, clock_));
    content_type_ = config.content_type;
  }
}

void SendStatisticsProxy::OnEncodedFrameTimeMeasured(
    int encode_time_ms,
    const CpuOveruseMetrics& metrics) {
  rtc::CritScope lock(&crit_);
  uma_container_->encode_time_counter_.Add(encode_time_ms);
  encode_time_.Apply(1.0f, encode_time_ms);
  stats_.avg_encode_time_ms = round(encode_time_.filtered());
  stats_.encode_usage_percent = metrics.encode_usage_percent;
}

void SendStatisticsProxy::OnSuspendChange(bool is_suspended) {
  int64_t now_ms = clock_->TimeInMilliseconds();
  rtc::CritScope lock(&crit_);
  stats_.suspended = is_suspended;
  if (is_suspended) {
    // Pause framerate (add min pause time since there may be frames/packets
    // that are not yet sent).
    const int64_t kMinMs = 500;
    uma_container_->input_fps_counter_.ProcessAndPauseForDuration(kMinMs);
    uma_container_->sent_fps_counter_.ProcessAndPauseForDuration(kMinMs);
    // Pause bitrate stats.
    uma_container_->total_byte_counter_.ProcessAndPauseForDuration(kMinMs);
    uma_container_->media_byte_counter_.ProcessAndPauseForDuration(kMinMs);
    uma_container_->rtx_byte_counter_.ProcessAndPauseForDuration(kMinMs);
    uma_container_->padding_byte_counter_.ProcessAndPauseForDuration(kMinMs);
    uma_container_->retransmit_byte_counter_.ProcessAndPauseForDuration(kMinMs);
    uma_container_->fec_byte_counter_.ProcessAndPauseForDuration(kMinMs);
    // Stop adaptation stats.
    uma_container_->cpu_adapt_timer_.Stop(now_ms);
    uma_container_->quality_adapt_timer_.Stop(now_ms);
  } else {
    // Start adaptation stats if scaling is enabled.
    if (cpu_downscales_ >= 0)
      uma_container_->cpu_adapt_timer_.Start(now_ms);
    if (quality_downscales_ >= 0)
      uma_container_->quality_adapt_timer_.Start(now_ms);
    // Stop pause explicitly for stats that may be zero/not updated for some
    // time.
    uma_container_->rtx_byte_counter_.ProcessAndStopPause();
    uma_container_->padding_byte_counter_.ProcessAndStopPause();
    uma_container_->retransmit_byte_counter_.ProcessAndStopPause();
    uma_container_->fec_byte_counter_.ProcessAndStopPause();
  }
}

VideoSendStream::Stats SendStatisticsProxy::GetStats() {
  rtc::CritScope lock(&crit_);
  PurgeOldStats();
  stats_.input_frame_rate =
      round(uma_container_->input_frame_rate_tracker_.ComputeRate());
  stats_.content_type =
      content_type_ == VideoEncoderConfig::ContentType::kRealtimeVideo
          ? VideoContentType::UNSPECIFIED
          : VideoContentType::SCREENSHARE;
  stats_.encode_frame_rate = round(encoded_frame_rate_tracker_.ComputeRate());
  stats_.media_bitrate_bps = media_byte_rate_tracker_.ComputeRate() * 8;
  return stats_;
}

void SendStatisticsProxy::PurgeOldStats() {
  int64_t old_stats_ms = clock_->TimeInMilliseconds() - kStatsTimeoutMs;
  for (std::map<uint32_t, VideoSendStream::StreamStats>::iterator it =
           stats_.substreams.begin();
       it != stats_.substreams.end(); ++it) {
    uint32_t ssrc = it->first;
    if (update_times_[ssrc].resolution_update_ms <= old_stats_ms) {
      it->second.width = 0;
      it->second.height = 0;
    }
  }
}

VideoSendStream::StreamStats* SendStatisticsProxy::GetStatsEntry(
    uint32_t ssrc) {
  std::map<uint32_t, VideoSendStream::StreamStats>::iterator it =
      stats_.substreams.find(ssrc);
  if (it != stats_.substreams.end())
    return &it->second;

  bool is_media = std::find(rtp_config_.ssrcs.begin(), rtp_config_.ssrcs.end(),
                            ssrc) != rtp_config_.ssrcs.end();
  bool is_flexfec = rtp_config_.flexfec.payload_type != -1 &&
                    ssrc == rtp_config_.flexfec.ssrc;
  bool is_rtx =
      std::find(rtp_config_.rtx.ssrcs.begin(), rtp_config_.rtx.ssrcs.end(),
                ssrc) != rtp_config_.rtx.ssrcs.end();
  if (!is_media && !is_flexfec && !is_rtx)
    return nullptr;

  // Insert new entry and return ptr.
  VideoSendStream::StreamStats* entry = &stats_.substreams[ssrc];
  entry->is_rtx = is_rtx;
  entry->is_flexfec = is_flexfec;

  return entry;
}

void SendStatisticsProxy::OnInactiveSsrc(uint32_t ssrc) {
  rtc::CritScope lock(&crit_);
  VideoSendStream::StreamStats* stats = GetStatsEntry(ssrc);
  if (!stats)
    return;

  stats->total_bitrate_bps = 0;
  stats->retransmit_bitrate_bps = 0;
  stats->height = 0;
  stats->width = 0;
}

void SendStatisticsProxy::OnSetEncoderTargetRate(uint32_t bitrate_bps) {
  rtc::CritScope lock(&crit_);
  if (uma_container_->target_rate_updates_.last_ms == -1 && bitrate_bps == 0)
    return;  // Start on first non-zero bitrate, may initially be zero.

  int64_t now = clock_->TimeInMilliseconds();
  if (uma_container_->target_rate_updates_.last_ms != -1) {
    bool was_paused = stats_.target_media_bitrate_bps == 0;
    int64_t diff_ms = now - uma_container_->target_rate_updates_.last_ms;
    uma_container_->paused_time_counter_.Add(was_paused, diff_ms);

    // Use last to not include update when stream is stopped and video disabled.
    if (uma_container_->target_rate_updates_.last_paused_or_resumed)
      ++uma_container_->target_rate_updates_.pause_resume_events;

    // Check if video is paused/resumed.
    uma_container_->target_rate_updates_.last_paused_or_resumed =
        (bitrate_bps == 0) != was_paused;
  }
  uma_container_->target_rate_updates_.last_ms = now;

  stats_.target_media_bitrate_bps = bitrate_bps;
}

void SendStatisticsProxy::UpdateEncoderFallbackStats(
    const CodecSpecificInfo* codec_info,
    int pixels) {
  UpdateFallbackDisabledStats(codec_info, pixels);

  if (!fallback_max_pixels_ || !uma_container_->fallback_info_.is_possible) {
    return;
  }

  if (!IsForcedFallbackPossible(codec_info)) {
    uma_container_->fallback_info_.is_possible = false;
    return;
  }

  FallbackEncoderInfo* fallback_info = &uma_container_->fallback_info_;

  const int64_t now_ms = clock_->TimeInMilliseconds();
  bool is_active = fallback_info->is_active;
  if (codec_info->codec_name != stats_.encoder_implementation_name) {
    // Implementation changed.
    is_active = strcmp(codec_info->codec_name, kVp8SwCodecName) == 0;
    if (!is_active && stats_.encoder_implementation_name != kVp8SwCodecName) {
      // First or not a VP8 SW change, update stats on next call.
      return;
    }
    if (is_active && (pixels > *fallback_max_pixels_)) {
      // Pixels should not be above |fallback_max_pixels_|. If above skip to
      // avoid fallbacks due to failure.
      fallback_info->is_possible = false;
      return;
    }
    stats_.has_entered_low_resolution = true;
    ++fallback_info->on_off_events;
  }

  if (fallback_info->last_update_ms) {
    int64_t diff_ms = now_ms - *(fallback_info->last_update_ms);
    // If the time diff since last update is greater than |max_frame_diff_ms|,
    // video is considered paused/muted and the change is not included.
    if (diff_ms < fallback_info->max_frame_diff_ms) {
      uma_container_->fallback_active_counter_.Add(fallback_info->is_active,
                                                   diff_ms);
      fallback_info->elapsed_ms += diff_ms;
    }
  }
  fallback_info->is_active = is_active;
  fallback_info->last_update_ms.emplace(now_ms);
}

void SendStatisticsProxy::UpdateFallbackDisabledStats(
    const CodecSpecificInfo* codec_info,
    int pixels) {
  if (!fallback_max_pixels_disabled_ ||
      !uma_container_->fallback_info_disabled_.is_possible ||
      stats_.has_entered_low_resolution) {
    return;
  }

  if (!IsForcedFallbackPossible(codec_info) ||
      strcmp(codec_info->codec_name, kVp8SwCodecName) == 0) {
    uma_container_->fallback_info_disabled_.is_possible = false;
    return;
  }

  if (pixels <= *fallback_max_pixels_disabled_ ||
      uma_container_->fallback_info_disabled_.min_pixel_limit_reached) {
    stats_.has_entered_low_resolution = true;
  }
}

void SendStatisticsProxy::OnMinPixelLimitReached() {
  rtc::CritScope lock(&crit_);
  uma_container_->fallback_info_disabled_.min_pixel_limit_reached = true;
}

void SendStatisticsProxy::OnSendEncodedImage(
    const EncodedImage& encoded_image,
    const CodecSpecificInfo* codec_info) {
  size_t simulcast_idx = 0;

  rtc::CritScope lock(&crit_);
  ++stats_.frames_encoded;
  if (codec_info) {
    if (codec_info->codecType == kVideoCodecVP8) {
      simulcast_idx = codec_info->codecSpecific.VP8.simulcastIdx;
    } else if (codec_info->codecType == kVideoCodecGeneric) {
      simulcast_idx = codec_info->codecSpecific.generic.simulcast_idx;
    }
    if (codec_info->codec_name) {
      UpdateEncoderFallbackStats(codec_info, encoded_image._encodedWidth *
                                                 encoded_image._encodedHeight);
      stats_.encoder_implementation_name = codec_info->codec_name;
    }
  }

  if (simulcast_idx >= rtp_config_.ssrcs.size()) {
    RTC_LOG(LS_ERROR) << "Encoded image outside simulcast range ("
                      << simulcast_idx << " >= " << rtp_config_.ssrcs.size()
                      << ").";
    return;
  }
  uint32_t ssrc = rtp_config_.ssrcs[simulcast_idx];

  VideoSendStream::StreamStats* stats = GetStatsEntry(ssrc);
  if (!stats)
    return;

  stats->width = encoded_image._encodedWidth;
  stats->height = encoded_image._encodedHeight;
  update_times_[ssrc].resolution_update_ms = clock_->TimeInMilliseconds();

  uma_container_->key_frame_counter_.Add(encoded_image._frameType ==
                                         kVideoFrameKey);
  stats_.bw_limited_resolution =
      encoded_image.adapt_reason_.bw_resolutions_disabled > 0 ||
      quality_downscales_ > 0;

  if (quality_downscales_ != -1) {
    uma_container_->quality_limited_frame_counter_.Add(quality_downscales_ > 0);
    if (quality_downscales_ > 0)
      uma_container_->quality_downscales_counter_.Add(quality_downscales_);
  }
  if (encoded_image.adapt_reason_.bw_resolutions_disabled != -1) {
    bool bw_limited = encoded_image.adapt_reason_.bw_resolutions_disabled > 0;
    uma_container_->bw_limited_frame_counter_.Add(bw_limited);
    if (bw_limited) {
      uma_container_->bw_resolutions_disabled_counter_.Add(
          encoded_image.adapt_reason_.bw_resolutions_disabled);
    }
  }

  if (encoded_image.qp_ != -1) {
    if (!stats_.qp_sum)
      stats_.qp_sum = rtc::Optional<uint64_t>(0);
    *stats_.qp_sum += encoded_image.qp_;

    if (codec_info) {
      if (codec_info->codecType == kVideoCodecVP8) {
        int spatial_idx = (rtp_config_.ssrcs.size() == 1)
                              ? -1
                              : static_cast<int>(simulcast_idx);
        uma_container_->qp_counters_[spatial_idx].vp8.Add(encoded_image.qp_);
      } else if (codec_info->codecType == kVideoCodecVP9) {
        int spatial_idx =
            (codec_info->codecSpecific.VP9.num_spatial_layers == 1)
                ? -1
                : codec_info->codecSpecific.VP9.spatial_idx;
        uma_container_->qp_counters_[spatial_idx].vp9.Add(encoded_image.qp_);
      } else if (codec_info->codecType == kVideoCodecH264) {
        int spatial_idx = -1;
        uma_container_->qp_counters_[spatial_idx].h264.Add(encoded_image.qp_);
      }
    }
  }

  media_byte_rate_tracker_.AddSamples(encoded_image._length);
  if (uma_container_->InsertEncodedFrame(encoded_image))
    encoded_frame_rate_tracker_.AddSamples(1);
}

int SendStatisticsProxy::GetSendFrameRate() const {
  rtc::CritScope lock(&crit_);
  return round(encoded_frame_rate_tracker_.ComputeRate());
}

void SendStatisticsProxy::OnIncomingFrame(int width, int height) {
  rtc::CritScope lock(&crit_);
  uma_container_->input_frame_rate_tracker_.AddSamples(1);
  uma_container_->input_fps_counter_.Add(1);
  uma_container_->input_width_counter_.Add(width);
  uma_container_->input_height_counter_.Add(height);
  if (cpu_downscales_ >= 0) {
    uma_container_->cpu_limited_frame_counter_.Add(
        stats_.cpu_limited_resolution);
  }
  if (encoded_frame_rate_tracker_.TotalSampleCount() == 0) {
    // Set start time now instead of when first key frame is encoded to avoid a
    // too high initial estimate.
    encoded_frame_rate_tracker_.AddSamples(0);
  }
}

void SendStatisticsProxy::OnFrameDroppedBySource() {
  rtc::CritScope lock(&crit_);
  ++stats_.frames_dropped_by_capturer;
}

void SendStatisticsProxy::OnFrameDroppedInEncoderQueue() {
  rtc::CritScope lock(&crit_);
  ++stats_.frames_dropped_by_encoder_queue;
}

void SendStatisticsProxy::OnFrameDroppedByEncoder() {
  rtc::CritScope lock(&crit_);
  ++stats_.frames_dropped_by_encoder;
}

void SendStatisticsProxy::OnFrameDroppedByMediaOptimizations() {
  rtc::CritScope lock(&crit_);
  ++stats_.frames_dropped_by_rate_limiter;
}

void SendStatisticsProxy::SetAdaptationStats(
    const VideoStreamEncoder::AdaptCounts& cpu_counts,
    const VideoStreamEncoder::AdaptCounts& quality_counts) {
  rtc::CritScope lock(&crit_);
  SetAdaptTimer(cpu_counts, &uma_container_->cpu_adapt_timer_);
  SetAdaptTimer(quality_counts, &uma_container_->quality_adapt_timer_);
  UpdateAdaptationStats(cpu_counts, quality_counts);
}

void SendStatisticsProxy::OnCpuAdaptationChanged(
    const VideoStreamEncoder::AdaptCounts& cpu_counts,
    const VideoStreamEncoder::AdaptCounts& quality_counts) {
  rtc::CritScope lock(&crit_);
  ++stats_.number_of_cpu_adapt_changes;
  UpdateAdaptationStats(cpu_counts, quality_counts);
}

void SendStatisticsProxy::OnQualityAdaptationChanged(
    const VideoStreamEncoder::AdaptCounts& cpu_counts,
    const VideoStreamEncoder::AdaptCounts& quality_counts) {
  rtc::CritScope lock(&crit_);
  ++stats_.number_of_quality_adapt_changes;
  UpdateAdaptationStats(cpu_counts, quality_counts);
}

void SendStatisticsProxy::UpdateAdaptationStats(
    const VideoStreamEncoder::AdaptCounts& cpu_counts,
    const VideoStreamEncoder::AdaptCounts& quality_counts) {
  cpu_downscales_ = cpu_counts.resolution;
  quality_downscales_ = quality_counts.resolution;

  stats_.cpu_limited_resolution = cpu_counts.resolution > 0;
  stats_.cpu_limited_framerate = cpu_counts.fps > 0;
  stats_.bw_limited_resolution = quality_counts.resolution > 0;
  stats_.bw_limited_framerate = quality_counts.fps > 0;
}

void SendStatisticsProxy::SetAdaptTimer(
    const VideoStreamEncoder::AdaptCounts& counts,
    StatsTimer* timer) {
  if (counts.resolution >= 0 || counts.fps >= 0) {
    // Adaptation enabled.
    if (!stats_.suspended)
      timer->Start(clock_->TimeInMilliseconds());
    return;
  }
  timer->Stop(clock_->TimeInMilliseconds());
}

void SendStatisticsProxy::RtcpPacketTypesCounterUpdated(
    uint32_t ssrc,
    const RtcpPacketTypeCounter& packet_counter) {
  rtc::CritScope lock(&crit_);
  VideoSendStream::StreamStats* stats = GetStatsEntry(ssrc);
  if (!stats)
    return;

  stats->rtcp_packet_type_counts = packet_counter;
  if (uma_container_->first_rtcp_stats_time_ms_ == -1)
    uma_container_->first_rtcp_stats_time_ms_ = clock_->TimeInMilliseconds();
}

void SendStatisticsProxy::StatisticsUpdated(const RtcpStatistics& statistics,
                                            uint32_t ssrc) {
  rtc::CritScope lock(&crit_);
  VideoSendStream::StreamStats* stats = GetStatsEntry(ssrc);
  if (!stats)
    return;

  stats->rtcp_stats = statistics;
  uma_container_->report_block_stats_.Store(statistics, 0, ssrc);
}

void SendStatisticsProxy::CNameChanged(const char* cname, uint32_t ssrc) {}

void SendStatisticsProxy::DataCountersUpdated(
    const StreamDataCounters& counters,
    uint32_t ssrc) {
  rtc::CritScope lock(&crit_);
  VideoSendStream::StreamStats* stats = GetStatsEntry(ssrc);
  RTC_DCHECK(stats) << "DataCountersUpdated reported for unknown ssrc " << ssrc;

  if (stats->is_flexfec) {
    // The same counters are reported for both the media ssrc and flexfec ssrc.
    // Bitrate stats are summed for all SSRCs. Use fec stats from media update.
    return;
  }

  stats->rtp_stats = counters;
  if (uma_container_->first_rtp_stats_time_ms_ == -1) {
    int64_t now_ms = clock_->TimeInMilliseconds();
    uma_container_->first_rtp_stats_time_ms_ = now_ms;
    uma_container_->cpu_adapt_timer_.Restart(now_ms);
    uma_container_->quality_adapt_timer_.Restart(now_ms);
  }

  uma_container_->total_byte_counter_.Set(counters.transmitted.TotalBytes(),
                                          ssrc);
  uma_container_->padding_byte_counter_.Set(counters.transmitted.padding_bytes,
                                            ssrc);
  uma_container_->retransmit_byte_counter_.Set(
      counters.retransmitted.TotalBytes(), ssrc);
  uma_container_->fec_byte_counter_.Set(counters.fec.TotalBytes(), ssrc);
  if (stats->is_rtx) {
    uma_container_->rtx_byte_counter_.Set(counters.transmitted.TotalBytes(),
                                          ssrc);
  } else {
    uma_container_->media_byte_counter_.Set(counters.MediaPayloadBytes(), ssrc);
  }
}

void SendStatisticsProxy::Notify(uint32_t total_bitrate_bps,
                                 uint32_t retransmit_bitrate_bps,
                                 uint32_t ssrc) {
  rtc::CritScope lock(&crit_);
  VideoSendStream::StreamStats* stats = GetStatsEntry(ssrc);
  if (!stats)
    return;

  stats->total_bitrate_bps = total_bitrate_bps;
  stats->retransmit_bitrate_bps = retransmit_bitrate_bps;
}

void SendStatisticsProxy::FrameCountUpdated(const FrameCounts& frame_counts,
                                            uint32_t ssrc) {
  rtc::CritScope lock(&crit_);
  VideoSendStream::StreamStats* stats = GetStatsEntry(ssrc);
  if (!stats)
    return;

  stats->frame_counts = frame_counts;
}

void SendStatisticsProxy::SendSideDelayUpdated(int avg_delay_ms,
                                               int max_delay_ms,
                                               uint32_t ssrc) {
  rtc::CritScope lock(&crit_);
  VideoSendStream::StreamStats* stats = GetStatsEntry(ssrc);
  if (!stats)
    return;
  stats->avg_delay_ms = avg_delay_ms;
  stats->max_delay_ms = max_delay_ms;

  uma_container_->delay_counter_.Add(avg_delay_ms);
  uma_container_->max_delay_counter_.Add(max_delay_ms);
}

void SendStatisticsProxy::StatsTimer::Start(int64_t now_ms) {
  if (start_ms == -1)
    start_ms = now_ms;
}

void SendStatisticsProxy::StatsTimer::Stop(int64_t now_ms) {
  if (start_ms != -1) {
    total_ms += now_ms - start_ms;
    start_ms = -1;
  }
}

void SendStatisticsProxy::StatsTimer::Restart(int64_t now_ms) {
  total_ms = 0;
  if (start_ms != -1)
    start_ms = now_ms;
}

void SendStatisticsProxy::SampleCounter::Add(int sample) {
  sum += sample;
  ++num_samples;
}

int SendStatisticsProxy::SampleCounter::Avg(
    int64_t min_required_samples) const {
  if (num_samples < min_required_samples || num_samples == 0)
    return -1;
  return static_cast<int>((sum + (num_samples / 2)) / num_samples);
}

void SendStatisticsProxy::BoolSampleCounter::Add(bool sample) {
  if (sample)
    ++sum;
  ++num_samples;
}

void SendStatisticsProxy::BoolSampleCounter::Add(bool sample, int64_t count) {
  if (sample)
    sum += count;
  num_samples += count;
}
int SendStatisticsProxy::BoolSampleCounter::Percent(
    int64_t min_required_samples) const {
  return Fraction(min_required_samples, 100.0f);
}

int SendStatisticsProxy::BoolSampleCounter::Permille(
    int64_t min_required_samples) const {
  return Fraction(min_required_samples, 1000.0f);
}

int SendStatisticsProxy::BoolSampleCounter::Fraction(
    int64_t min_required_samples,
    float multiplier) const {
  if (num_samples < min_required_samples || num_samples == 0)
    return -1;
  return static_cast<int>((sum * multiplier / num_samples) + 0.5f);
}
}  // namespace webrtc
