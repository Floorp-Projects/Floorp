/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <limits>
#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/base/checks.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/remote_bitrate_estimator/include/mock/mock_remote_bitrate_estimator.h"
#include "webrtc/modules/remote_bitrate_estimator/transport_feedback_adapter.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "webrtc/modules/utility/include/mock/mock_process_thread.h"
#include "webrtc/system_wrappers/include/clock.h"

using ::testing::_;
using ::testing::Invoke;

namespace webrtc {
namespace test {

class TransportFeedbackAdapterTest : public ::testing::Test {
 public:
  TransportFeedbackAdapterTest()
      : clock_(0),
        bitrate_estimator_(nullptr),
        receiver_estimated_bitrate_(0) {}

  virtual ~TransportFeedbackAdapterTest() {}

  virtual void SetUp() {
    adapter_.reset(new TransportFeedbackAdapter(
        new RtcpBandwidthObserverAdapter(this), &clock_, &process_thread_));

    bitrate_estimator_ = new MockRemoteBitrateEstimator();
    EXPECT_CALL(process_thread_, RegisterModule(bitrate_estimator_)).Times(1);
    adapter_->SetBitrateEstimator(bitrate_estimator_);
  }

  virtual void TearDown() {
    EXPECT_CALL(process_thread_, DeRegisterModule(bitrate_estimator_)).Times(1);
    adapter_.reset();
  }

 protected:
  // Proxy class used since TransportFeedbackAdapter will own the instance
  // passed at construction.
  class RtcpBandwidthObserverAdapter : public RtcpBandwidthObserver {
   public:
    explicit RtcpBandwidthObserverAdapter(TransportFeedbackAdapterTest* owner)
        : owner_(owner) {}

    void OnReceivedEstimatedBitrate(uint32_t bitrate) override {
      owner_->receiver_estimated_bitrate_ = bitrate;
    }

    void OnReceivedRtcpReceiverReport(const ReportBlockList& report_blocks,
                                      int64_t rtt,
                                      int64_t now_ms) override {
      RTC_NOTREACHED();
    }

    TransportFeedbackAdapterTest* const owner_;
  };

  void OnReceivedEstimatedBitrate(uint32_t bitrate) {}

  void OnReceivedRtcpReceiverReport(const ReportBlockList& report_blocks,
                                    int64_t rtt,
                                    int64_t now_ms) {}

  void ComparePacketVectors(const std::vector<PacketInfo>& truth,
                            const std::vector<PacketInfo>& input) {
    ASSERT_EQ(truth.size(), input.size());
    size_t len = truth.size();
    // truth contains the input data for the test, and input is what will be
    // sent to the bandwidth estimator. truth.arrival_tims_ms is used to
    // populate the transport feedback messages. As these times may be changed
    // (because of resolution limits in the packets, and because of the time
    // base adjustment performed by the TransportFeedbackAdapter at the first
    // packet, the truth[x].arrival_time and input[x].arrival_time may not be
    // equal. However, the difference must be the same for all x.
    int64_t arrival_time_delta =
        truth[0].arrival_time_ms - input[0].arrival_time_ms;
    for (size_t i = 0; i < len; ++i) {
      EXPECT_EQ(truth[i].arrival_time_ms,
                input[i].arrival_time_ms + arrival_time_delta);
      EXPECT_EQ(truth[i].send_time_ms, input[i].send_time_ms);
      EXPECT_EQ(truth[i].sequence_number, input[i].sequence_number);
      EXPECT_EQ(truth[i].payload_size, input[i].payload_size);
      EXPECT_EQ(truth[i].was_paced, input[i].was_paced);
    }
  }

  // Utility method, to reset arrival_time_ms before adding send time.
  void OnSentPacket(PacketInfo info) {
    info.arrival_time_ms = 0;
    adapter_->AddPacket(info.sequence_number, info.payload_size,
                        info.was_paced);
    adapter_->OnSentPacket(info.sequence_number, info.send_time_ms);
  }

  SimulatedClock clock_;
  MockProcessThread process_thread_;
  MockRemoteBitrateEstimator* bitrate_estimator_;
  rtc::scoped_ptr<TransportFeedbackAdapter> adapter_;

  uint32_t receiver_estimated_bitrate_;
};

TEST_F(TransportFeedbackAdapterTest, AdaptsFeedbackAndPopulatesSendTimes) {
  std::vector<PacketInfo> packets;
  packets.push_back(PacketInfo(100, 200, 0, 1500, true));
  packets.push_back(PacketInfo(110, 210, 1, 1500, true));
  packets.push_back(PacketInfo(120, 220, 2, 1500, true));
  packets.push_back(PacketInfo(130, 230, 3, 1500, true));
  packets.push_back(PacketInfo(140, 240, 4, 1500, true));

  for (const PacketInfo& packet : packets)
    OnSentPacket(packet);

  rtcp::TransportFeedback feedback;
  feedback.WithBase(packets[0].sequence_number,
                    packets[0].arrival_time_ms * 1000);

  for (const PacketInfo& packet : packets) {
    EXPECT_TRUE(feedback.WithReceivedPacket(packet.sequence_number,
                                            packet.arrival_time_ms * 1000));
  }

  feedback.Build();

  EXPECT_CALL(*bitrate_estimator_, IncomingPacketFeedbackVector(_))
      .Times(1)
      .WillOnce(Invoke(
          [packets, this](const std::vector<PacketInfo>& feedback_vector) {
            ComparePacketVectors(packets, feedback_vector);
          }));
  adapter_->OnTransportFeedback(feedback);
}

TEST_F(TransportFeedbackAdapterTest, HandlesDroppedPackets) {
  std::vector<PacketInfo> packets;
  packets.push_back(PacketInfo(100, 200, 0, 1500, true));
  packets.push_back(PacketInfo(110, 210, 1, 1500, true));
  packets.push_back(PacketInfo(120, 220, 2, 1500, true));
  packets.push_back(PacketInfo(130, 230, 3, 1500, true));
  packets.push_back(PacketInfo(140, 240, 4, 1500, true));

  const uint16_t kSendSideDropBefore = 1;
  const uint16_t kReceiveSideDropAfter = 3;

  for (const PacketInfo& packet : packets) {
    if (packet.sequence_number >= kSendSideDropBefore)
      OnSentPacket(packet);
  }

  rtcp::TransportFeedback feedback;
  feedback.WithBase(packets[0].sequence_number,
                    packets[0].arrival_time_ms * 1000);

  for (const PacketInfo& packet : packets) {
    if (packet.sequence_number <= kReceiveSideDropAfter) {
      EXPECT_TRUE(feedback.WithReceivedPacket(packet.sequence_number,
                                              packet.arrival_time_ms * 1000));
    }
  }

  feedback.Build();

  std::vector<PacketInfo> expected_packets(
      packets.begin() + kSendSideDropBefore,
      packets.begin() + kReceiveSideDropAfter + 1);

  EXPECT_CALL(*bitrate_estimator_, IncomingPacketFeedbackVector(_))
      .Times(1)
      .WillOnce(Invoke([expected_packets,
                        this](const std::vector<PacketInfo>& feedback_vector) {
        ComparePacketVectors(expected_packets, feedback_vector);
      }));
  adapter_->OnTransportFeedback(feedback);
}

TEST_F(TransportFeedbackAdapterTest, SendTimeWrapsBothWays) {
  int64_t kHighArrivalTimeMs = rtcp::TransportFeedback::kDeltaScaleFactor *
                               static_cast<int64_t>(1 << 8) *
                               static_cast<int64_t>((1 << 23) - 1) / 1000;
  std::vector<PacketInfo> packets;
  packets.push_back(PacketInfo(kHighArrivalTimeMs - 64, 200, 0, 1500, true));
  packets.push_back(PacketInfo(kHighArrivalTimeMs + 64, 210, 1, 1500, true));
  packets.push_back(PacketInfo(kHighArrivalTimeMs, 220, 2, 1500, true));

  for (const PacketInfo& packet : packets)
    OnSentPacket(packet);

  for (size_t i = 0; i < packets.size(); ++i) {
    rtc::scoped_ptr<rtcp::TransportFeedback> feedback(
        new rtcp::TransportFeedback());
    feedback->WithBase(packets[i].sequence_number,
                       packets[i].arrival_time_ms * 1000);

    EXPECT_TRUE(feedback->WithReceivedPacket(
        packets[i].sequence_number, packets[i].arrival_time_ms * 1000));

    rtc::scoped_ptr<rtcp::RawPacket> raw_packet = feedback->Build();
    feedback = rtcp::TransportFeedback::ParseFrom(raw_packet->Buffer(),
                                                  raw_packet->Length());

    std::vector<PacketInfo> expected_packets;
    expected_packets.push_back(packets[i]);

    EXPECT_CALL(*bitrate_estimator_, IncomingPacketFeedbackVector(_))
        .Times(1)
        .WillOnce(Invoke([expected_packets, this](
            const std::vector<PacketInfo>& feedback_vector) {
          ComparePacketVectors(expected_packets, feedback_vector);
        }));
    adapter_->OnTransportFeedback(*feedback.get());
  }
}

TEST_F(TransportFeedbackAdapterTest, TimestampDeltas) {
  std::vector<PacketInfo> sent_packets;
  const int64_t kSmallDeltaUs =
      rtcp::TransportFeedback::kDeltaScaleFactor * ((1 << 8) - 1);
  const int64_t kLargePositiveDeltaUs =
      rtcp::TransportFeedback::kDeltaScaleFactor *
      std::numeric_limits<int16_t>::max();
  const int64_t kLargeNegativeDeltaUs =
      rtcp::TransportFeedback::kDeltaScaleFactor *
      std::numeric_limits<int16_t>::min();

  PacketInfo info(100, 200, 0, 1500, true);
  sent_packets.push_back(info);

  info.send_time_ms += kSmallDeltaUs / 1000;
  info.arrival_time_ms += kSmallDeltaUs / 1000;
  ++info.sequence_number;
  sent_packets.push_back(info);

  info.send_time_ms += kLargePositiveDeltaUs / 1000;
  info.arrival_time_ms += kLargePositiveDeltaUs / 1000;
  ++info.sequence_number;
  sent_packets.push_back(info);

  info.send_time_ms += kLargeNegativeDeltaUs / 1000;
  info.arrival_time_ms += kLargeNegativeDeltaUs / 1000;
  ++info.sequence_number;
  sent_packets.push_back(info);

  // Too large, delta - will need two feedback messages.
  info.send_time_ms += (kLargePositiveDeltaUs + 1000) / 1000;
  info.arrival_time_ms += (kLargePositiveDeltaUs + 1000) / 1000;
  ++info.sequence_number;

  // Packets will be added to send history.
  for (const PacketInfo& packet : sent_packets)
    OnSentPacket(packet);
  OnSentPacket(info);

  // Create expected feedback and send into adapter.
  rtc::scoped_ptr<rtcp::TransportFeedback> feedback(
      new rtcp::TransportFeedback());
  feedback->WithBase(sent_packets[0].sequence_number,
                     sent_packets[0].arrival_time_ms * 1000);

  for (const PacketInfo& packet : sent_packets) {
    EXPECT_TRUE(feedback->WithReceivedPacket(packet.sequence_number,
                                             packet.arrival_time_ms * 1000));
  }
  EXPECT_FALSE(feedback->WithReceivedPacket(info.sequence_number,
                                            info.arrival_time_ms * 1000));

  rtc::scoped_ptr<rtcp::RawPacket> raw_packet = feedback->Build();
  feedback = rtcp::TransportFeedback::ParseFrom(raw_packet->Buffer(),
                                                raw_packet->Length());

  std::vector<PacketInfo> received_feedback;

  EXPECT_TRUE(feedback.get() != nullptr);
  EXPECT_CALL(*bitrate_estimator_, IncomingPacketFeedbackVector(_))
      .Times(1)
      .WillOnce(Invoke([sent_packets, &received_feedback](
          const std::vector<PacketInfo>& feedback_vector) {
        EXPECT_EQ(sent_packets.size(), feedback_vector.size());
        received_feedback = feedback_vector;
      }));
  adapter_->OnTransportFeedback(*feedback.get());

  // Create a new feedback message and add the trailing item.
  feedback.reset(new rtcp::TransportFeedback());
  feedback->WithBase(info.sequence_number, info.arrival_time_ms * 1000);
  EXPECT_TRUE(feedback->WithReceivedPacket(info.sequence_number,
                                           info.arrival_time_ms * 1000));
  raw_packet = feedback->Build();
  feedback = rtcp::TransportFeedback::ParseFrom(raw_packet->Buffer(),
                                                raw_packet->Length());

  EXPECT_TRUE(feedback.get() != nullptr);
  EXPECT_CALL(*bitrate_estimator_, IncomingPacketFeedbackVector(_))
      .Times(1)
      .WillOnce(Invoke(
          [&received_feedback](const std::vector<PacketInfo>& feedback_vector) {
            EXPECT_EQ(1u, feedback_vector.size());
            received_feedback.push_back(feedback_vector[0]);
          }));
  adapter_->OnTransportFeedback(*feedback.get());

  sent_packets.push_back(info);

  ComparePacketVectors(sent_packets, received_feedback);
}

}  // namespace test
}  // namespace webrtc
