/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_RECEIVE_STATISTICS_PROXY_H_
#define WEBRTC_VIDEO_RECEIVE_STATISTICS_PROXY_H_

#include <string>

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/ratetracker.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/common_types.h"
#include "webrtc/frame_callback.h"
#include "webrtc/modules/remote_bitrate_estimator/rate_statistics.h"
#include "webrtc/modules/video_coding/include/video_coding_defines.h"
#include "webrtc/video/report_block_stats.h"
#include "webrtc/video/vie_channel.h"
#include "webrtc/video_receive_stream.h"
#include "webrtc/video_renderer.h"

namespace webrtc {

class Clock;
class ViECodec;
class ViEDecoderObserver;
struct CodecSpecificInfo;

class ReceiveStatisticsProxy : public VCMReceiveStatisticsCallback,
                               public RtcpStatisticsCallback,
                               public RtcpPacketTypeCounterObserver,
                               public StreamDataCountersCallback {
 public:
  ReceiveStatisticsProxy(uint32_t ssrc, Clock* clock);
  virtual ~ReceiveStatisticsProxy();

  VideoReceiveStream::Stats GetStats() const;

  void OnDecodedFrame();
  void OnRenderedFrame(int width, int height);
  void OnIncomingPayloadType(int payload_type);
  void OnDecoderImplementationName(const char* implementation_name);
  void OnIncomingRate(unsigned int framerate, unsigned int bitrate_bps);
  void OnDecoderTiming(int decode_ms,
                       int max_decode_ms,
                       int current_delay_ms,
                       int target_delay_ms,
                       int jitter_buffer_ms,
                       int min_playout_delay_ms,
                       int render_delay_ms,
                       int64_t rtt_ms);
  void ReceiveStateChange(VideoReceiveState state);

  void OnPreDecode(const EncodedImage& encoded_image,
                   const CodecSpecificInfo* codec_specific_info);

  // Overrides VCMReceiveStatisticsCallback.
  void OnReceiveRatesUpdated(uint32_t bitRate, uint32_t frameRate) override;
  void OnFrameCountsUpdated(const FrameCounts& frame_counts) override;
  void OnDiscardedPacketsUpdated(int discarded_packets) override;

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

 private:
  struct SampleCounter {
    SampleCounter() : sum(0), num_samples(0) {}
    void Add(int sample);
    int Avg(int min_required_samples) const;

   private:
    int sum;
    int num_samples;
  };
  struct QpCounters {
    SampleCounter vp8;
  };

  void UpdateHistograms() EXCLUSIVE_LOCKS_REQUIRED(crit_);

  Clock* const clock_;

  mutable rtc::CriticalSection crit_;
  VideoReceiveStream::Stats stats_ GUARDED_BY(crit_);
  RateStatistics decode_fps_estimator_ GUARDED_BY(crit_);
  RateStatistics renders_fps_estimator_ GUARDED_BY(crit_);
  rtc::RateTracker render_fps_tracker_ GUARDED_BY(crit_);
  rtc::RateTracker render_pixel_tracker_ GUARDED_BY(crit_);
  SampleCounter render_width_counter_ GUARDED_BY(crit_);
  SampleCounter render_height_counter_ GUARDED_BY(crit_);
  SampleCounter decode_time_counter_ GUARDED_BY(crit_);
  SampleCounter delay_counter_ GUARDED_BY(crit_);
  ReportBlockStats report_block_stats_ GUARDED_BY(crit_);
  QpCounters qp_counters_;  // Only accessed on the decoding thread.
  VideoReceiveState receive_state_ GUARDED_BY(crit_);
};

}  // namespace webrtc
#endif  // WEBRTC_VIDEO_RECEIVE_STATISTICS_PROXY_H_
