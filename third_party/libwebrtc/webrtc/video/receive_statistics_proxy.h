/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_RECEIVE_STATISTICS_PROXY_H_
#define VIDEO_RECEIVE_STATISTICS_PROXY_H_

#include <map>
#include <string>
#include <vector>

#include "api/optional.h"
#include "call/video_receive_stream.h"
#include "common_types.h"  // NOLINT(build/include)
#include "common_video/include/frame_callback.h"
#include "modules/video_coding/include/video_coding_defines.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/numerics/histogram_percentile_counter.h"
#include "rtc_base/numerics/moving_max_counter.h"
#include "rtc_base/rate_statistics.h"
#include "rtc_base/ratetracker.h"
#include "rtc_base/thread_annotations.h"
#include "video/quality_threshold.h"
#include "video/report_block_stats.h"
#include "video/stats_counter.h"
#include "video/video_stream_decoder.h"

namespace webrtc {

class Clock;
class ViECodec;
class ViEDecoderObserver;
struct CodecSpecificInfo;

class ReceiveStatisticsProxy : public VCMReceiveStatisticsCallback,
                               public RtcpStatisticsCallback,
                               public RtcpPacketTypeCounterObserver,
                               public StreamDataCountersCallback,
                               public CallStatsObserver {
 public:
  ReceiveStatisticsProxy(const VideoReceiveStream::Config* config,
                         Clock* clock);
  virtual ~ReceiveStatisticsProxy();

  VideoReceiveStream::Stats GetStats() const;

  void OnDecodedFrame(rtc::Optional<uint8_t> qp, VideoContentType content_type);
  void OnSyncOffsetUpdated(int64_t sync_offset_ms, double estimated_freq_khz);
  void OnRenderedFrame(const VideoFrame& frame);
  void OnIncomingPayloadType(int payload_type);
  void OnDecoderImplementationName(const char* implementation_name);
  void OnIncomingRate(unsigned int framerate, unsigned int bitrate_bps);

  void OnPreDecode(const EncodedImage& encoded_image,
                   const CodecSpecificInfo* codec_specific_info);

  // Indicates video stream has been paused (no incoming packets).
  void OnStreamInactive();

  // Overrides VCMReceiveStatisticsCallback.
  void OnReceiveRatesUpdated(uint32_t bitRate, uint32_t frameRate) override;
  void OnFrameCountsUpdated(const FrameCounts& frame_counts) override;
  void OnDiscardedPacketsUpdated(int discarded_packets) override;
  void OnCompleteFrame(bool is_keyframe,
                       size_t size_bytes,
                       VideoContentType content_type) override;
  void OnFrameBufferTimingsUpdated(int decode_ms,
                                   int max_decode_ms,
                                   int current_delay_ms,
                                   int target_delay_ms,
                                   int jitter_buffer_ms,
                                   int min_playout_delay_ms,
                                   int render_delay_ms) override;

  void OnTimingFrameInfoUpdated(const TimingFrameInfo& info) override;

  // Overrides RtcpStatisticsCallback.
  void StatisticsUpdated(const webrtc::RtcpStatistics& statistics,
                         uint32_t ssrc) override;
  void CNameChanged(const char* cname, uint32_t ssrc) override;

  // Overrides RtcpPacketTypeCounterObserver.
  void RtcpPacketTypesCounterUpdated(
      uint32_t ssrc,
      const RtcpPacketTypeCounter& packet_counter) override;
  // Overrides StreamDataCountersCallback.
  void DataCountersUpdated(const webrtc::StreamDataCounters& counters,
                           uint32_t ssrc) override;

  // Implements CallStatsObserver.
  void OnRttUpdate(int64_t avg_rtt_ms, int64_t max_rtt_ms) override;

 private:
  struct SampleCounter {
    SampleCounter() : sum(0), num_samples(0) {}
    void Add(int sample);
    int Avg(int64_t min_required_samples) const;
    int Max() const;
    void Reset();
    void Add(const SampleCounter& other);

   private:
    int64_t sum;
    int64_t num_samples;
    rtc::Optional<int> max;
  };

  struct QpCounters {
    SampleCounter vp8;
  };

  struct ContentSpecificStats {
    ContentSpecificStats();

    void Add(const ContentSpecificStats& other);

    SampleCounter e2e_delay_counter;
    SampleCounter interframe_delay_counter;
    int64_t flow_duration_ms = 0;
    int64_t total_media_bytes = 0;
    SampleCounter received_width;
    SampleCounter received_height;
    SampleCounter qp_counter;
    FrameCounts frame_counts;
    rtc::HistogramPercentileCounter interframe_delay_percentiles;
  };

  void UpdateHistograms() RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_);

  void QualitySample() RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_);

  // Removes info about old frames and then updates the framerate.
  void UpdateFramerate(int64_t now_ms) const
      RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_);

  Clock* const clock_;
  // Ownership of this object lies with the owner of the ReceiveStatisticsProxy
  // instance.  Lifetime is guaranteed to outlive |this|.
  // TODO(tommi): In practice the config_ reference is only used for accessing
  // config_.rtp.ulpfec.ulpfec_payload_type.  Instead of holding a pointer back,
  // we could just store the value of ulpfec_payload_type and change the
  // ReceiveStatisticsProxy() ctor to accept a const& of Config (since we'll
  // then no longer store a pointer to the object).
  const VideoReceiveStream::Config& config_;
  const int64_t start_ms_;

  rtc::CriticalSection crit_;
  int64_t last_sample_time_ RTC_GUARDED_BY(crit_);
  QualityThreshold fps_threshold_ RTC_GUARDED_BY(crit_);
  QualityThreshold qp_threshold_ RTC_GUARDED_BY(crit_);
  QualityThreshold variance_threshold_ RTC_GUARDED_BY(crit_);
  SampleCounter qp_sample_ RTC_GUARDED_BY(crit_);
  int num_bad_states_ RTC_GUARDED_BY(crit_);
  int num_certain_states_ RTC_GUARDED_BY(crit_);
  mutable VideoReceiveStream::Stats stats_ RTC_GUARDED_BY(crit_);
  RateStatistics decode_fps_estimator_ RTC_GUARDED_BY(crit_);
  RateStatistics renders_fps_estimator_ RTC_GUARDED_BY(crit_);
  rtc::RateTracker render_fps_tracker_ RTC_GUARDED_BY(crit_);
  rtc::RateTracker render_pixel_tracker_ RTC_GUARDED_BY(crit_);
  rtc::RateTracker total_byte_tracker_ RTC_GUARDED_BY(crit_);
  SampleCounter sync_offset_counter_ RTC_GUARDED_BY(crit_);
  SampleCounter decode_time_counter_ RTC_GUARDED_BY(crit_);
  SampleCounter jitter_buffer_delay_counter_ RTC_GUARDED_BY(crit_);
  SampleCounter target_delay_counter_ RTC_GUARDED_BY(crit_);
  SampleCounter current_delay_counter_ RTC_GUARDED_BY(crit_);
  SampleCounter delay_counter_ RTC_GUARDED_BY(crit_);
  mutable rtc::MovingMaxCounter<int> interframe_delay_max_moving_
      RTC_GUARDED_BY(crit_);
  std::map<VideoContentType, ContentSpecificStats> content_specific_stats_
      RTC_GUARDED_BY(crit_);
  MaxCounter freq_offset_counter_ RTC_GUARDED_BY(crit_);
  int64_t first_report_block_time_ms_ RTC_GUARDED_BY(crit_);
  ReportBlockStats report_block_stats_ RTC_GUARDED_BY(crit_);
  QpCounters qp_counters_;  // Only accessed on the decoding thread.
  std::map<uint32_t, StreamDataCounters> rtx_stats_ RTC_GUARDED_BY(crit_);
  int64_t avg_rtt_ms_ RTC_GUARDED_BY(crit_);
  mutable std::map<int64_t, size_t> frame_window_ RTC_GUARDED_BY(&crit_);
  VideoContentType last_content_type_ RTC_GUARDED_BY(&crit_);
  rtc::Optional<int64_t> last_decoded_frame_time_ms_ RTC_GUARDED_BY(&crit_);
  // Mutable because calling Max() on MovingMaxCounter is not const. Yet it is
  // called from const GetStats().
  mutable rtc::MovingMaxCounter<TimingFrameInfo> timing_frame_info_counter_
      RTC_GUARDED_BY(&crit_);
};

}  // namespace webrtc
#endif  // VIDEO_RECEIVE_STATISTICS_PROXY_H_
