/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/call/rampup_tests.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/platform_thread.h"
#include "webrtc/test/testsupport/perf_test.h"

namespace webrtc {
namespace {

static const int64_t kPollIntervalMs = 20;

std::vector<uint32_t> GenerateSsrcs(size_t num_streams, uint32_t ssrc_offset) {
  std::vector<uint32_t> ssrcs;
  for (size_t i = 0; i != num_streams; ++i)
    ssrcs.push_back(static_cast<uint32_t>(ssrc_offset + i));
  return ssrcs;
}
}  // namespace

RampUpTester::RampUpTester(size_t num_video_streams,
                           size_t num_audio_streams,
                           unsigned int start_bitrate_bps,
                           const std::string& extension_type,
                           bool rtx,
                           bool red)
    : EndToEndTest(test::CallTest::kLongTimeoutMs),
      event_(false, false),
      clock_(Clock::GetRealTimeClock()),
      num_video_streams_(num_video_streams),
      num_audio_streams_(num_audio_streams),
      rtx_(rtx),
      red_(red),
      send_stream_(nullptr),
      start_bitrate_bps_(start_bitrate_bps),
      start_bitrate_verified_(false),
      expected_bitrate_bps_(0),
      test_start_ms_(-1),
      ramp_up_finished_ms_(-1),
      extension_type_(extension_type),
      video_ssrcs_(GenerateSsrcs(num_video_streams_, 100)),
      video_rtx_ssrcs_(GenerateSsrcs(num_video_streams_, 200)),
      audio_ssrcs_(GenerateSsrcs(num_audio_streams_, 300)),
      poller_thread_(&BitrateStatsPollingThread,
                     this,
                     "BitrateStatsPollingThread"),
      sender_call_(nullptr) {
  EXPECT_LE(num_audio_streams_, 1u);
  if (rtx_) {
    for (size_t i = 0; i < video_ssrcs_.size(); ++i)
      rtx_ssrc_map_[video_rtx_ssrcs_[i]] = video_ssrcs_[i];
  }
}

RampUpTester::~RampUpTester() {
  event_.Set();
}

Call::Config RampUpTester::GetSenderCallConfig() {
  Call::Config call_config;
  if (start_bitrate_bps_ != 0) {
    call_config.bitrate_config.start_bitrate_bps = start_bitrate_bps_;
  }
  call_config.bitrate_config.min_bitrate_bps = 10000;
  return call_config;
}

void RampUpTester::OnVideoStreamsCreated(
    VideoSendStream* send_stream,
    const std::vector<VideoReceiveStream*>& receive_streams) {
  send_stream_ = send_stream;
}

test::PacketTransport* RampUpTester::CreateSendTransport(Call* sender_call) {
  send_transport_ = new test::PacketTransport(sender_call, this,
                                              test::PacketTransport::kSender,
                                              forward_transport_config_);
  return send_transport_;
}

size_t RampUpTester::GetNumVideoStreams() const {
  return num_video_streams_;
}

size_t RampUpTester::GetNumAudioStreams() const {
  return num_audio_streams_;
}

void RampUpTester::ModifyVideoConfigs(
    VideoSendStream::Config* send_config,
    std::vector<VideoReceiveStream::Config>* receive_configs,
    VideoEncoderConfig* encoder_config) {
  send_config->suspend_below_min_bitrate = true;

  if (num_video_streams_ == 1) {
    encoder_config->streams[0].target_bitrate_bps =
        encoder_config->streams[0].max_bitrate_bps = 2000000;
    // For single stream rampup until 1mbps
    expected_bitrate_bps_ = kSingleStreamTargetBps;
  } else {
    // For multi stream rampup until all streams are being sent. That means
    // enough birate to send all the target streams plus the min bitrate of
    // the last one.
    expected_bitrate_bps_ = encoder_config->streams.back().min_bitrate_bps;
    for (size_t i = 0; i < encoder_config->streams.size() - 1; ++i) {
      expected_bitrate_bps_ += encoder_config->streams[i].target_bitrate_bps;
    }
  }

  send_config->rtp.extensions.clear();

  bool remb;
  bool transport_cc;
  if (extension_type_ == RtpExtension::kAbsSendTime) {
    remb = true;
    transport_cc = false;
    send_config->rtp.extensions.push_back(
        RtpExtension(extension_type_.c_str(), kAbsSendTimeExtensionId));
  } else if (extension_type_ == RtpExtension::kTransportSequenceNumber) {
    remb = false;
    transport_cc = true;
    send_config->rtp.extensions.push_back(RtpExtension(
        extension_type_.c_str(), kTransportSequenceNumberExtensionId));
  } else {
    remb = true;
    transport_cc = false;
    send_config->rtp.extensions.push_back(RtpExtension(
        extension_type_.c_str(), kTransmissionTimeOffsetExtensionId));
  }

  send_config->rtp.nack.rtp_history_ms = test::CallTest::kNackRtpHistoryMs;
  send_config->rtp.ssrcs = video_ssrcs_;
  if (rtx_) {
    send_config->rtp.rtx.payload_type = test::CallTest::kSendRtxPayloadType;
    send_config->rtp.rtx.ssrcs = video_rtx_ssrcs_;
  }
  if (red_) {
    send_config->rtp.fec.ulpfec_payload_type =
        test::CallTest::kUlpfecPayloadType;
    send_config->rtp.fec.red_payload_type = test::CallTest::kRedPayloadType;
  }

  size_t i = 0;
  for (VideoReceiveStream::Config& recv_config : *receive_configs) {
    recv_config.rtp.remb = remb;
    recv_config.rtp.transport_cc = transport_cc;
    recv_config.rtp.extensions = send_config->rtp.extensions;

    recv_config.rtp.remote_ssrc = video_ssrcs_[i];
    recv_config.rtp.nack.rtp_history_ms = send_config->rtp.nack.rtp_history_ms;

    if (red_) {
      recv_config.rtp.fec.red_payload_type =
          send_config->rtp.fec.red_payload_type;
      recv_config.rtp.fec.ulpfec_payload_type =
          send_config->rtp.fec.ulpfec_payload_type;
    }

    if (rtx_) {
      recv_config.rtp.rtx[send_config->encoder_settings.payload_type].ssrc =
          video_rtx_ssrcs_[i];
      recv_config.rtp.rtx[send_config->encoder_settings.payload_type]
          .payload_type = send_config->rtp.rtx.payload_type;
    }
    ++i;
  }
}

void RampUpTester::ModifyAudioConfigs(
    AudioSendStream::Config* send_config,
    std::vector<AudioReceiveStream::Config>* receive_configs) {
  if (num_audio_streams_ == 0)
    return;

  EXPECT_NE(RtpExtension::kTOffset, extension_type_)
      << "Audio BWE not supported with toffset.";

  send_config->rtp.ssrc = audio_ssrcs_[0];
  send_config->rtp.extensions.clear();

  bool transport_cc = false;
  if (extension_type_ == RtpExtension::kAbsSendTime) {
    transport_cc = false;
    send_config->rtp.extensions.push_back(
        RtpExtension(extension_type_.c_str(), kAbsSendTimeExtensionId));
  } else if (extension_type_ == RtpExtension::kTransportSequenceNumber) {
    transport_cc = true;
    send_config->rtp.extensions.push_back(RtpExtension(
        extension_type_.c_str(), kTransportSequenceNumberExtensionId));
  }

  for (AudioReceiveStream::Config& recv_config : *receive_configs) {
    recv_config.combined_audio_video_bwe = true;
    recv_config.rtp.transport_cc = transport_cc;
    recv_config.rtp.extensions = send_config->rtp.extensions;
    recv_config.rtp.remote_ssrc = send_config->rtp.ssrc;
  }
}

void RampUpTester::OnCallsCreated(Call* sender_call, Call* receiver_call) {
  sender_call_ = sender_call;
}

bool RampUpTester::BitrateStatsPollingThread(void* obj) {
  return static_cast<RampUpTester*>(obj)->PollStats();
}

bool RampUpTester::PollStats() {
  if (sender_call_) {
    Call::Stats stats = sender_call_->GetStats();

    RTC_DCHECK_GT(expected_bitrate_bps_, 0);
    if (!start_bitrate_verified_ && start_bitrate_bps_ != 0) {
      // For tests with an explicitly set start bitrate, verify the first
      // bitrate estimate is close to the start bitrate and lower than the
      // test target bitrate. This is to verify a call respects the configured
      // start bitrate, but due to the BWE implementation we can't guarantee the
      // first estimate really is as high as the start bitrate.
      EXPECT_GT(stats.send_bandwidth_bps, 0.9 * start_bitrate_bps_);
      start_bitrate_verified_ = true;
    }
    if (stats.send_bandwidth_bps >= expected_bitrate_bps_) {
      ramp_up_finished_ms_ = clock_->TimeInMilliseconds();
      observation_complete_.Set();
    }
  }

  return !event_.Wait(kPollIntervalMs);
}

void RampUpTester::ReportResult(const std::string& measurement,
                                size_t value,
                                const std::string& units) const {
  webrtc::test::PrintResult(
      measurement, "",
      ::testing::UnitTest::GetInstance()->current_test_info()->name(), value,
      units, false);
}

void RampUpTester::AccumulateStats(const VideoSendStream::StreamStats& stream,
                                   size_t* total_packets_sent,
                                   size_t* total_sent,
                                   size_t* padding_sent,
                                   size_t* media_sent) const {
  *total_packets_sent += stream.rtp_stats.transmitted.packets +
                         stream.rtp_stats.retransmitted.packets +
                         stream.rtp_stats.fec.packets;
  *total_sent += stream.rtp_stats.transmitted.TotalBytes() +
                 stream.rtp_stats.retransmitted.TotalBytes() +
                 stream.rtp_stats.fec.TotalBytes();
  *padding_sent += stream.rtp_stats.transmitted.padding_bytes +
                   stream.rtp_stats.retransmitted.padding_bytes +
                   stream.rtp_stats.fec.padding_bytes;
  *media_sent += stream.rtp_stats.MediaPayloadBytes();
}

void RampUpTester::TriggerTestDone() {
  RTC_DCHECK_GE(test_start_ms_, 0);

  // TODO(holmer): Add audio send stats here too when those APIs are available.
  VideoSendStream::Stats send_stats = send_stream_->GetStats();

  size_t total_packets_sent = 0;
  size_t total_sent = 0;
  size_t padding_sent = 0;
  size_t media_sent = 0;
  for (uint32_t ssrc : video_ssrcs_) {
    AccumulateStats(send_stats.substreams[ssrc], &total_packets_sent,
                    &total_sent, &padding_sent, &media_sent);
  }

  size_t rtx_total_packets_sent = 0;
  size_t rtx_total_sent = 0;
  size_t rtx_padding_sent = 0;
  size_t rtx_media_sent = 0;
  for (uint32_t rtx_ssrc : video_rtx_ssrcs_) {
    AccumulateStats(send_stats.substreams[rtx_ssrc], &rtx_total_packets_sent,
                    &rtx_total_sent, &rtx_padding_sent, &rtx_media_sent);
  }

  ReportResult("ramp-up-total-packets-sent", total_packets_sent, "packets");
  ReportResult("ramp-up-total-sent", total_sent, "bytes");
  ReportResult("ramp-up-media-sent", media_sent, "bytes");
  ReportResult("ramp-up-padding-sent", padding_sent, "bytes");
  ReportResult("ramp-up-rtx-total-packets-sent", rtx_total_packets_sent,
               "packets");
  ReportResult("ramp-up-rtx-total-sent", rtx_total_sent, "bytes");
  ReportResult("ramp-up-rtx-media-sent", rtx_media_sent, "bytes");
  ReportResult("ramp-up-rtx-padding-sent", rtx_padding_sent, "bytes");
  if (ramp_up_finished_ms_ >= 0) {
    ReportResult("ramp-up-time", ramp_up_finished_ms_ - test_start_ms_,
                 "milliseconds");
  }
  ReportResult("ramp-up-average-network-latency",
               send_transport_->GetAverageDelayMs(), "milliseconds");
}

void RampUpTester::PerformTest() {
  test_start_ms_ = clock_->TimeInMilliseconds();
  poller_thread_.Start();
  EXPECT_TRUE(Wait()) << "Timed out while waiting for ramp-up to complete.";
  TriggerTestDone();
  poller_thread_.Stop();
}

RampUpDownUpTester::RampUpDownUpTester(size_t num_video_streams,
                                       size_t num_audio_streams,
                                       unsigned int start_bitrate_bps,
                                       const std::string& extension_type,
                                       bool rtx,
                                       bool red)
    : RampUpTester(num_video_streams,
                   num_audio_streams,
                   start_bitrate_bps,
                   extension_type,
                   rtx,
                   red),
      test_state_(kFirstRampup),
      state_start_ms_(clock_->TimeInMilliseconds()),
      interval_start_ms_(clock_->TimeInMilliseconds()),
      sent_bytes_(0) {
  forward_transport_config_.link_capacity_kbps = kHighBandwidthLimitBps / 1000;
}

RampUpDownUpTester::~RampUpDownUpTester() {}

bool RampUpDownUpTester::PollStats() {
  if (send_stream_) {
    webrtc::VideoSendStream::Stats stats = send_stream_->GetStats();
    int transmit_bitrate_bps = 0;
    for (auto it : stats.substreams) {
      transmit_bitrate_bps += it.second.total_bitrate_bps;
    }

    EvolveTestState(transmit_bitrate_bps, stats.suspended);
  }

  return !event_.Wait(kPollIntervalMs);
}

Call::Config RampUpDownUpTester::GetReceiverCallConfig() {
  Call::Config config;
  config.bitrate_config.min_bitrate_bps = 10000;
  return config;
}

std::string RampUpDownUpTester::GetModifierString() const {
  std::string str("_");
  if (num_video_streams_ > 0) {
    std::ostringstream s;
    s << num_video_streams_;
    str += s.str();
    str += "stream";
    str += (num_video_streams_ > 1 ? "s" : "");
    str += "_";
  }
  if (num_audio_streams_ > 0) {
    std::ostringstream s;
    s << num_audio_streams_;
    str += s.str();
    str += "stream";
    str += (num_audio_streams_ > 1 ? "s" : "");
    str += "_";
  }
  str += (rtx_ ? "" : "no");
  str += "rtx";
  return str;
}

void RampUpDownUpTester::EvolveTestState(int bitrate_bps, bool suspended) {
  int64_t now = clock_->TimeInMilliseconds();
  switch (test_state_) {
    case kFirstRampup: {
      EXPECT_FALSE(suspended);
      if (bitrate_bps > kExpectedHighBitrateBps) {
        // The first ramp-up has reached the target bitrate. Change the
        // channel limit, and move to the next test state.
        forward_transport_config_.link_capacity_kbps =
            kLowBandwidthLimitBps / 1000;
        send_transport_->SetConfig(forward_transport_config_);
        test_state_ = kLowRate;
        webrtc::test::PrintResult("ramp_up_down_up", GetModifierString(),
                                  "first_rampup", now - state_start_ms_, "ms",
                                  false);
        state_start_ms_ = now;
        interval_start_ms_ = now;
        sent_bytes_ = 0;
      }
      break;
    }
    case kLowRate: {
      if (bitrate_bps < kExpectedLowBitrateBps && suspended) {
        // The ramp-down was successful. Change the channel limit back to a
        // high value, and move to the next test state.
        forward_transport_config_.link_capacity_kbps =
            kHighBandwidthLimitBps / 1000;
        send_transport_->SetConfig(forward_transport_config_);
        test_state_ = kSecondRampup;
        webrtc::test::PrintResult("ramp_up_down_up", GetModifierString(),
                                  "rampdown", now - state_start_ms_, "ms",
                                  false);
        state_start_ms_ = now;
        interval_start_ms_ = now;
        sent_bytes_ = 0;
      }
      break;
    }
    case kSecondRampup: {
      if (bitrate_bps > kExpectedHighBitrateBps && !suspended) {
        webrtc::test::PrintResult("ramp_up_down_up", GetModifierString(),
                                  "second_rampup", now - state_start_ms_, "ms",
                                  false);
        ReportResult("ramp-up-down-up-average-network-latency",
                     send_transport_->GetAverageDelayMs(), "milliseconds");
        observation_complete_.Set();
      }
      break;
    }
  }
}

class RampUpTest : public test::CallTest {
 public:
  RampUpTest() {}

  virtual ~RampUpTest() {
    EXPECT_EQ(nullptr, video_send_stream_);
    EXPECT_TRUE(video_receive_streams_.empty());
  }
};

TEST_F(RampUpTest, SingleStream) {
  RampUpTester test(1, 0, 0, RtpExtension::kTOffset, false, false);
  RunBaseTest(&test);
}

TEST_F(RampUpTest, Simulcast) {
  RampUpTester test(3, 0, 0, RtpExtension::kTOffset, false, false);
  RunBaseTest(&test);
}

TEST_F(RampUpTest, SimulcastWithRtx) {
  RampUpTester test(3, 0, 0, RtpExtension::kTOffset, true, false);
  RunBaseTest(&test);
}

TEST_F(RampUpTest, SimulcastByRedWithRtx) {
  RampUpTester test(3, 0, 0, RtpExtension::kTOffset, true, true);
  RunBaseTest(&test);
}

TEST_F(RampUpTest, SingleStreamWithHighStartBitrate) {
  RampUpTester test(1, 0, 0.9 * kSingleStreamTargetBps, RtpExtension::kTOffset,
                    false, false);
  RunBaseTest(&test);
}

// Disabled on Mac due to flakiness, see
// https://bugs.chromium.org/p/webrtc/issues/detail?id=5407
#ifndef WEBRTC_MAC

static const uint32_t kStartBitrateBps = 60000;

TEST_F(RampUpTest, UpDownUpOneStream) {
  RampUpDownUpTester test(1, 0, kStartBitrateBps, RtpExtension::kAbsSendTime,
                          false, false);
  RunBaseTest(&test);
}

TEST_F(RampUpTest, UpDownUpThreeStreams) {
  RampUpDownUpTester test(3, 0, kStartBitrateBps, RtpExtension::kAbsSendTime,
                          false, false);
  RunBaseTest(&test);
}

TEST_F(RampUpTest, UpDownUpOneStreamRtx) {
  RampUpDownUpTester test(1, 0, kStartBitrateBps, RtpExtension::kAbsSendTime,
                          true, false);
  RunBaseTest(&test);
}

TEST_F(RampUpTest, UpDownUpThreeStreamsRtx) {
  RampUpDownUpTester test(3, 0, kStartBitrateBps, RtpExtension::kAbsSendTime,
                          true, false);
  RunBaseTest(&test);
}

TEST_F(RampUpTest, UpDownUpOneStreamByRedRtx) {
  RampUpDownUpTester test(1, 0, kStartBitrateBps, RtpExtension::kAbsSendTime,
                          true, true);
  RunBaseTest(&test);
}

TEST_F(RampUpTest, UpDownUpThreeStreamsByRedRtx) {
  RampUpDownUpTester test(3, 0, kStartBitrateBps, RtpExtension::kAbsSendTime,
                          true, true);
  RunBaseTest(&test);
}

TEST_F(RampUpTest, SendSideVideoUpDownUpRtx) {
  RampUpDownUpTester test(3, 0, kStartBitrateBps,
                          RtpExtension::kTransportSequenceNumber, true, false);
  RunBaseTest(&test);
}

// TODO(holmer): Enable when audio bitrates are included in the bitrate
//               allocation.
TEST_F(RampUpTest, DISABLED_SendSideAudioVideoUpDownUpRtx) {
  RampUpDownUpTester test(3, 1, kStartBitrateBps,
                          RtpExtension::kTransportSequenceNumber, true, false);
  RunBaseTest(&test);
}

#endif

TEST_F(RampUpTest, AbsSendTimeSingleStream) {
  RampUpTester test(1, 0, 0, RtpExtension::kAbsSendTime, false, false);
  RunBaseTest(&test);
}

TEST_F(RampUpTest, AbsSendTimeSimulcast) {
  RampUpTester test(3, 0, 0, RtpExtension::kAbsSendTime, false, false);
  RunBaseTest(&test);
}

TEST_F(RampUpTest, AbsSendTimeSimulcastWithRtx) {
  RampUpTester test(3, 0, 0, RtpExtension::kAbsSendTime, true, false);
  RunBaseTest(&test);
}

TEST_F(RampUpTest, AbsSendTimeSimulcastByRedWithRtx) {
  RampUpTester test(3, 0, 0, RtpExtension::kAbsSendTime, true, true);
  RunBaseTest(&test);
}

TEST_F(RampUpTest, AbsSendTimeSingleStreamWithHighStartBitrate) {
  RampUpTester test(1, 0, 0.9 * kSingleStreamTargetBps,
                    RtpExtension::kAbsSendTime, false, false);
  RunBaseTest(&test);
}

TEST_F(RampUpTest, TransportSequenceNumberSingleStream) {
  RampUpTester test(1, 0, 0, RtpExtension::kTransportSequenceNumber, false,
                    false);
  RunBaseTest(&test);
}

TEST_F(RampUpTest, TransportSequenceNumberSimulcast) {
  RampUpTester test(3, 0, 0, RtpExtension::kTransportSequenceNumber, false,
                    false);
  RunBaseTest(&test);
}

TEST_F(RampUpTest, TransportSequenceNumberSimulcastWithRtx) {
  RampUpTester test(3, 0, 0, RtpExtension::kTransportSequenceNumber, true,
                    false);
  RunBaseTest(&test);
}

TEST_F(RampUpTest, AudioVideoTransportSequenceNumberSimulcastWithRtx) {
  RampUpTester test(3, 1, 0, RtpExtension::kTransportSequenceNumber, true,
                    false);
  RunBaseTest(&test);
}

TEST_F(RampUpTest, TransportSequenceNumberSimulcastByRedWithRtx) {
  RampUpTester test(3, 0, 0, RtpExtension::kTransportSequenceNumber, true,
                    true);
  RunBaseTest(&test);
}

TEST_F(RampUpTest, TransportSequenceNumberSingleStreamWithHighStartBitrate) {
  RampUpTester test(1, 0, 0.9 * kSingleStreamTargetBps,
                    RtpExtension::kTransportSequenceNumber, false, false);
  RunBaseTest(&test);
}
}  // namespace webrtc
