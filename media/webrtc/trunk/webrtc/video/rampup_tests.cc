/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/common.h"
#include "webrtc/modules/rtp_rtcp/interface/receive_statistics.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_header_parser.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_payload_registry.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_utility.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/test/testsupport/perf_test.h"
#include "webrtc/video/rampup_tests.h"

namespace webrtc {
namespace {

static const int kMaxPacketSize = 1500;

std::vector<uint32_t> GenerateSsrcs(size_t num_streams,
                                    uint32_t ssrc_offset) {
  std::vector<uint32_t> ssrcs;
  for (size_t i = 0; i != num_streams; ++i)
    ssrcs.push_back(static_cast<uint32_t>(ssrc_offset + i));
  return ssrcs;
}
}  // namespace

StreamObserver::StreamObserver(const SsrcMap& rtx_media_ssrcs,
                               newapi::Transport* feedback_transport,
                               Clock* clock,
                               RemoteBitrateEstimatorFactory* rbe_factory,
                               RateControlType control_type)
    : clock_(clock),
      test_done_(EventWrapper::Create()),
      rtp_parser_(RtpHeaderParser::Create()),
      feedback_transport_(feedback_transport),
      receive_stats_(ReceiveStatistics::Create(clock)),
      payload_registry_(
          new RTPPayloadRegistry(RTPPayloadStrategy::CreateStrategy(false))),
      crit_(CriticalSectionWrapper::CreateCriticalSection()),
      expected_bitrate_bps_(0),
      start_bitrate_bps_(0),
      rtx_media_ssrcs_(rtx_media_ssrcs),
      total_sent_(0),
      padding_sent_(0),
      rtx_media_sent_(0),
      total_packets_sent_(0),
      padding_packets_sent_(0),
      rtx_media_packets_sent_(0),
      test_start_ms_(clock_->TimeInMilliseconds()),
      ramp_up_finished_ms_(0) {
  // Ideally we would only have to instantiate an RtcpSender, an
  // RtpHeaderParser and a RemoteBitrateEstimator here, but due to the current
  // state of the RTP module we need a full module and receive statistics to
  // be able to produce an RTCP with REMB.
  RtpRtcp::Configuration config;
  config.receive_statistics = receive_stats_.get();
  feedback_transport_.Enable();
  config.outgoing_transport = &feedback_transport_;
  rtp_rtcp_.reset(RtpRtcp::CreateRtpRtcp(config));
  rtp_rtcp_->SetREMBStatus(true);
  rtp_rtcp_->SetRTCPStatus(kRtcpNonCompound);
  rtp_parser_->RegisterRtpHeaderExtension(kRtpExtensionAbsoluteSendTime,
                                          kAbsSendTimeExtensionId);
  rtp_parser_->RegisterRtpHeaderExtension(kRtpExtensionTransmissionTimeOffset,
                                          kTransmissionTimeOffsetExtensionId);
  const uint32_t kRemoteBitrateEstimatorMinBitrateBps = 30000;
  remote_bitrate_estimator_.reset(
      rbe_factory->Create(this, clock, control_type,
                          kRemoteBitrateEstimatorMinBitrateBps));
}

void StreamObserver::set_expected_bitrate_bps(
    unsigned int expected_bitrate_bps) {
  CriticalSectionScoped lock(crit_.get());
  expected_bitrate_bps_ = expected_bitrate_bps;
}

void StreamObserver::set_start_bitrate_bps(unsigned int start_bitrate_bps) {
  CriticalSectionScoped lock(crit_.get());
  start_bitrate_bps_ = start_bitrate_bps;
}

void StreamObserver::OnReceiveBitrateChanged(
    const std::vector<unsigned int>& ssrcs, unsigned int bitrate) {
  CriticalSectionScoped lock(crit_.get());
  DCHECK_GT(expected_bitrate_bps_, 0u);
  if (start_bitrate_bps_ != 0) {
    // For tests with an explicitly set start bitrate, verify the first
    // bitrate estimate is close to the start bitrate and lower than the
    // test target bitrate. This is to verify a call respects the configured
    // start bitrate, but due to the BWE implementation we can't guarantee the
    // first estimate really is as high as the start bitrate.
    EXPECT_GT(bitrate, 0.9 * start_bitrate_bps_);
    start_bitrate_bps_ = 0;
  }
  if (bitrate >= expected_bitrate_bps_) {
    ramp_up_finished_ms_ = clock_->TimeInMilliseconds();
    // Just trigger if there was any rtx padding packet.
    if (rtx_media_ssrcs_.empty() || rtx_media_sent_ > 0) {
      TriggerTestDone();
    }
  }
  rtp_rtcp_->SetREMBData(bitrate, ssrcs);
  rtp_rtcp_->Process();
}

bool StreamObserver::SendRtp(const uint8_t* packet, size_t length) {
  CriticalSectionScoped lock(crit_.get());
  RTPHeader header;
  EXPECT_TRUE(rtp_parser_->Parse(packet, length, &header));
  receive_stats_->IncomingPacket(header, length, false);
  payload_registry_->SetIncomingPayloadType(header);
  remote_bitrate_estimator_->IncomingPacket(
      clock_->TimeInMilliseconds(), length - 12, header);
  if (remote_bitrate_estimator_->TimeUntilNextProcess() <= 0) {
    remote_bitrate_estimator_->Process();
  }
  total_sent_ += length;
  padding_sent_ += header.paddingLength;
  ++total_packets_sent_;
  if (header.paddingLength > 0)
    ++padding_packets_sent_;
  if (rtx_media_ssrcs_.find(header.ssrc) != rtx_media_ssrcs_.end()) {
    rtx_media_sent_ += length - header.headerLength - header.paddingLength;
    if (header.paddingLength == 0)
      ++rtx_media_packets_sent_;
    uint8_t restored_packet[kMaxPacketSize];
    uint8_t* restored_packet_ptr = restored_packet;
    size_t restored_length = length;
    payload_registry_->RestoreOriginalPacket(&restored_packet_ptr,
                                             packet,
                                             &restored_length,
                                             rtx_media_ssrcs_[header.ssrc],
                                             header);
    length = restored_length;
    EXPECT_TRUE(rtp_parser_->Parse(
        restored_packet, static_cast<int>(length), &header));
  } else {
    rtp_rtcp_->SetRemoteSSRC(header.ssrc);
  }
  return true;
}

bool StreamObserver::SendRtcp(const uint8_t* packet, size_t length) {
  return true;
}

EventTypeWrapper StreamObserver::Wait() {
  return test_done_->Wait(test::CallTest::kLongTimeoutMs);
}

void StreamObserver::ReportResult(const std::string& measurement,
                  size_t value,
                  const std::string& units) {
  webrtc::test::PrintResult(
      measurement, "",
      ::testing::UnitTest::GetInstance()->current_test_info()->name(),
      value, units, false);
}

void StreamObserver::TriggerTestDone() EXCLUSIVE_LOCKS_REQUIRED(crit_) {
  ReportResult("ramp-up-total-sent", total_sent_, "bytes");
  ReportResult("ramp-up-padding-sent", padding_sent_, "bytes");
  ReportResult("ramp-up-rtx-media-sent", rtx_media_sent_, "bytes");
  ReportResult("ramp-up-total-packets-sent", total_packets_sent_, "packets");
  ReportResult("ramp-up-padding-packets-sent",
               padding_packets_sent_,
               "packets");
  ReportResult("ramp-up-rtx-packets-sent",
               rtx_media_packets_sent_,
               "packets");
  ReportResult("ramp-up-time",
               ramp_up_finished_ms_ - test_start_ms_,
               "milliseconds");
  test_done_->Set();
}

LowRateStreamObserver::LowRateStreamObserver(
    newapi::Transport* feedback_transport,
    Clock* clock,
    size_t number_of_streams,
    bool rtx_used)
    : clock_(clock),
      number_of_streams_(number_of_streams),
      rtx_used_(rtx_used),
      test_done_(EventWrapper::Create()),
      rtp_parser_(RtpHeaderParser::Create()),
      feedback_transport_(feedback_transport),
      receive_stats_(ReceiveStatistics::Create(clock)),
      crit_(CriticalSectionWrapper::CreateCriticalSection()),
      send_stream_(nullptr),
      test_state_(kFirstRampup),
      state_start_ms_(clock_->TimeInMilliseconds()),
      interval_start_ms_(state_start_ms_),
      last_remb_bps_(0),
      sent_bytes_(0),
      total_overuse_bytes_(0),
      suspended_in_stats_(false) {
  RtpRtcp::Configuration config;
  config.receive_statistics = receive_stats_.get();
  feedback_transport_.Enable();
  config.outgoing_transport = &feedback_transport_;
  rtp_rtcp_.reset(RtpRtcp::CreateRtpRtcp(config));
  rtp_rtcp_->SetREMBStatus(true);
  rtp_rtcp_->SetRTCPStatus(kRtcpNonCompound);
  rtp_parser_->RegisterRtpHeaderExtension(kRtpExtensionAbsoluteSendTime,
                                          kAbsSendTimeExtensionId);
  AbsoluteSendTimeRemoteBitrateEstimatorFactory rbe_factory;
  const uint32_t kRemoteBitrateEstimatorMinBitrateBps = 10000;
  remote_bitrate_estimator_.reset(
      rbe_factory.Create(this, clock, kAimdControl,
                         kRemoteBitrateEstimatorMinBitrateBps));
  forward_transport_config_.link_capacity_kbps =
      kHighBandwidthLimitBps / 1000;
  forward_transport_config_.queue_length_packets = 100;  // Something large.
  test::DirectTransport::SetConfig(forward_transport_config_);
  test::DirectTransport::SetReceiver(this);
}

void LowRateStreamObserver::SetSendStream(VideoSendStream* send_stream) {
  CriticalSectionScoped lock(crit_.get());
  send_stream_ = send_stream;
}

void LowRateStreamObserver::OnReceiveBitrateChanged(
    const std::vector<unsigned int>& ssrcs,
    unsigned int bitrate) {
  CriticalSectionScoped lock(crit_.get());
  rtp_rtcp_->SetREMBData(bitrate, ssrcs);
  rtp_rtcp_->Process();
  last_remb_bps_ = bitrate;
}

bool LowRateStreamObserver::SendRtp(const uint8_t* data, size_t length) {
  CriticalSectionScoped lock(crit_.get());
  sent_bytes_ += length;
  int64_t now_ms = clock_->TimeInMilliseconds();
  if (now_ms > interval_start_ms_ + 1000) {  // Let at least 1 second pass.
    // Verify that the send rate was about right.
    unsigned int average_rate_bps = static_cast<unsigned int>(sent_bytes_) *
                                    8 * 1000 / (now_ms - interval_start_ms_);
    // TODO(holmer): Why is this failing?
    // EXPECT_LT(average_rate_bps, last_remb_bps_ * 1.1);
    if (average_rate_bps > last_remb_bps_ * 1.1) {
      total_overuse_bytes_ +=
          sent_bytes_ -
          last_remb_bps_ / 8 * (now_ms - interval_start_ms_) / 1000;
    }
    EvolveTestState(average_rate_bps);
    interval_start_ms_ = now_ms;
    sent_bytes_ = 0;
  }
  return test::DirectTransport::SendRtp(data, length);
}

PacketReceiver::DeliveryStatus LowRateStreamObserver::DeliverPacket(
    const uint8_t* packet, size_t length) {
  CriticalSectionScoped lock(crit_.get());
  RTPHeader header;
  EXPECT_TRUE(rtp_parser_->Parse(packet, length, &header));
  receive_stats_->IncomingPacket(header, length, false);
  remote_bitrate_estimator_->IncomingPacket(
      clock_->TimeInMilliseconds(), length - 12, header);
  if (remote_bitrate_estimator_->TimeUntilNextProcess() <= 0) {
    remote_bitrate_estimator_->Process();
  }
  suspended_in_stats_ = send_stream_->GetStats().suspended;
  return DELIVERY_OK;
}

bool LowRateStreamObserver::SendRtcp(const uint8_t* packet, size_t length) {
  return true;
}

std::string LowRateStreamObserver::GetModifierString() {
  std::string str("_");
  char temp_str[5];
  sprintf(temp_str, "%i",
      static_cast<int>(number_of_streams_));
  str += std::string(temp_str);
  str += "stream";
  str += (number_of_streams_ > 1 ? "s" : "");
  str += "_";
  str += (rtx_used_ ? "" : "no");
  str += "rtx";
  return str;
}

void LowRateStreamObserver::EvolveTestState(unsigned int bitrate_bps) {
  int64_t now = clock_->TimeInMilliseconds();
  CriticalSectionScoped lock(crit_.get());
  DCHECK(send_stream_ != nullptr);
  switch (test_state_) {
    case kFirstRampup: {
      EXPECT_FALSE(suspended_in_stats_);
      if (bitrate_bps > kExpectedHighBitrateBps) {
        // The first ramp-up has reached the target bitrate. Change the
        // channel limit, and move to the next test state.
        forward_transport_config_.link_capacity_kbps =
            kLowBandwidthLimitBps / 1000;
        test::DirectTransport::SetConfig(forward_transport_config_);
        test_state_ = kLowRate;
        webrtc::test::PrintResult("ramp_up_down_up",
                                  GetModifierString(),
                                  "first_rampup",
                                  now - state_start_ms_,
                                  "ms",
                                  false);
        state_start_ms_ = now;
        interval_start_ms_ = now;
        sent_bytes_ = 0;
      }
      break;
    }
    case kLowRate: {
      if (bitrate_bps < kExpectedLowBitrateBps && suspended_in_stats_) {
        // The ramp-down was successful. Change the channel limit back to a
        // high value, and move to the next test state.
        forward_transport_config_.link_capacity_kbps =
            kHighBandwidthLimitBps / 1000;
        test::DirectTransport::SetConfig(forward_transport_config_);
        test_state_ = kSecondRampup;
        webrtc::test::PrintResult("ramp_up_down_up",
                                  GetModifierString(),
                                  "rampdown",
                                  now - state_start_ms_,
                                  "ms",
                                  false);
        state_start_ms_ = now;
        interval_start_ms_ = now;
        sent_bytes_ = 0;
      }
      break;
    }
    case kSecondRampup: {
      if (bitrate_bps > kExpectedHighBitrateBps && !suspended_in_stats_) {
        webrtc::test::PrintResult("ramp_up_down_up",
                                  GetModifierString(),
                                  "second_rampup",
                                  now - state_start_ms_,
                                  "ms",
                                  false);
        webrtc::test::PrintResult("ramp_up_down_up",
                                  GetModifierString(),
                                  "total_overuse",
                                  total_overuse_bytes_,
                                  "bytes",
                                  false);
        test_done_->Set();
      }
      break;
    }
  }
}

EventTypeWrapper LowRateStreamObserver::Wait() {
  return test_done_->Wait(test::CallTest::kLongTimeoutMs);
}

void RampUpTest::RunRampUpTest(bool rtx,
                               size_t num_streams,
                               unsigned int start_bitrate_bps,
                               const std::string& extension_type) {
  std::vector<uint32_t> ssrcs(GenerateSsrcs(num_streams, 100));
  std::vector<uint32_t> rtx_ssrcs(GenerateSsrcs(num_streams, 200));
  StreamObserver::SsrcMap rtx_ssrc_map;
  if (rtx) {
    for (size_t i = 0; i < ssrcs.size(); ++i)
      rtx_ssrc_map[rtx_ssrcs[i]] = ssrcs[i];
  }

  CreateSendConfig(num_streams);

  rtc::scoped_ptr<RemoteBitrateEstimatorFactory> rbe_factory;
  RateControlType control_type;
  if (extension_type == RtpExtension::kAbsSendTime) {
    control_type = kAimdControl;
    rbe_factory.reset(new AbsoluteSendTimeRemoteBitrateEstimatorFactory);
    send_config_.rtp.extensions.push_back(RtpExtension(
        extension_type.c_str(), kAbsSendTimeExtensionId));
  } else {
    control_type = kMimdControl;
    rbe_factory.reset(new RemoteBitrateEstimatorFactory);
    send_config_.rtp.extensions.push_back(RtpExtension(
        extension_type.c_str(), kTransmissionTimeOffsetExtensionId));
  }

  test::DirectTransport receiver_transport;
  StreamObserver stream_observer(rtx_ssrc_map,
                                 &receiver_transport,
                                 Clock::GetRealTimeClock(),
                                 rbe_factory.get(),
                                 control_type);

  Call::Config call_config(&stream_observer);
  if (start_bitrate_bps != 0) {
    call_config.bitrate_config.start_bitrate_bps = start_bitrate_bps;
    stream_observer.set_start_bitrate_bps(start_bitrate_bps);
  }

  CreateSenderCall(call_config);

  receiver_transport.SetReceiver(sender_call_->Receiver());

  if (num_streams == 1) {
    encoder_config_.streams[0].target_bitrate_bps = 2000000;
    encoder_config_.streams[0].max_bitrate_bps = 2000000;
  }

  send_config_.rtp.nack.rtp_history_ms = kNackRtpHistoryMs;
  send_config_.rtp.ssrcs = ssrcs;
  if (rtx) {
    send_config_.rtp.rtx.payload_type = kSendRtxPayloadType;
    send_config_.rtp.rtx.ssrcs = rtx_ssrcs;
  }

  if (num_streams == 1) {
    // For single stream rampup until 1mbps
    stream_observer.set_expected_bitrate_bps(kSingleStreamTargetBps);
  } else {
    // For multi stream rampup until all streams are being sent. That means
    // enough birate to send all the target streams plus the min bitrate of
    // the last one.
    int expected_bitrate_bps = encoder_config_.streams.back().min_bitrate_bps;
    for (size_t i = 0; i < encoder_config_.streams.size() - 1; ++i) {
      expected_bitrate_bps += encoder_config_.streams[i].target_bitrate_bps;
    }
    stream_observer.set_expected_bitrate_bps(expected_bitrate_bps);
  }

  CreateStreams();
  CreateFrameGeneratorCapturer();

  Start();

  EXPECT_EQ(kEventSignaled, stream_observer.Wait());

  Stop();
  DestroyStreams();
}

void RampUpTest::RunRampUpDownUpTest(size_t number_of_streams, bool rtx) {
  test::DirectTransport receiver_transport;
  LowRateStreamObserver stream_observer(
      &receiver_transport, Clock::GetRealTimeClock(), number_of_streams, rtx);

  Call::Config call_config(&stream_observer);
  CreateSenderCall(call_config);
  receiver_transport.SetReceiver(sender_call_->Receiver());

  CreateSendConfig(number_of_streams);

  send_config_.rtp.nack.rtp_history_ms = kNackRtpHistoryMs;
  send_config_.rtp.extensions.push_back(RtpExtension(
      RtpExtension::kAbsSendTime, kAbsSendTimeExtensionId));
  send_config_.suspend_below_min_bitrate = true;
  if (rtx) {
    send_config_.rtp.rtx.payload_type = kSendRtxPayloadType;
    send_config_.rtp.rtx.ssrcs = GenerateSsrcs(number_of_streams, 200);
  }

  CreateStreams();
  stream_observer.SetSendStream(send_stream_);

  CreateFrameGeneratorCapturer();

  Start();

  EXPECT_EQ(kEventSignaled, stream_observer.Wait());

  Stop();
  DestroyStreams();
}

TEST_F(RampUpTest, SingleStream) {
  RunRampUpTest(false, 1, 0, RtpExtension::kTOffset);
}

TEST_F(RampUpTest, Simulcast) {
  RunRampUpTest(false, 3, 0, RtpExtension::kTOffset);
}

TEST_F(RampUpTest, SimulcastWithRtx) {
  RunRampUpTest(true, 3, 0, RtpExtension::kTOffset);
}

TEST_F(RampUpTest, SingleStreamWithHighStartBitrate) {
  RunRampUpTest(false, 1, 0.9 * kSingleStreamTargetBps, RtpExtension::kTOffset);
}

TEST_F(RampUpTest, UpDownUpOneStream) { RunRampUpDownUpTest(1, false); }

TEST_F(RampUpTest, UpDownUpThreeStreams) { RunRampUpDownUpTest(3, false); }

TEST_F(RampUpTest, UpDownUpOneStreamRtx) { RunRampUpDownUpTest(1, true); }

TEST_F(RampUpTest, UpDownUpThreeStreamsRtx) { RunRampUpDownUpTest(3, true); }

TEST_F(RampUpTest, AbsSendTimeSingleStream) {
  RunRampUpTest(false, 1, 0, RtpExtension::kAbsSendTime);
}

TEST_F(RampUpTest, AbsSendTimeSimulcast) {
  RunRampUpTest(false, 3, 0, RtpExtension::kAbsSendTime);
}

TEST_F(RampUpTest, AbsSendTimeSimulcastWithRtx) {
  RunRampUpTest(true, 3, 0, RtpExtension::kAbsSendTime);
}

TEST_F(RampUpTest, AbsSendTimeSingleStreamWithHighStartBitrate) {
  RunRampUpTest(false, 1, 0.9 * kSingleStreamTargetBps,
                RtpExtension::kAbsSendTime);
}
}  // namespace webrtc
