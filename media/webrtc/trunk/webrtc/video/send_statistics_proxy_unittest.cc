/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file includes unit tests for SendStatisticsProxy.
#include "webrtc/video/send_statistics_proxy.h"

#include <map>
#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace webrtc {

class SendStatisticsProxyTest : public ::testing::Test {
 public:
  SendStatisticsProxyTest()
      : fake_clock_(1234), avg_delay_ms_(0), max_delay_ms_(0) {}
  virtual ~SendStatisticsProxyTest() {}

 protected:
  virtual void SetUp() {
    statistics_proxy_.reset(
        new SendStatisticsProxy(&fake_clock_, GetTestConfig()));
    config_ = GetTestConfig();
    expected_ = VideoSendStream::Stats();
  }

  VideoSendStream::Config GetTestConfig() {
    VideoSendStream::Config config;
    config.rtp.ssrcs.push_back(17);
    config.rtp.ssrcs.push_back(42);
    config.rtp.rtx.ssrcs.push_back(18);
    config.rtp.rtx.ssrcs.push_back(43);
    return config;
  }

  void ExpectEqual(VideoSendStream::Stats one, VideoSendStream::Stats other) {
    EXPECT_EQ(one.input_frame_rate, other.input_frame_rate);
    EXPECT_EQ(one.encode_frame_rate, other.encode_frame_rate);
    EXPECT_EQ(one.media_bitrate_bps, other.media_bitrate_bps);
    EXPECT_EQ(one.suspended, other.suspended);

    EXPECT_EQ(one.substreams.size(), other.substreams.size());
    for (std::map<uint32_t, VideoSendStream::StreamStats>::const_iterator it =
             one.substreams.begin();
         it != one.substreams.end(); ++it) {
      std::map<uint32_t, VideoSendStream::StreamStats>::const_iterator
          corresponding_it = other.substreams.find(it->first);
      ASSERT_TRUE(corresponding_it != other.substreams.end());
      const VideoSendStream::StreamStats& a = it->second;
      const VideoSendStream::StreamStats& b = corresponding_it->second;

      EXPECT_EQ(a.frame_counts.key_frames, b.frame_counts.key_frames);
      EXPECT_EQ(a.frame_counts.delta_frames, b.frame_counts.delta_frames);
      EXPECT_EQ(a.total_bitrate_bps, b.total_bitrate_bps);
      EXPECT_EQ(a.avg_delay_ms, b.avg_delay_ms);
      EXPECT_EQ(a.max_delay_ms, b.max_delay_ms);

      EXPECT_EQ(a.rtp_stats.transmitted.payload_bytes,
                b.rtp_stats.transmitted.payload_bytes);
      EXPECT_EQ(a.rtp_stats.transmitted.header_bytes,
                b.rtp_stats.transmitted.header_bytes);
      EXPECT_EQ(a.rtp_stats.transmitted.padding_bytes,
                b.rtp_stats.transmitted.padding_bytes);
      EXPECT_EQ(a.rtp_stats.transmitted.packets,
                b.rtp_stats.transmitted.packets);
      EXPECT_EQ(a.rtp_stats.retransmitted.packets,
                b.rtp_stats.retransmitted.packets);
      EXPECT_EQ(a.rtp_stats.fec.packets, b.rtp_stats.fec.packets);

      EXPECT_EQ(a.rtcp_stats.fraction_lost, b.rtcp_stats.fraction_lost);
      EXPECT_EQ(a.rtcp_stats.cumulative_lost, b.rtcp_stats.cumulative_lost);
      EXPECT_EQ(a.rtcp_stats.extended_max_sequence_number,
                b.rtcp_stats.extended_max_sequence_number);
      EXPECT_EQ(a.rtcp_stats.jitter, b.rtcp_stats.jitter);
    }
  }

  rtc::scoped_ptr<SendStatisticsProxy> statistics_proxy_;
  SimulatedClock fake_clock_;
  VideoSendStream::Config config_;
  int avg_delay_ms_;
  int max_delay_ms_;
  VideoSendStream::Stats expected_;
  typedef std::map<uint32_t, VideoSendStream::StreamStats>::const_iterator
      StreamIterator;
};

TEST_F(SendStatisticsProxyTest, RtcpStatistics) {
  RtcpStatisticsCallback* callback = statistics_proxy_.get();
  for (std::vector<uint32_t>::const_iterator it = config_.rtp.ssrcs.begin();
       it != config_.rtp.ssrcs.end();
       ++it) {
    const uint32_t ssrc = *it;
    VideoSendStream::StreamStats& ssrc_stats = expected_.substreams[ssrc];

    // Add statistics with some arbitrary, but unique, numbers.
    uint32_t offset = ssrc * sizeof(RtcpStatistics);
    ssrc_stats.rtcp_stats.cumulative_lost = offset;
    ssrc_stats.rtcp_stats.extended_max_sequence_number = offset + 1;
    ssrc_stats.rtcp_stats.fraction_lost = offset + 2;
    ssrc_stats.rtcp_stats.jitter = offset + 3;
    callback->StatisticsUpdated(ssrc_stats.rtcp_stats, ssrc);
  }
  for (std::vector<uint32_t>::const_iterator it = config_.rtp.rtx.ssrcs.begin();
       it != config_.rtp.rtx.ssrcs.end();
       ++it) {
    const uint32_t ssrc = *it;
    VideoSendStream::StreamStats& ssrc_stats = expected_.substreams[ssrc];

    // Add statistics with some arbitrary, but unique, numbers.
    uint32_t offset = ssrc * sizeof(RtcpStatistics);
    ssrc_stats.rtcp_stats.cumulative_lost = offset;
    ssrc_stats.rtcp_stats.extended_max_sequence_number = offset + 1;
    ssrc_stats.rtcp_stats.fraction_lost = offset + 2;
    ssrc_stats.rtcp_stats.jitter = offset + 3;
    callback->StatisticsUpdated(ssrc_stats.rtcp_stats, ssrc);
  }
  VideoSendStream::Stats stats = statistics_proxy_->GetStats();
  ExpectEqual(expected_, stats);
}

TEST_F(SendStatisticsProxyTest, EncodedBitrateAndFramerate) {
  const int media_bitrate_bps = 500;
  const int encode_fps = 29;

  ViEEncoderObserver* encoder_observer = statistics_proxy_.get();
  encoder_observer->OutgoingRate(0, encode_fps, media_bitrate_bps);

  VideoSendStream::Stats stats = statistics_proxy_->GetStats();
  EXPECT_EQ(media_bitrate_bps, stats.media_bitrate_bps);
  EXPECT_EQ(encode_fps, stats.encode_frame_rate);
}

TEST_F(SendStatisticsProxyTest, Suspended) {
  // Verify that the value is false by default.
  EXPECT_FALSE(statistics_proxy_->GetStats().suspended);

  // Verify that we can set it to true.
  ViEEncoderObserver* encoder_observer = statistics_proxy_.get();
  encoder_observer->SuspendChange(0, true);
  EXPECT_TRUE(statistics_proxy_->GetStats().suspended);

  // Verify that we can set it back to false again.
  encoder_observer->SuspendChange(0, false);
  EXPECT_FALSE(statistics_proxy_->GetStats().suspended);
}

TEST_F(SendStatisticsProxyTest, FrameCounts) {
  FrameCountObserver* observer = statistics_proxy_.get();
  for (std::vector<uint32_t>::const_iterator it = config_.rtp.ssrcs.begin();
       it != config_.rtp.ssrcs.end();
       ++it) {
    const uint32_t ssrc = *it;
    // Add statistics with some arbitrary, but unique, numbers.
    VideoSendStream::StreamStats& stats = expected_.substreams[ssrc];
    uint32_t offset = ssrc * sizeof(VideoSendStream::StreamStats);
    FrameCounts frame_counts;
    frame_counts.key_frames = offset;
    frame_counts.delta_frames = offset + 1;
    stats.frame_counts = frame_counts;
    observer->FrameCountUpdated(frame_counts, ssrc);
  }
  for (std::vector<uint32_t>::const_iterator it = config_.rtp.rtx.ssrcs.begin();
       it != config_.rtp.rtx.ssrcs.end();
       ++it) {
    const uint32_t ssrc = *it;
    // Add statistics with some arbitrary, but unique, numbers.
    VideoSendStream::StreamStats& stats = expected_.substreams[ssrc];
    uint32_t offset = ssrc * sizeof(VideoSendStream::StreamStats);
    FrameCounts frame_counts;
    frame_counts.key_frames = offset;
    frame_counts.delta_frames = offset + 1;
    stats.frame_counts = frame_counts;
    observer->FrameCountUpdated(frame_counts, ssrc);
  }

  VideoSendStream::Stats stats = statistics_proxy_->GetStats();
  ExpectEqual(expected_, stats);
}

TEST_F(SendStatisticsProxyTest, DataCounters) {
  StreamDataCountersCallback* callback = statistics_proxy_.get();
  for (std::vector<uint32_t>::const_iterator it = config_.rtp.ssrcs.begin();
       it != config_.rtp.ssrcs.end();
       ++it) {
    const uint32_t ssrc = *it;
    StreamDataCounters& counters = expected_.substreams[ssrc].rtp_stats;
    // Add statistics with some arbitrary, but unique, numbers.
    size_t offset = ssrc * sizeof(StreamDataCounters);
    uint32_t offset_uint32 = static_cast<uint32_t>(offset);
    counters.transmitted.payload_bytes = offset;
    counters.transmitted.header_bytes = offset + 1;
    counters.fec.packets = offset_uint32 + 2;
    counters.transmitted.padding_bytes = offset + 3;
    counters.retransmitted.packets = offset_uint32 + 4;
    counters.transmitted.packets = offset_uint32 + 5;
    callback->DataCountersUpdated(counters, ssrc);
  }
  for (std::vector<uint32_t>::const_iterator it = config_.rtp.rtx.ssrcs.begin();
       it != config_.rtp.rtx.ssrcs.end();
       ++it) {
    const uint32_t ssrc = *it;
    StreamDataCounters& counters = expected_.substreams[ssrc].rtp_stats;
    // Add statistics with some arbitrary, but unique, numbers.
    size_t offset = ssrc * sizeof(StreamDataCounters);
    uint32_t offset_uint32 = static_cast<uint32_t>(offset);
    counters.transmitted.payload_bytes = offset;
    counters.transmitted.header_bytes = offset + 1;
    counters.fec.packets = offset_uint32 + 2;
    counters.transmitted.padding_bytes = offset + 3;
    counters.retransmitted.packets = offset_uint32 + 4;
    counters.transmitted.packets = offset_uint32 + 5;
    callback->DataCountersUpdated(counters, ssrc);
  }

  VideoSendStream::Stats stats = statistics_proxy_->GetStats();
  ExpectEqual(expected_, stats);
}

TEST_F(SendStatisticsProxyTest, Bitrate) {
  BitrateStatisticsObserver* observer = statistics_proxy_.get();
  for (std::vector<uint32_t>::const_iterator it = config_.rtp.ssrcs.begin();
       it != config_.rtp.ssrcs.end();
       ++it) {
    const uint32_t ssrc = *it;
    BitrateStatistics total;
    BitrateStatistics retransmit;
    // Use ssrc as bitrate_bps to get a unique value for each stream.
    total.bitrate_bps = ssrc;
    retransmit.bitrate_bps = ssrc + 1;
    observer->Notify(total, retransmit, ssrc);
    expected_.substreams[ssrc].total_bitrate_bps = total.bitrate_bps;
    expected_.substreams[ssrc].retransmit_bitrate_bps = retransmit.bitrate_bps;
  }
  for (std::vector<uint32_t>::const_iterator it = config_.rtp.rtx.ssrcs.begin();
       it != config_.rtp.rtx.ssrcs.end();
       ++it) {
    const uint32_t ssrc = *it;
    BitrateStatistics total;
    BitrateStatistics retransmit;
    // Use ssrc as bitrate_bps to get a unique value for each stream.
    total.bitrate_bps = ssrc;
    retransmit.bitrate_bps = ssrc + 1;
    observer->Notify(total, retransmit, ssrc);
    expected_.substreams[ssrc].total_bitrate_bps = total.bitrate_bps;
    expected_.substreams[ssrc].retransmit_bitrate_bps = retransmit.bitrate_bps;
  }

  VideoSendStream::Stats stats = statistics_proxy_->GetStats();
  ExpectEqual(expected_, stats);
}

TEST_F(SendStatisticsProxyTest, SendSideDelay) {
  SendSideDelayObserver* observer = statistics_proxy_.get();
  for (std::vector<uint32_t>::const_iterator it = config_.rtp.ssrcs.begin();
       it != config_.rtp.ssrcs.end();
       ++it) {
    const uint32_t ssrc = *it;
    // Use ssrc as avg_delay_ms and max_delay_ms to get a unique value for each
    // stream.
    int avg_delay_ms = ssrc;
    int max_delay_ms = ssrc + 1;
    observer->SendSideDelayUpdated(avg_delay_ms, max_delay_ms, ssrc);
    expected_.substreams[ssrc].avg_delay_ms = avg_delay_ms;
    expected_.substreams[ssrc].max_delay_ms = max_delay_ms;
  }
  for (std::vector<uint32_t>::const_iterator it = config_.rtp.rtx.ssrcs.begin();
       it != config_.rtp.rtx.ssrcs.end();
       ++it) {
    const uint32_t ssrc = *it;
    // Use ssrc as avg_delay_ms and max_delay_ms to get a unique value for each
    // stream.
    int avg_delay_ms = ssrc;
    int max_delay_ms = ssrc + 1;
    observer->SendSideDelayUpdated(avg_delay_ms, max_delay_ms, ssrc);
    expected_.substreams[ssrc].avg_delay_ms = avg_delay_ms;
    expected_.substreams[ssrc].max_delay_ms = max_delay_ms;
  }
  VideoSendStream::Stats stats = statistics_proxy_->GetStats();
  ExpectEqual(expected_, stats);
}

TEST_F(SendStatisticsProxyTest, NoSubstreams) {
  uint32_t excluded_ssrc =
      std::max(
          *std::max_element(config_.rtp.ssrcs.begin(), config_.rtp.ssrcs.end()),
          *std::max_element(config_.rtp.rtx.ssrcs.begin(),
                            config_.rtp.rtx.ssrcs.end())) +
      1;
  // From RtcpStatisticsCallback.
  RtcpStatistics rtcp_stats;
  RtcpStatisticsCallback* rtcp_callback = statistics_proxy_.get();
  rtcp_callback->StatisticsUpdated(rtcp_stats, excluded_ssrc);

  // From BitrateStatisticsObserver.
  BitrateStatistics total;
  BitrateStatistics retransmit;
  BitrateStatisticsObserver* bitrate_observer = statistics_proxy_.get();
  bitrate_observer->Notify(total, retransmit, excluded_ssrc);

  // From FrameCountObserver.
  FrameCountObserver* fps_observer = statistics_proxy_.get();
  FrameCounts frame_counts;
  frame_counts.key_frames = 1;
  fps_observer->FrameCountUpdated(frame_counts, excluded_ssrc);

  VideoSendStream::Stats stats = statistics_proxy_->GetStats();
  EXPECT_TRUE(stats.substreams.empty());
}

TEST_F(SendStatisticsProxyTest, EncodedResolutionTimesOut) {
  static const int kEncodedWidth = 123;
  static const int kEncodedHeight = 81;
  EncodedImage encoded_image;
  encoded_image._encodedWidth = kEncodedWidth;
  encoded_image._encodedHeight = kEncodedHeight;

  RTPVideoHeader rtp_video_header;

  rtp_video_header.simulcastIdx = 0;
  statistics_proxy_->OnSendEncodedImage(encoded_image, &rtp_video_header);
  rtp_video_header.simulcastIdx = 1;
  statistics_proxy_->OnSendEncodedImage(encoded_image, &rtp_video_header);

  VideoSendStream::Stats stats = statistics_proxy_->GetStats();
  EXPECT_EQ(kEncodedWidth, stats.substreams[config_.rtp.ssrcs[0]].width);
  EXPECT_EQ(kEncodedHeight, stats.substreams[config_.rtp.ssrcs[0]].height);
  EXPECT_EQ(kEncodedWidth, stats.substreams[config_.rtp.ssrcs[1]].width);
  EXPECT_EQ(kEncodedHeight, stats.substreams[config_.rtp.ssrcs[1]].height);

  // Forward almost to timeout, this should not have removed stats.
  fake_clock_.AdvanceTimeMilliseconds(SendStatisticsProxy::kStatsTimeoutMs - 1);
  stats = statistics_proxy_->GetStats();
  EXPECT_EQ(kEncodedWidth, stats.substreams[config_.rtp.ssrcs[0]].width);
  EXPECT_EQ(kEncodedHeight, stats.substreams[config_.rtp.ssrcs[0]].height);

  // Update the first SSRC with bogus RTCP stats to make sure that encoded
  // resolution still times out (no global timeout for all stats).
  RtcpStatistics rtcp_statistics;
  RtcpStatisticsCallback* rtcp_stats = statistics_proxy_.get();
  rtcp_stats->StatisticsUpdated(rtcp_statistics, config_.rtp.ssrcs[0]);

  // Report stats for second SSRC to make sure it's not outdated along with the
  // first SSRC.
  rtp_video_header.simulcastIdx = 1;
  statistics_proxy_->OnSendEncodedImage(encoded_image, &rtp_video_header);

  // Forward 1 ms, reach timeout, substream 0 should have no resolution
  // reported, but substream 1 should.
  fake_clock_.AdvanceTimeMilliseconds(1);
  stats = statistics_proxy_->GetStats();
  EXPECT_EQ(0, stats.substreams[config_.rtp.ssrcs[0]].width);
  EXPECT_EQ(0, stats.substreams[config_.rtp.ssrcs[0]].height);
  EXPECT_EQ(kEncodedWidth, stats.substreams[config_.rtp.ssrcs[1]].width);
  EXPECT_EQ(kEncodedHeight, stats.substreams[config_.rtp.ssrcs[1]].height);
}

}  // namespace webrtc
