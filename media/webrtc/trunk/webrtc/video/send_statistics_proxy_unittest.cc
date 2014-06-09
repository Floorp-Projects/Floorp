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

class SendStatisticsProxyTest : public ::testing::Test,
                                protected SendStatisticsProxy::StatsProvider {
 public:
  SendStatisticsProxyTest() : avg_delay_ms_(0), max_delay_ms_(0) {}
  virtual ~SendStatisticsProxyTest() {}

 protected:
  virtual void SetUp() {
    statistics_proxy_.reset(
        new SendStatisticsProxy(GetTestConfig(), this));
    config_ = GetTestConfig();
    expected_ = VideoSendStream::Stats();
  }

  VideoSendStream::Config GetTestConfig() {
    VideoSendStream::Config config;
    config.rtp.ssrcs.push_back(17);
    config.rtp.ssrcs.push_back(42);
    return config;
  }

  virtual bool GetSendSideDelay(VideoSendStream::Stats* stats) OVERRIDE {
    stats->avg_delay_ms = avg_delay_ms_;
    stats->max_delay_ms = max_delay_ms_;
    return true;
  }

  virtual std::string GetCName() { return cname_; }

  void ExpectEqual(VideoSendStream::Stats one, VideoSendStream::Stats other) {
    EXPECT_EQ(one.avg_delay_ms, other.avg_delay_ms);
    EXPECT_EQ(one.input_frame_rate, other.input_frame_rate);
    EXPECT_EQ(one.encode_frame_rate, other.encode_frame_rate);
    EXPECT_EQ(one.avg_delay_ms, other.avg_delay_ms);
    EXPECT_EQ(one.max_delay_ms, other.max_delay_ms);
    EXPECT_EQ(one.c_name, other.c_name);

    EXPECT_EQ(one.substreams.size(), other.substreams.size());
    for (std::map<uint32_t, StreamStats>::const_iterator it =
             one.substreams.begin();
         it != one.substreams.end();
         ++it) {
      std::map<uint32_t, StreamStats>::const_iterator corresponding_it =
          other.substreams.find(it->first);
      ASSERT_TRUE(corresponding_it != other.substreams.end());
      const StreamStats& a = it->second;
      const StreamStats& b = corresponding_it->second;

      EXPECT_EQ(a.key_frames, b.key_frames);
      EXPECT_EQ(a.delta_frames, b.delta_frames);
      EXPECT_EQ(a.bitrate_bps, b.bitrate_bps);

      EXPECT_EQ(a.rtp_stats.bytes, b.rtp_stats.bytes);
      EXPECT_EQ(a.rtp_stats.header_bytes, b.rtp_stats.header_bytes);
      EXPECT_EQ(a.rtp_stats.padding_bytes, b.rtp_stats.padding_bytes);
      EXPECT_EQ(a.rtp_stats.packets, b.rtp_stats.packets);
      EXPECT_EQ(a.rtp_stats.retransmitted_packets,
                b.rtp_stats.retransmitted_packets);
      EXPECT_EQ(a.rtp_stats.fec_packets, b.rtp_stats.fec_packets);

      EXPECT_EQ(a.rtcp_stats.fraction_lost, b.rtcp_stats.fraction_lost);
      EXPECT_EQ(a.rtcp_stats.cumulative_lost, b.rtcp_stats.cumulative_lost);
      EXPECT_EQ(a.rtcp_stats.extended_max_sequence_number,
                b.rtcp_stats.extended_max_sequence_number);
      EXPECT_EQ(a.rtcp_stats.jitter, b.rtcp_stats.jitter);
    }
  }

  scoped_ptr<SendStatisticsProxy> statistics_proxy_;
  VideoSendStream::Config config_;
  int avg_delay_ms_;
  int max_delay_ms_;
  std::string cname_;
  VideoSendStream::Stats expected_;
  typedef std::map<uint32_t, StreamStats>::const_iterator StreamIterator;
};

TEST_F(SendStatisticsProxyTest, RtcpStatistics) {
  RtcpStatisticsCallback* callback = statistics_proxy_.get();
  for (std::vector<uint32_t>::const_iterator it = config_.rtp.ssrcs.begin();
       it != config_.rtp.ssrcs.end();
       ++it) {
    const uint32_t ssrc = *it;
    StreamStats& ssrc_stats = expected_.substreams[ssrc];

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

TEST_F(SendStatisticsProxyTest, FrameRates) {
  const int capture_fps = 31;
  const int encode_fps = 29;

  ViECaptureObserver* capture_observer = statistics_proxy_.get();
  capture_observer->CapturedFrameRate(0, capture_fps);
  ViEEncoderObserver* encoder_observer = statistics_proxy_.get();
  encoder_observer->OutgoingRate(0, encode_fps, 0);

  VideoSendStream::Stats stats = statistics_proxy_->GetStats();
  EXPECT_EQ(capture_fps, stats.input_frame_rate);
  EXPECT_EQ(encode_fps, stats.encode_frame_rate);
}

TEST_F(SendStatisticsProxyTest, FrameCounts) {
  FrameCountObserver* observer = statistics_proxy_.get();
  for (std::vector<uint32_t>::const_iterator it = config_.rtp.ssrcs.begin();
       it != config_.rtp.ssrcs.end();
       ++it) {
    const uint32_t ssrc = *it;
    // Add statistics with some arbitrary, but unique, numbers.
    StreamStats& stats = expected_.substreams[ssrc];
    uint32_t offset = ssrc * sizeof(StreamStats);
    stats.key_frames = offset;
    stats.delta_frames = offset + 1;
    observer->FrameCountUpdated(kVideoFrameKey, stats.key_frames, ssrc);
    observer->FrameCountUpdated(kVideoFrameDelta, stats.delta_frames, ssrc);
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
    uint32_t offset = ssrc * sizeof(StreamDataCounters);
    counters.bytes = offset;
    counters.header_bytes = offset + 1;
    counters.fec_packets = offset + 2;
    counters.padding_bytes = offset + 3;
    counters.retransmitted_packets = offset + 4;
    counters.packets = offset + 5;
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
    BitrateStatistics bitrate;
    bitrate.bitrate_bps = ssrc;
    observer->Notify(bitrate, ssrc);
    expected_.substreams[ssrc].bitrate_bps = ssrc;
  }

  VideoSendStream::Stats stats = statistics_proxy_->GetStats();
  ExpectEqual(expected_, stats);
}

TEST_F(SendStatisticsProxyTest, StreamStats) {
  avg_delay_ms_ = 1;
  max_delay_ms_ = 2;
  cname_ = "qwertyuiop";

  VideoSendStream::Stats stats = statistics_proxy_->GetStats();

  EXPECT_EQ(avg_delay_ms_, stats.avg_delay_ms);
  EXPECT_EQ(max_delay_ms_, stats.max_delay_ms);
  EXPECT_EQ(cname_, stats.c_name);
}

TEST_F(SendStatisticsProxyTest, NoSubstreams) {
  uint32_t exluded_ssrc =
      *std::max_element(config_.rtp.ssrcs.begin(), config_.rtp.ssrcs.end()) + 1;
  // From RtcpStatisticsCallback.
  RtcpStatistics rtcp_stats;
  RtcpStatisticsCallback* rtcp_callback = statistics_proxy_.get();
  rtcp_callback->StatisticsUpdated(rtcp_stats, exluded_ssrc);

  // From StreamDataCountersCallback.
  StreamDataCounters rtp_stats;
  StreamDataCountersCallback* rtp_callback = statistics_proxy_.get();
  rtp_callback->DataCountersUpdated(rtp_stats, exluded_ssrc);

  // From BitrateStatisticsObserver.
  BitrateStatistics bitrate;
  BitrateStatisticsObserver* bitrate_observer = statistics_proxy_.get();
  bitrate_observer->Notify(bitrate, exluded_ssrc);

  // From FrameCountObserver.
  FrameCountObserver* fps_observer = statistics_proxy_.get();
  fps_observer->FrameCountUpdated(kVideoFrameKey, 1, exluded_ssrc);

  VideoSendStream::Stats stats = statistics_proxy_->GetStats();
  EXPECT_TRUE(stats.substreams.empty());
}

}  // namespace webrtc
