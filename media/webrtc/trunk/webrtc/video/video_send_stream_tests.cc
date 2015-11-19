/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <algorithm>  // max
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/base/checks.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/call.h"
#include "webrtc/common_video/interface/i420_video_frame.h"
#include "webrtc/common_video/interface/native_handle.h"
#include "webrtc/frame_callback.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_header_parser.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_sender.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_utility.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/event_wrapper.h"
#include "webrtc/system_wrappers/interface/ref_count.h"
#include "webrtc/system_wrappers/interface/sleep.h"
#include "webrtc/system_wrappers/interface/thread_wrapper.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/test/call_test.h"
#include "webrtc/test/configurable_frame_size_encoder.h"
#include "webrtc/test/null_transport.h"
#include "webrtc/test/testsupport/perf_test.h"
#include "webrtc/video/send_statistics_proxy.h"
#include "webrtc/video/transport_adapter.h"
#include "webrtc/video_send_stream.h"

namespace webrtc {

enum VideoFormat { kGeneric, kVP8, };

void ExpectEqualFrames(const I420VideoFrame& frame1,
                       const I420VideoFrame& frame2);
void ExpectEqualTextureFrames(const I420VideoFrame& frame1,
                              const I420VideoFrame& frame2);
void ExpectEqualBufferFrames(const I420VideoFrame& frame1,
                             const I420VideoFrame& frame2);
void ExpectEqualFramesVector(const std::vector<I420VideoFrame>& frames1,
                             const std::vector<I420VideoFrame>& frames2);
I420VideoFrame CreateI420VideoFrame(int width, int height, uint8_t data);

class FakeNativeHandle : public NativeHandle {
 public:
  FakeNativeHandle() {}
  virtual ~FakeNativeHandle() {}
  virtual void* GetHandle() { return nullptr; }
};

class VideoSendStreamTest : public test::CallTest {
 protected:
  void TestNackRetransmission(uint32_t retransmit_ssrc,
                              uint8_t retransmit_payload_type);
  void TestPacketFragmentationSize(VideoFormat format, bool with_fec);
};

TEST_F(VideoSendStreamTest, CanStartStartedStream) {
  test::NullTransport transport;
  Call::Config call_config(&transport);
  CreateSenderCall(call_config);

  CreateSendConfig(1);
  CreateStreams();
  send_stream_->Start();
  send_stream_->Start();
  DestroyStreams();
}

TEST_F(VideoSendStreamTest, CanStopStoppedStream) {
  test::NullTransport transport;
  Call::Config call_config(&transport);
  CreateSenderCall(call_config);

  CreateSendConfig(1);
  CreateStreams();
  send_stream_->Stop();
  send_stream_->Stop();
  DestroyStreams();
}

TEST_F(VideoSendStreamTest, SupportsCName) {
  static std::string kCName = "PjQatC14dGfbVwGPUOA9IH7RlsFDbWl4AhXEiDsBizo=";
  class CNameObserver : public test::SendTest {
   public:
    CNameObserver() : SendTest(kDefaultTimeoutMs) {}

   private:
    Action OnSendRtcp(const uint8_t* packet, size_t length) override {
      RTCPUtility::RTCPParserV2 parser(packet, length, true);
      EXPECT_TRUE(parser.IsValid());

      RTCPUtility::RTCPPacketTypes packet_type = parser.Begin();
      while (packet_type != RTCPUtility::kRtcpNotValidCode) {
        if (packet_type == RTCPUtility::kRtcpSdesChunkCode) {
          EXPECT_EQ(parser.Packet().CName.CName, kCName);
          observation_complete_->Set();
        }

        packet_type = parser.Iterate();
      }

      return SEND_PACKET;
    }

    void ModifyConfigs(VideoSendStream::Config* send_config,
                       std::vector<VideoReceiveStream::Config>* receive_configs,
                       VideoEncoderConfig* encoder_config) override {
      send_config->rtp.c_name = kCName;
    }

    void PerformTest() override {
      EXPECT_EQ(kEventSignaled, Wait())
          << "Timed out while waiting for RTCP with CNAME.";
    }
  } test;

  RunBaseTest(&test);
}

TEST_F(VideoSendStreamTest, SupportsAbsoluteSendTime) {
  static const uint8_t kAbsSendTimeExtensionId = 13;
  class AbsoluteSendTimeObserver : public test::SendTest {
   public:
    AbsoluteSendTimeObserver() : SendTest(kDefaultTimeoutMs) {
      EXPECT_TRUE(parser_->RegisterRtpHeaderExtension(
          kRtpExtensionAbsoluteSendTime, kAbsSendTimeExtensionId));
    }

    Action OnSendRtp(const uint8_t* packet, size_t length) override {
      RTPHeader header;
      EXPECT_TRUE(parser_->Parse(packet, length, &header));

      EXPECT_FALSE(header.extension.hasTransmissionTimeOffset);
      EXPECT_TRUE(header.extension.hasAbsoluteSendTime);
      EXPECT_EQ(header.extension.transmissionTimeOffset, 0);
      EXPECT_GT(header.extension.absoluteSendTime, 0u);
      observation_complete_->Set();

      return SEND_PACKET;
    }

    void ModifyConfigs(VideoSendStream::Config* send_config,
                       std::vector<VideoReceiveStream::Config>* receive_configs,
                       VideoEncoderConfig* encoder_config) override {
      send_config->rtp.extensions.push_back(
          RtpExtension(RtpExtension::kAbsSendTime, kAbsSendTimeExtensionId));
    }

    void PerformTest() override {
      EXPECT_EQ(kEventSignaled, Wait())
          << "Timed out while waiting for single RTP packet.";
    }
  } test;

  RunBaseTest(&test);
}

TEST_F(VideoSendStreamTest, SupportsTransmissionTimeOffset) {
  static const uint8_t kTOffsetExtensionId = 13;
  static const int kEncodeDelayMs = 5;
  class TransmissionTimeOffsetObserver : public test::SendTest {
   public:
    TransmissionTimeOffsetObserver()
        : SendTest(kDefaultTimeoutMs),
          encoder_(Clock::GetRealTimeClock(), kEncodeDelayMs) {
      EXPECT_TRUE(parser_->RegisterRtpHeaderExtension(
          kRtpExtensionTransmissionTimeOffset, kTOffsetExtensionId));
    }

   private:
    Action OnSendRtp(const uint8_t* packet, size_t length) override {
      RTPHeader header;
      EXPECT_TRUE(parser_->Parse(packet, length, &header));

      EXPECT_TRUE(header.extension.hasTransmissionTimeOffset);
      EXPECT_FALSE(header.extension.hasAbsoluteSendTime);
      EXPECT_GT(header.extension.transmissionTimeOffset, 0);
      EXPECT_EQ(header.extension.absoluteSendTime, 0u);
      observation_complete_->Set();

      return SEND_PACKET;
    }

    void ModifyConfigs(VideoSendStream::Config* send_config,
                       std::vector<VideoReceiveStream::Config>* receive_configs,
                       VideoEncoderConfig* encoder_config) override {
      send_config->encoder_settings.encoder = &encoder_;
      send_config->rtp.extensions.push_back(
          RtpExtension(RtpExtension::kTOffset, kTOffsetExtensionId));
    }

    void PerformTest() override {
      EXPECT_EQ(kEventSignaled, Wait())
          << "Timed out while waiting for a single RTP packet.";
    }

    test::DelayedEncoder encoder_;
  } test;

  RunBaseTest(&test);
}

class FakeReceiveStatistics : public NullReceiveStatistics {
 public:
  FakeReceiveStatistics(uint32_t send_ssrc,
                        uint32_t last_sequence_number,
                        uint32_t cumulative_lost,
                        uint8_t fraction_lost)
      : lossy_stats_(new LossyStatistician(last_sequence_number,
                                           cumulative_lost,
                                           fraction_lost)) {
    stats_map_[send_ssrc] = lossy_stats_.get();
  }

  StatisticianMap GetActiveStatisticians() const override { return stats_map_; }

  StreamStatistician* GetStatistician(uint32_t ssrc) const override {
    return lossy_stats_.get();
  }

 private:
  class LossyStatistician : public StreamStatistician {
   public:
    LossyStatistician(uint32_t extended_max_sequence_number,
                      uint32_t cumulative_lost,
                      uint8_t fraction_lost) {
      stats_.fraction_lost = fraction_lost;
      stats_.cumulative_lost = cumulative_lost;
      stats_.extended_max_sequence_number = extended_max_sequence_number;
    }
    bool GetStatistics(RtcpStatistics* statistics, bool reset) override {
      *statistics = stats_;
      return true;
    }
    void GetDataCounters(size_t* bytes_received,
                         uint32_t* packets_received) const override {
      *bytes_received = 0;
      *packets_received = 0;
    }
    void GetReceiveStreamDataCounters(
        StreamDataCounters* data_counters) const override {}
    uint32_t BitrateReceived() const override { return 0; }
    void ResetStatistics() override {}
    bool IsRetransmitOfOldPacket(const RTPHeader& header,
                                 int64_t min_rtt) const override {
      return false;
    }

    bool IsPacketInOrder(uint16_t sequence_number) const override {
      return true;
    }

    RtcpStatistics stats_;
  };

  rtc::scoped_ptr<LossyStatistician> lossy_stats_;
  StatisticianMap stats_map_;
};

TEST_F(VideoSendStreamTest, SupportsFec) {
  class FecObserver : public test::SendTest {
   public:
    FecObserver()
        : SendTest(kDefaultTimeoutMs),
          transport_adapter_(SendTransport()),
          send_count_(0),
          received_media_(false),
          received_fec_(false) {
      transport_adapter_.Enable();
    }

   private:
    Action OnSendRtp(const uint8_t* packet, size_t length) override {
      RTPHeader header;
      EXPECT_TRUE(parser_->Parse(packet, length, &header));

      // Send lossy receive reports to trigger FEC enabling.
      if (send_count_++ % 2 != 0) {
        // Receive statistics reporting having lost 50% of the packets.
        FakeReceiveStatistics lossy_receive_stats(
            kSendSsrcs[0], header.sequenceNumber, send_count_ / 2, 127);
        RTCPSender rtcp_sender(0, false, Clock::GetRealTimeClock(),
                               &lossy_receive_stats, nullptr);
        EXPECT_EQ(0, rtcp_sender.RegisterSendTransport(&transport_adapter_));

        rtcp_sender.SetRTCPStatus(kRtcpNonCompound);
        rtcp_sender.SetRemoteSSRC(kSendSsrcs[0]);

        RTCPSender::FeedbackState feedback_state;

        EXPECT_EQ(0, rtcp_sender.SendRTCP(feedback_state, kRtcpRr));
      }

      EXPECT_EQ(kRedPayloadType, header.payloadType);

      uint8_t encapsulated_payload_type = packet[header.headerLength];

      if (encapsulated_payload_type == kUlpfecPayloadType) {
        received_fec_ = true;
      } else {
        received_media_ = true;
      }

      if (received_media_ && received_fec_)
        observation_complete_->Set();

      return SEND_PACKET;
    }

    void ModifyConfigs(VideoSendStream::Config* send_config,
                       std::vector<VideoReceiveStream::Config>* receive_configs,
                       VideoEncoderConfig* encoder_config) override {
      send_config->rtp.fec.red_payload_type = kRedPayloadType;
      send_config->rtp.fec.ulpfec_payload_type = kUlpfecPayloadType;
    }

    void PerformTest() override {
      EXPECT_TRUE(Wait()) << "Timed out waiting for FEC and media packets.";
    }

    internal::TransportAdapter transport_adapter_;
    int send_count_;
    bool received_media_;
    bool received_fec_;
  } test;

  RunBaseTest(&test);
}

void VideoSendStreamTest::TestNackRetransmission(
    uint32_t retransmit_ssrc,
    uint8_t retransmit_payload_type) {
  class NackObserver : public test::SendTest {
   public:
    explicit NackObserver(uint32_t retransmit_ssrc,
                          uint8_t retransmit_payload_type)
        : SendTest(kDefaultTimeoutMs),
          transport_adapter_(SendTransport()),
          send_count_(0),
          retransmit_ssrc_(retransmit_ssrc),
          retransmit_payload_type_(retransmit_payload_type),
          nacked_sequence_number_(-1) {
      transport_adapter_.Enable();
    }

   private:
    Action OnSendRtp(const uint8_t* packet, size_t length) override {
      RTPHeader header;
      EXPECT_TRUE(parser_->Parse(packet, length, &header));

      // Nack second packet after receiving the third one.
      if (++send_count_ == 3) {
        uint16_t nack_sequence_number = header.sequenceNumber - 1;
        nacked_sequence_number_ = nack_sequence_number;
        NullReceiveStatistics null_stats;
        RTCPSender rtcp_sender(
            0, false, Clock::GetRealTimeClock(), &null_stats, nullptr);
        EXPECT_EQ(0, rtcp_sender.RegisterSendTransport(&transport_adapter_));

        rtcp_sender.SetRTCPStatus(kRtcpNonCompound);
        rtcp_sender.SetRemoteSSRC(kSendSsrcs[0]);

        RTCPSender::FeedbackState feedback_state;

        EXPECT_EQ(0,
                  rtcp_sender.SendRTCP(
                      feedback_state, kRtcpNack, 1, &nack_sequence_number));
      }

      uint16_t sequence_number = header.sequenceNumber;

      if (header.ssrc == retransmit_ssrc_ &&
          retransmit_ssrc_ != kSendSsrcs[0]) {
        // Not kSendSsrcs[0], assume correct RTX packet. Extract sequence
        // number.
        const uint8_t* rtx_header = packet + header.headerLength;
        sequence_number = (rtx_header[0] << 8) + rtx_header[1];
      }

      if (sequence_number == nacked_sequence_number_) {
        EXPECT_EQ(retransmit_ssrc_, header.ssrc);
        EXPECT_EQ(retransmit_payload_type_, header.payloadType);
        observation_complete_->Set();
      }

      return SEND_PACKET;
    }

    void ModifyConfigs(VideoSendStream::Config* send_config,
                       std::vector<VideoReceiveStream::Config>* receive_configs,
                       VideoEncoderConfig* encoder_config) override {
      send_config->rtp.nack.rtp_history_ms = kNackRtpHistoryMs;
      send_config->rtp.rtx.payload_type = retransmit_payload_type_;
      if (retransmit_ssrc_ != kSendSsrcs[0])
        send_config->rtp.rtx.ssrcs.push_back(retransmit_ssrc_);
    }

    void PerformTest() override {
      EXPECT_EQ(kEventSignaled, Wait())
          << "Timed out while waiting for NACK retransmission.";
    }

    internal::TransportAdapter transport_adapter_;
    int send_count_;
    uint32_t retransmit_ssrc_;
    uint8_t retransmit_payload_type_;
    int nacked_sequence_number_;
  } test(retransmit_ssrc, retransmit_payload_type);

  RunBaseTest(&test);
}

TEST_F(VideoSendStreamTest, RetransmitsNack) {
  // Normal NACKs should use the send SSRC.
  TestNackRetransmission(kSendSsrcs[0], kFakeSendPayloadType);
}

TEST_F(VideoSendStreamTest, RetransmitsNackOverRtx) {
  // NACKs over RTX should use a separate SSRC.
  TestNackRetransmission(kSendRtxSsrcs[0], kSendRtxPayloadType);
}

void VideoSendStreamTest::TestPacketFragmentationSize(VideoFormat format,
                                                      bool with_fec) {
  // Use a fake encoder to output a frame of every size in the range [90, 290],
  // for each size making sure that the exact number of payload bytes received
  // is correct and that packets are fragmented to respect max packet size.
  static const size_t kMaxPacketSize = 128;
  static const size_t start = 90;
  static const size_t stop = 290;

  // Observer that verifies that the expected number of packets and bytes
  // arrive for each frame size, from start_size to stop_size.
  class FrameFragmentationTest : public test::SendTest,
                                 public EncodedFrameObserver {
   public:
    FrameFragmentationTest(size_t max_packet_size,
                           size_t start_size,
                           size_t stop_size,
                           bool test_generic_packetization,
                           bool use_fec)
        : SendTest(kLongTimeoutMs),
          transport_adapter_(SendTransport()),
          encoder_(stop),
          max_packet_size_(max_packet_size),
          stop_size_(stop_size),
          test_generic_packetization_(test_generic_packetization),
          use_fec_(use_fec),
          packet_count_(0),
          accumulated_size_(0),
          accumulated_payload_(0),
          fec_packet_received_(false),
          current_size_rtp_(start_size),
          current_size_frame_(static_cast<int32_t>(start_size)) {
      // Fragmentation required, this test doesn't make sense without it.
      encoder_.SetFrameSize(start_size);
      DCHECK_GT(stop_size, max_packet_size);
      transport_adapter_.Enable();
    }

   private:
    Action OnSendRtp(const uint8_t* packet, size_t size) override {
      size_t length = size;
      RTPHeader header;
      EXPECT_TRUE(parser_->Parse(packet, length, &header));

      EXPECT_LE(length, max_packet_size_);

      if (use_fec_) {
        uint8_t payload_type = packet[header.headerLength];
        bool is_fec = header.payloadType == kRedPayloadType &&
                      payload_type == kUlpfecPayloadType;
        if (is_fec) {
          fec_packet_received_ = true;
          return SEND_PACKET;
        }
      }

      accumulated_size_ += length;

      if (use_fec_)
        TriggerLossReport(header);

      if (test_generic_packetization_) {
        size_t overhead = header.headerLength + header.paddingLength +
                          (1 /* Generic header */);
        if (use_fec_)
          overhead += 1;  // RED for FEC header.
        accumulated_payload_ += length - overhead;
      }

      // Marker bit set indicates last packet of a frame.
      if (header.markerBit) {
        if (use_fec_ && accumulated_payload_ == current_size_rtp_ - 1) {
          // With FEC enabled, frame size is incremented asynchronously, so
          // "old" frames one byte too small may arrive. Accept, but don't
          // increase expected frame size.
          accumulated_size_ = 0;
          accumulated_payload_ = 0;
          return SEND_PACKET;
        }

        EXPECT_GE(accumulated_size_, current_size_rtp_);
        if (test_generic_packetization_) {
          EXPECT_EQ(current_size_rtp_, accumulated_payload_);
        }

        // Last packet of frame; reset counters.
        accumulated_size_ = 0;
        accumulated_payload_ = 0;
        if (current_size_rtp_ == stop_size_) {
          // Done! (Don't increase size again, might arrive more @ stop_size).
          observation_complete_->Set();
        } else {
          // Increase next expected frame size. If testing with FEC, make sure
          // a FEC packet has been received for this frame size before
          // proceeding, to make sure that redundancy packets don't exceed
          // size limit.
          if (!use_fec_) {
            ++current_size_rtp_;
          } else if (fec_packet_received_) {
            fec_packet_received_ = false;
            ++current_size_rtp_;
            ++current_size_frame_;
          }
        }
      }

      return SEND_PACKET;
    }

    void TriggerLossReport(const RTPHeader& header) {
      // Send lossy receive reports to trigger FEC enabling.
      if (packet_count_++ % 2 != 0) {
        // Receive statistics reporting having lost 50% of the packets.
        FakeReceiveStatistics lossy_receive_stats(
            kSendSsrcs[0], header.sequenceNumber, packet_count_ / 2, 127);
        RTCPSender rtcp_sender(0, false, Clock::GetRealTimeClock(),
                               &lossy_receive_stats, nullptr);
        EXPECT_EQ(0, rtcp_sender.RegisterSendTransport(&transport_adapter_));

        rtcp_sender.SetRTCPStatus(kRtcpNonCompound);
        rtcp_sender.SetRemoteSSRC(kSendSsrcs[0]);

        RTCPSender::FeedbackState feedback_state;

        EXPECT_EQ(0, rtcp_sender.SendRTCP(feedback_state, kRtcpRr));
      }
    }

    virtual void EncodedFrameCallback(const EncodedFrame& encoded_frame) {
      // Increase frame size for next encoded frame, in the context of the
      // encoder thread.
      if (!use_fec_ &&
          current_size_frame_.Value() < static_cast<int32_t>(stop_size_)) {
        ++current_size_frame_;
      }
      encoder_.SetFrameSize(static_cast<size_t>(current_size_frame_.Value()));
    }

    Call::Config GetSenderCallConfig() override {
      Call::Config config(SendTransport());
      const int kMinBitrateBps = 30000;
      config.bitrate_config.min_bitrate_bps = kMinBitrateBps;
      return config;
    }

    void ModifyConfigs(VideoSendStream::Config* send_config,
                       std::vector<VideoReceiveStream::Config>* receive_configs,
                       VideoEncoderConfig* encoder_config) override {
      if (use_fec_) {
        send_config->rtp.fec.red_payload_type = kRedPayloadType;
        send_config->rtp.fec.ulpfec_payload_type = kUlpfecPayloadType;
      }

      if (!test_generic_packetization_)
        send_config->encoder_settings.payload_name = "VP8";

      send_config->encoder_settings.encoder = &encoder_;
      send_config->rtp.max_packet_size = kMaxPacketSize;
      send_config->post_encode_callback = this;

      // Add an extension header, to make the RTP header larger than the base
      // length of 12 bytes.
      static const uint8_t kAbsSendTimeExtensionId = 13;
      send_config->rtp.extensions.push_back(
          RtpExtension(RtpExtension::kAbsSendTime, kAbsSendTimeExtensionId));
    }

    void PerformTest() override {
      EXPECT_EQ(kEventSignaled, Wait())
          << "Timed out while observing incoming RTP packets.";
    }

    internal::TransportAdapter transport_adapter_;
    test::ConfigurableFrameSizeEncoder encoder_;

    const size_t max_packet_size_;
    const size_t stop_size_;
    const bool test_generic_packetization_;
    const bool use_fec_;

    uint32_t packet_count_;
    size_t accumulated_size_;
    size_t accumulated_payload_;
    bool fec_packet_received_;

    size_t current_size_rtp_;
    Atomic32 current_size_frame_;
  };

  // Don't auto increment if FEC is used; continue sending frame size until
  // a FEC packet has been received.
  FrameFragmentationTest test(
      kMaxPacketSize, start, stop, format == kGeneric, with_fec);

  RunBaseTest(&test);
}

// TODO(sprang): Is there any way of speeding up these tests?
TEST_F(VideoSendStreamTest, FragmentsGenericAccordingToMaxPacketSize) {
  TestPacketFragmentationSize(kGeneric, false);
}

TEST_F(VideoSendStreamTest, FragmentsGenericAccordingToMaxPacketSizeWithFec) {
  TestPacketFragmentationSize(kGeneric, true);
}

TEST_F(VideoSendStreamTest, FragmentsVp8AccordingToMaxPacketSize) {
  TestPacketFragmentationSize(kVP8, false);
}

TEST_F(VideoSendStreamTest, FragmentsVp8AccordingToMaxPacketSizeWithFec) {
  TestPacketFragmentationSize(kVP8, true);
}

// The test will go through a number of phases.
// 1. Start sending packets.
// 2. As soon as the RTP stream has been detected, signal a low REMB value to
//    suspend the stream.
// 3. Wait until |kSuspendTimeFrames| have been captured without seeing any RTP
//    packets.
// 4. Signal a high REMB and then wait for the RTP stream to start again.
//    When the stream is detected again, and the stats show that the stream
//    is no longer suspended, the test ends.
TEST_F(VideoSendStreamTest, SuspendBelowMinBitrate) {
  static const int kSuspendTimeFrames = 60;  // Suspend for 2 seconds @ 30 fps.

  class RembObserver : public test::SendTest, public I420FrameCallback {
   public:
    RembObserver()
        : SendTest(kDefaultTimeoutMs),
          transport_adapter_(&transport_),
          clock_(Clock::GetRealTimeClock()),
          crit_(CriticalSectionWrapper::CreateCriticalSection()),
          test_state_(kBeforeSuspend),
          rtp_count_(0),
          last_sequence_number_(0),
          suspended_frame_count_(0),
          low_remb_bps_(0),
          high_remb_bps_(0) {
      transport_adapter_.Enable();
    }

   private:
    Action OnSendRtcp(const uint8_t* packet, size_t length) override {
      // Receive statistics reporting having lost 0% of the packets.
      // This is needed for the send-side bitrate controller to work properly.
      CriticalSectionScoped lock(crit_.get());
      SendRtcpFeedback(0);  // REMB is only sent if value is > 0.
      return SEND_PACKET;
    }

    Action OnSendRtp(const uint8_t* packet, size_t length) override {
      CriticalSectionScoped lock(crit_.get());
      ++rtp_count_;
      RTPHeader header;
      EXPECT_TRUE(parser_->Parse(packet, length, &header));
      last_sequence_number_ = header.sequenceNumber;

      if (test_state_ == kBeforeSuspend) {
        // The stream has started. Try to suspend it.
        SendRtcpFeedback(low_remb_bps_);
        test_state_ = kDuringSuspend;
      } else if (test_state_ == kDuringSuspend) {
        if (header.paddingLength == 0) {
          // Received non-padding packet during suspension period. Reset the
          // counter.
          suspended_frame_count_ = 0;
        }
      } else if (test_state_ == kWaitingForPacket) {
        if (header.paddingLength == 0) {
          // Non-padding packet observed. Test is almost complete. Will just
          // have to wait for the stats to change.
          test_state_ = kWaitingForStats;
        }
      } else if (test_state_ == kWaitingForStats) {
        VideoSendStream::Stats stats = stream_->GetStats();
        if (stats.suspended == false) {
          // Stats flipped to false. Test is complete.
          observation_complete_->Set();
        }
      }

      return SEND_PACKET;
    }

    // This method implements the I420FrameCallback.
    void FrameCallback(I420VideoFrame* video_frame) override {
      CriticalSectionScoped lock(crit_.get());
      if (test_state_ == kDuringSuspend &&
          ++suspended_frame_count_ > kSuspendTimeFrames) {
        VideoSendStream::Stats stats = stream_->GetStats();
        EXPECT_TRUE(stats.suspended);
        SendRtcpFeedback(high_remb_bps_);
        test_state_ = kWaitingForPacket;
      }
    }

    void set_low_remb_bps(int value) {
      CriticalSectionScoped lock(crit_.get());
      low_remb_bps_ = value;
    }

    void set_high_remb_bps(int value) {
      CriticalSectionScoped lock(crit_.get());
      high_remb_bps_ = value;
    }

    void SetReceivers(PacketReceiver* send_transport_receiver,
                      PacketReceiver* receive_transport_receiver) override {
      transport_.SetReceiver(send_transport_receiver);
    }

    void OnStreamsCreated(
        VideoSendStream* send_stream,
        const std::vector<VideoReceiveStream*>& receive_streams) override {
      stream_ = send_stream;
    }

    void ModifyConfigs(VideoSendStream::Config* send_config,
                       std::vector<VideoReceiveStream::Config>* receive_configs,
                       VideoEncoderConfig* encoder_config) override {
      send_config->rtp.nack.rtp_history_ms = kNackRtpHistoryMs;
      send_config->pre_encode_callback = this;
      send_config->suspend_below_min_bitrate = true;
      int min_bitrate_bps = encoder_config->streams[0].min_bitrate_bps;
      set_low_remb_bps(min_bitrate_bps - 10000);
      int threshold_window = std::max(min_bitrate_bps / 10, 10000);
      ASSERT_GT(encoder_config->streams[0].max_bitrate_bps,
                min_bitrate_bps + threshold_window + 5000);
      set_high_remb_bps(min_bitrate_bps + threshold_window + 5000);
    }

    void PerformTest() override {
      EXPECT_EQ(kEventSignaled, Wait())
          << "Timed out during suspend-below-min-bitrate test.";
      transport_.StopSending();
    }

    enum TestState {
      kBeforeSuspend,
      kDuringSuspend,
      kWaitingForPacket,
      kWaitingForStats
    };

    virtual void SendRtcpFeedback(int remb_value)
        EXCLUSIVE_LOCKS_REQUIRED(crit_) {
      FakeReceiveStatistics receive_stats(
          kSendSsrcs[0], last_sequence_number_, rtp_count_, 0);
      RTCPSender rtcp_sender(0, false, clock_, &receive_stats, nullptr);
      EXPECT_EQ(0, rtcp_sender.RegisterSendTransport(&transport_adapter_));

      rtcp_sender.SetRTCPStatus(kRtcpNonCompound);
      rtcp_sender.SetRemoteSSRC(kSendSsrcs[0]);
      if (remb_value > 0) {
        rtcp_sender.SetREMBStatus(true);
        rtcp_sender.SetREMBData(remb_value, std::vector<uint32_t>());
      }
      RTCPSender::FeedbackState feedback_state;
      EXPECT_EQ(0, rtcp_sender.SendRTCP(feedback_state, kRtcpRr));
    }

    internal::TransportAdapter transport_adapter_;
    test::DirectTransport transport_;
    Clock* const clock_;
    VideoSendStream* stream_;

    const rtc::scoped_ptr<CriticalSectionWrapper> crit_;
    TestState test_state_ GUARDED_BY(crit_);
    int rtp_count_ GUARDED_BY(crit_);
    int last_sequence_number_ GUARDED_BY(crit_);
    int suspended_frame_count_ GUARDED_BY(crit_);
    int low_remb_bps_ GUARDED_BY(crit_);
    int high_remb_bps_ GUARDED_BY(crit_);
  } test;

  RunBaseTest(&test);
}

TEST_F(VideoSendStreamTest, NoPaddingWhenVideoIsMuted) {
  class NoPaddingWhenVideoIsMuted : public test::SendTest {
   public:
    NoPaddingWhenVideoIsMuted()
        : SendTest(kDefaultTimeoutMs),
          clock_(Clock::GetRealTimeClock()),
          transport_adapter_(ReceiveTransport()),
          crit_(CriticalSectionWrapper::CreateCriticalSection()),
          last_packet_time_ms_(-1),
          capturer_(nullptr) {
      transport_adapter_.Enable();
    }

   private:
    Action OnSendRtp(const uint8_t* packet, size_t length) override {
      CriticalSectionScoped lock(crit_.get());
      last_packet_time_ms_ = clock_->TimeInMilliseconds();
      capturer_->Stop();
      return SEND_PACKET;
    }

    Action OnSendRtcp(const uint8_t* packet, size_t length) override {
      CriticalSectionScoped lock(crit_.get());
      const int kVideoMutedThresholdMs = 10000;
      if (last_packet_time_ms_ > 0 &&
          clock_->TimeInMilliseconds() - last_packet_time_ms_ >
              kVideoMutedThresholdMs)
        observation_complete_->Set();
      // Receive statistics reporting having lost 50% of the packets.
      FakeReceiveStatistics receive_stats(kSendSsrcs[0], 1, 1, 0);
      RTCPSender rtcp_sender(0, false, Clock::GetRealTimeClock(),
                             &receive_stats, nullptr);
      EXPECT_EQ(0, rtcp_sender.RegisterSendTransport(&transport_adapter_));

      rtcp_sender.SetRTCPStatus(kRtcpNonCompound);
      rtcp_sender.SetRemoteSSRC(kSendSsrcs[0]);

      RTCPSender::FeedbackState feedback_state;

      EXPECT_EQ(0, rtcp_sender.SendRTCP(feedback_state, kRtcpRr));
      return SEND_PACKET;
    }

    void SetReceivers(PacketReceiver* send_transport_receiver,
                      PacketReceiver* receive_transport_receiver) override {
      RtpRtcpObserver::SetReceivers(send_transport_receiver,
                                    send_transport_receiver);
    }

    size_t GetNumStreams() const override { return 3; }

    virtual void OnFrameGeneratorCapturerCreated(
        test::FrameGeneratorCapturer* frame_generator_capturer) {
      CriticalSectionScoped lock(crit_.get());
      capturer_ = frame_generator_capturer;
    }

    void PerformTest() override {
      EXPECT_EQ(kEventSignaled, Wait())
          << "Timed out while waiting for RTP packets to stop being sent.";
    }

    Clock* const clock_;
    internal::TransportAdapter transport_adapter_;
    const rtc::scoped_ptr<CriticalSectionWrapper> crit_;
    int64_t last_packet_time_ms_ GUARDED_BY(crit_);
    test::FrameGeneratorCapturer* capturer_ GUARDED_BY(crit_);
  } test;

  RunBaseTest(&test);
}

// This test first observes "high" bitrate use at which point it sends a REMB to
// indicate that it should be lowered significantly. The test then observes that
// the bitrate observed is sinking well below the min-transmit-bitrate threshold
// to verify that the min-transmit bitrate respects incoming REMB.
//
// Note that the test starts at "high" bitrate and does not ramp up to "higher"
// bitrate since no receiver block or remb is sent in the initial phase.
TEST_F(VideoSendStreamTest, MinTransmitBitrateRespectsRemb) {
  static const int kMinTransmitBitrateBps = 400000;
  static const int kHighBitrateBps = 150000;
  static const int kRembBitrateBps = 80000;
  static const int kRembRespectedBitrateBps = 100000;
  class BitrateObserver : public test::SendTest, public PacketReceiver {
   public:
    BitrateObserver()
        : SendTest(kDefaultTimeoutMs),
          feedback_transport_(ReceiveTransport()),
          bitrate_capped_(false) {
      RtpRtcp::Configuration config;
      feedback_transport_.Enable();
      config.outgoing_transport = &feedback_transport_;
      rtp_rtcp_.reset(RtpRtcp::CreateRtpRtcp(config));
      rtp_rtcp_->SetREMBStatus(true);
      rtp_rtcp_->SetRTCPStatus(kRtcpNonCompound);
    }

    void OnStreamsCreated(
        VideoSendStream* send_stream,
        const std::vector<VideoReceiveStream*>& receive_streams) override {
      stream_ = send_stream;
    }

   private:
    DeliveryStatus DeliverPacket(const uint8_t* packet,
                                 size_t length) override {
      if (RtpHeaderParser::IsRtcp(packet, length))
        return DELIVERY_OK;

      RTPHeader header;
      if (!parser_->Parse(packet, length, &header))
        return DELIVERY_PACKET_ERROR;
      DCHECK(stream_ != nullptr);
      VideoSendStream::Stats stats = stream_->GetStats();
      if (!stats.substreams.empty()) {
        EXPECT_EQ(1u, stats.substreams.size());
        int total_bitrate_bps =
            stats.substreams.begin()->second.total_bitrate_bps;
        test::PrintResult("bitrate_stats_",
                          "min_transmit_bitrate_low_remb",
                          "bitrate_bps",
                          static_cast<size_t>(total_bitrate_bps),
                          "bps",
                          false);
        if (total_bitrate_bps > kHighBitrateBps) {
          rtp_rtcp_->SetREMBData(kRembBitrateBps,
                                 std::vector<uint32_t>(1, header.ssrc));
          rtp_rtcp_->Process();
          bitrate_capped_ = true;
        } else if (bitrate_capped_ &&
                   total_bitrate_bps < kRembRespectedBitrateBps) {
          observation_complete_->Set();
        }
      }
      return DELIVERY_OK;
    }

    void SetReceivers(PacketReceiver* send_transport_receiver,
                      PacketReceiver* receive_transport_receiver) override {
      RtpRtcpObserver::SetReceivers(this, send_transport_receiver);
    }

    void ModifyConfigs(VideoSendStream::Config* send_config,
                       std::vector<VideoReceiveStream::Config>* receive_configs,
                       VideoEncoderConfig* encoder_config) override {
      encoder_config->min_transmit_bitrate_bps = kMinTransmitBitrateBps;
    }

    void PerformTest() override {
      EXPECT_EQ(kEventSignaled, Wait())
          << "Timeout while waiting for low bitrate stats after REMB.";
    }

    rtc::scoped_ptr<RtpRtcp> rtp_rtcp_;
    internal::TransportAdapter feedback_transport_;
    VideoSendStream* stream_;
    bool bitrate_capped_;
  } test;

  RunBaseTest(&test);
}

TEST_F(VideoSendStreamTest, CanReconfigureToUseStartBitrateAbovePreviousMax) {
  class StartBitrateObserver : public test::FakeEncoder {
   public:
    StartBitrateObserver()
        : FakeEncoder(Clock::GetRealTimeClock()), start_bitrate_kbps_(0) {}
    int32_t InitEncode(const VideoCodec* config,
                       int32_t number_of_cores,
                       size_t max_payload_size) override {
      rtc::CritScope lock(&crit_);
      start_bitrate_kbps_ = config->startBitrate;
      return FakeEncoder::InitEncode(config, number_of_cores, max_payload_size);
    }

    int32_t SetRates(uint32_t new_target_bitrate, uint32_t framerate) override {
      rtc::CritScope lock(&crit_);
      start_bitrate_kbps_ = new_target_bitrate;
      return FakeEncoder::SetRates(new_target_bitrate, framerate);
    }

    int GetStartBitrateKbps() const {
      rtc::CritScope lock(&crit_);
      return start_bitrate_kbps_;
    }

   private:
    mutable rtc::CriticalSection crit_;
    int start_bitrate_kbps_ GUARDED_BY(crit_);
  };

  test::NullTransport transport;
  CreateSenderCall(Call::Config(&transport));

  CreateSendConfig(1);

  Call::Config::BitrateConfig bitrate_config;
  bitrate_config.start_bitrate_bps =
      2 * encoder_config_.streams[0].max_bitrate_bps;
  sender_call_->SetBitrateConfig(bitrate_config);

  StartBitrateObserver encoder;
  send_config_.encoder_settings.encoder = &encoder;

  CreateStreams();

  EXPECT_EQ(encoder_config_.streams[0].max_bitrate_bps / 1000,
            encoder.GetStartBitrateKbps());

  encoder_config_.streams[0].max_bitrate_bps =
      2 * bitrate_config.start_bitrate_bps;
  send_stream_->ReconfigureVideoEncoder(encoder_config_);

  // New bitrate should be reconfigured above the previous max. As there's no
  // network connection this shouldn't be flaky, as no bitrate should've been
  // reported in between.
  EXPECT_EQ(bitrate_config.start_bitrate_bps / 1000,
            encoder.GetStartBitrateKbps());

  DestroyStreams();
}

TEST_F(VideoSendStreamTest, CapturesTextureAndI420VideoFrames) {
  class FrameObserver : public I420FrameCallback {
   public:
    FrameObserver() : output_frame_event_(EventWrapper::Create()) {}

    void FrameCallback(I420VideoFrame* video_frame) override {
      output_frames_.push_back(*video_frame);
      output_frame_event_->Set();
    }

    void WaitOutputFrame() {
      const unsigned long kWaitFrameTimeoutMs = 3000;
      EXPECT_EQ(kEventSignaled, output_frame_event_->Wait(kWaitFrameTimeoutMs))
          << "Timeout while waiting for output frames.";
    }

    const std::vector<I420VideoFrame>& output_frames() const {
      return output_frames_;
    }

   private:
    // Delivered output frames.
    std::vector<I420VideoFrame> output_frames_;

    // Indicate an output frame has arrived.
    rtc::scoped_ptr<EventWrapper> output_frame_event_;
  };

  // Initialize send stream.
  test::NullTransport transport;
  CreateSenderCall(Call::Config(&transport));

  CreateSendConfig(1);
  FrameObserver observer;
  send_config_.pre_encode_callback = &observer;
  CreateStreams();

  // Prepare five input frames. Send ordinary I420VideoFrame and texture frames
  // alternatively.
  std::vector<I420VideoFrame> input_frames;
  int width = static_cast<int>(encoder_config_.streams[0].width);
  int height = static_cast<int>(encoder_config_.streams[0].height);
  webrtc::RefCountImpl<FakeNativeHandle>* handle1 =
      new webrtc::RefCountImpl<FakeNativeHandle>();
  webrtc::RefCountImpl<FakeNativeHandle>* handle2 =
      new webrtc::RefCountImpl<FakeNativeHandle>();
  webrtc::RefCountImpl<FakeNativeHandle>* handle3 =
      new webrtc::RefCountImpl<FakeNativeHandle>();
  input_frames.push_back(I420VideoFrame(handle1, width, height, 1, 1));
  input_frames.push_back(I420VideoFrame(handle2, width, height, 2, 2));
  input_frames.push_back(CreateI420VideoFrame(width, height, 3));
  input_frames.push_back(CreateI420VideoFrame(width, height, 4));
  input_frames.push_back(I420VideoFrame(handle3, width, height, 5, 5));

  send_stream_->Start();
  for (size_t i = 0; i < input_frames.size(); i++) {
    send_stream_->Input()->IncomingCapturedFrame(input_frames[i]);
    // Do not send the next frame too fast, so the frame dropper won't drop it.
    if (i < input_frames.size() - 1)
      SleepMs(1000 / encoder_config_.streams[0].max_framerate);
    // Wait until the output frame is received before sending the next input
    // frame. Or the previous input frame may be replaced without delivering.
    observer.WaitOutputFrame();
  }
  send_stream_->Stop();

  // Test if the input and output frames are the same. render_time_ms and
  // timestamp are not compared because capturer sets those values.
  ExpectEqualFramesVector(input_frames, observer.output_frames());

  DestroyStreams();
}

void ExpectEqualFrames(const I420VideoFrame& frame1,
                       const I420VideoFrame& frame2) {
  if (frame1.native_handle() != nullptr || frame2.native_handle() != nullptr)
    ExpectEqualTextureFrames(frame1, frame2);
  else
    ExpectEqualBufferFrames(frame1, frame2);
}

void ExpectEqualTextureFrames(const I420VideoFrame& frame1,
                              const I420VideoFrame& frame2) {
  EXPECT_EQ(frame1.native_handle(), frame2.native_handle());
  EXPECT_EQ(frame1.width(), frame2.width());
  EXPECT_EQ(frame1.height(), frame2.height());
  EXPECT_EQ(frame1.render_time_ms(), frame2.render_time_ms());
}

void ExpectEqualBufferFrames(const I420VideoFrame& frame1,
                             const I420VideoFrame& frame2) {
  EXPECT_EQ(frame1.width(), frame2.width());
  EXPECT_EQ(frame1.height(), frame2.height());
  EXPECT_EQ(frame1.stride(kYPlane), frame2.stride(kYPlane));
  EXPECT_EQ(frame1.stride(kUPlane), frame2.stride(kUPlane));
  EXPECT_EQ(frame1.stride(kVPlane), frame2.stride(kVPlane));
  EXPECT_EQ(frame1.render_time_ms(), frame2.render_time_ms());
  ASSERT_EQ(frame1.allocated_size(kYPlane), frame2.allocated_size(kYPlane));
  EXPECT_EQ(0,
            memcmp(frame1.buffer(kYPlane),
                   frame2.buffer(kYPlane),
                   frame1.allocated_size(kYPlane)));
  ASSERT_EQ(frame1.allocated_size(kUPlane), frame2.allocated_size(kUPlane));
  EXPECT_EQ(0,
            memcmp(frame1.buffer(kUPlane),
                   frame2.buffer(kUPlane),
                   frame1.allocated_size(kUPlane)));
  ASSERT_EQ(frame1.allocated_size(kVPlane), frame2.allocated_size(kVPlane));
  EXPECT_EQ(0,
            memcmp(frame1.buffer(kVPlane),
                   frame2.buffer(kVPlane),
                   frame1.allocated_size(kVPlane)));
}

void ExpectEqualFramesVector(const std::vector<I420VideoFrame>& frames1,
                             const std::vector<I420VideoFrame>& frames2) {
  EXPECT_EQ(frames1.size(), frames2.size());
  for (size_t i = 0; i < std::min(frames1.size(), frames2.size()); ++i)
    ExpectEqualFrames(frames1[i], frames2[i]);
}

I420VideoFrame CreateI420VideoFrame(int width, int height, uint8_t data) {
  const int kSizeY = width * height * 2;
  rtc::scoped_ptr<uint8_t[]> buffer(new uint8_t[kSizeY]);
  memset(buffer.get(), data, kSizeY);
  I420VideoFrame frame;
  frame.CreateFrame(buffer.get(), buffer.get(), buffer.get(), width, height,
                    width, width / 2, width / 2);
  frame.set_timestamp(data);
  frame.set_render_time_ms(data);
  return frame;
}

TEST_F(VideoSendStreamTest, EncoderIsProperlyInitializedAndDestroyed) {
  class EncoderStateObserver : public test::SendTest, public VideoEncoder {
   public:
    EncoderStateObserver()
        : SendTest(kDefaultTimeoutMs),
          crit_(CriticalSectionWrapper::CreateCriticalSection()),
          initialized_(false),
          callback_registered_(false),
          num_releases_(0),
          released_(false) {}

    bool IsReleased() {
      CriticalSectionScoped lock(crit_.get());
      return released_;
    }

    bool IsReadyForEncode() {
      CriticalSectionScoped lock(crit_.get());
      return initialized_ && callback_registered_;
    }

    size_t num_releases() {
      CriticalSectionScoped lock(crit_.get());
      return num_releases_;
    }

   private:
    int32_t InitEncode(const VideoCodec* codecSettings,
                       int32_t numberOfCores,
                       size_t maxPayloadSize) override {
      CriticalSectionScoped lock(crit_.get());
      EXPECT_FALSE(initialized_);
      initialized_ = true;
      released_ = false;
      return 0;
    }

    int32_t Encode(const I420VideoFrame& inputImage,
                   const CodecSpecificInfo* codecSpecificInfo,
                   const std::vector<VideoFrameType>* frame_types) override {
      EXPECT_TRUE(IsReadyForEncode());

      observation_complete_->Set();
      return 0;
    }

    int32_t RegisterEncodeCompleteCallback(
        EncodedImageCallback* callback) override {
      CriticalSectionScoped lock(crit_.get());
      EXPECT_TRUE(initialized_);
      callback_registered_ = true;
      return 0;
    }

    int32_t Release() override {
      CriticalSectionScoped lock(crit_.get());
      EXPECT_TRUE(IsReadyForEncode());
      EXPECT_FALSE(released_);
      initialized_ = false;
      callback_registered_ = false;
      released_ = true;
      ++num_releases_;
      return 0;
    }

    int32_t SetChannelParameters(uint32_t packetLoss, int64_t rtt) override {
      EXPECT_TRUE(IsReadyForEncode());
      return 0;
    }

    int32_t SetRates(uint32_t newBitRate, uint32_t frameRate) override {
      EXPECT_TRUE(IsReadyForEncode());
      return 0;
    }

    void OnStreamsCreated(
        VideoSendStream* send_stream,
        const std::vector<VideoReceiveStream*>& receive_streams) override {
      // Encoder initialization should be done in stream construction before
      // starting.
      EXPECT_TRUE(IsReadyForEncode());
      stream_ = send_stream;
    }

    void ModifyConfigs(VideoSendStream::Config* send_config,
                       std::vector<VideoReceiveStream::Config>* receive_configs,
                       VideoEncoderConfig* encoder_config) override {
      send_config->encoder_settings.encoder = this;
      encoder_config_ = *encoder_config;
    }

    void PerformTest() override {
      EXPECT_EQ(kEventSignaled, Wait())
          << "Timed out while waiting for Encode.";
      EXPECT_EQ(0u, num_releases());
      stream_->ReconfigureVideoEncoder(encoder_config_);
      EXPECT_EQ(0u, num_releases());
      stream_->Stop();
      // Encoder should not be released before destroying the VideoSendStream.
      EXPECT_FALSE(IsReleased());
      EXPECT_TRUE(IsReadyForEncode());
      stream_->Start();
      // Sanity check, make sure we still encode frames with this encoder.
      EXPECT_EQ(kEventSignaled, Wait())
          << "Timed out while waiting for Encode.";
    }

    rtc::scoped_ptr<CriticalSectionWrapper> crit_;
    VideoSendStream* stream_;
    bool initialized_ GUARDED_BY(crit_);
    bool callback_registered_ GUARDED_BY(crit_);
    size_t num_releases_ GUARDED_BY(crit_);
    bool released_ GUARDED_BY(crit_);
    VideoEncoderConfig encoder_config_;
  } test_encoder;

  RunBaseTest(&test_encoder);

  EXPECT_TRUE(test_encoder.IsReleased());
  EXPECT_EQ(1u, test_encoder.num_releases());
}

TEST_F(VideoSendStreamTest, EncoderSetupPropagatesCommonEncoderConfigValues) {
  class VideoCodecConfigObserver : public test::SendTest,
                                   public test::FakeEncoder {
   public:
    VideoCodecConfigObserver()
        : SendTest(kDefaultTimeoutMs),
          FakeEncoder(Clock::GetRealTimeClock()),
          num_initializations_(0) {}

   private:
    void ModifyConfigs(VideoSendStream::Config* send_config,
                       std::vector<VideoReceiveStream::Config>* receive_configs,
                       VideoEncoderConfig* encoder_config) override {
      send_config->encoder_settings.encoder = this;
      encoder_config_ = *encoder_config;
    }

    void OnStreamsCreated(
        VideoSendStream* send_stream,
        const std::vector<VideoReceiveStream*>& receive_streams) override {
      stream_ = send_stream;
    }

    int32_t InitEncode(const VideoCodec* config,
                       int32_t number_of_cores,
                       size_t max_payload_size) override {
      if (num_initializations_ == 0) {
        // Verify default values.
        EXPECT_EQ(kRealtimeVideo, config->mode);
      } else {
        // Verify that changed values are propagated.
        EXPECT_EQ(kScreensharing, config->mode);
      }
      ++num_initializations_;
      return FakeEncoder::InitEncode(config, number_of_cores, max_payload_size);
    }

    void PerformTest() override {
      EXPECT_EQ(1u, num_initializations_) << "VideoEncoder not initialized.";

      encoder_config_.content_type = VideoEncoderConfig::kScreenshare;
      stream_->ReconfigureVideoEncoder(encoder_config_);
      EXPECT_EQ(2u, num_initializations_)
          << "ReconfigureVideoEncoder did not reinitialize the encoder with "
             "new encoder settings.";
    }

    size_t num_initializations_;
    VideoSendStream* stream_;
    VideoEncoderConfig encoder_config_;
  } test;

  RunBaseTest(&test);
}

static const size_t kVideoCodecConfigObserverNumberOfTemporalLayers = 4;
template <typename T>
class VideoCodecConfigObserver : public test::SendTest,
                                 public test::FakeEncoder {

 public:
  VideoCodecConfigObserver(VideoCodecType video_codec_type,
                           const char* codec_name)
      : SendTest(VideoSendStreamTest::kDefaultTimeoutMs),
        FakeEncoder(Clock::GetRealTimeClock()),
        video_codec_type_(video_codec_type),
        codec_name_(codec_name),
        num_initializations_(0) {
    memset(&encoder_settings_, 0, sizeof(encoder_settings_));
  }

 private:
  void ModifyConfigs(VideoSendStream::Config* send_config,
                     std::vector<VideoReceiveStream::Config>* receive_configs,
                     VideoEncoderConfig* encoder_config) override {
    send_config->encoder_settings.encoder = this;
    send_config->encoder_settings.payload_name = codec_name_;

    for (size_t i = 0; i < encoder_config->streams.size(); ++i) {
      encoder_config->streams[i].temporal_layer_thresholds_bps.resize(
          kVideoCodecConfigObserverNumberOfTemporalLayers - 1);
    }

    encoder_config->encoder_specific_settings = &encoder_settings_;
    encoder_config_ = *encoder_config;
  }

  void OnStreamsCreated(
      VideoSendStream* send_stream,
      const std::vector<VideoReceiveStream*>& receive_streams) override {
    stream_ = send_stream;
  }

  int32_t InitEncode(const VideoCodec* config,
                     int32_t number_of_cores,
                     size_t max_payload_size) override {
    EXPECT_EQ(video_codec_type_, config->codecType);
    VerifyCodecSpecifics(*config);
    ++num_initializations_;
    return FakeEncoder::InitEncode(config, number_of_cores, max_payload_size);
  }

  void VerifyCodecSpecifics(const VideoCodec& config) const;

  void PerformTest() override {
    EXPECT_EQ(1u, num_initializations_) << "VideoEncoder not initialized.";

    encoder_settings_.frameDroppingOn = true;
    stream_->ReconfigureVideoEncoder(encoder_config_);
    EXPECT_EQ(2u, num_initializations_)
        << "ReconfigureVideoEncoder did not reinitialize the encoder with "
           "new encoder settings.";
  }

  int32_t Encode(const I420VideoFrame& input_image,
                 const CodecSpecificInfo* codec_specific_info,
                 const std::vector<VideoFrameType>* frame_types) override {
    // Silently skip the encode, FakeEncoder::Encode doesn't produce VP8.
    return 0;
  }

  T encoder_settings_;
  const VideoCodecType video_codec_type_;
  const char* const codec_name_;
  size_t num_initializations_;
  VideoSendStream* stream_;
  VideoEncoderConfig encoder_config_;
};

template <>
void VideoCodecConfigObserver<VideoCodecH264>::VerifyCodecSpecifics(
    const VideoCodec& config) const {
  EXPECT_EQ(0, memcmp(&config.codecSpecific.H264, &encoder_settings_,
                      sizeof(encoder_settings_)));
}
template <>
void VideoCodecConfigObserver<VideoCodecVP8>::VerifyCodecSpecifics(
    const VideoCodec& config) const {
  // Check that the number of temporal layers has propagated properly to
  // VideoCodec.
  EXPECT_EQ(kVideoCodecConfigObserverNumberOfTemporalLayers,
            config.codecSpecific.VP8.numberOfTemporalLayers);

  for (unsigned char i = 0; i < config.numberOfSimulcastStreams; ++i) {
    EXPECT_EQ(kVideoCodecConfigObserverNumberOfTemporalLayers,
              config.simulcastStream[i].numberOfTemporalLayers);
  }

  // Set expected temporal layers as they should have been set when
  // reconfiguring the encoder and not match the set config.
  VideoCodecVP8 encoder_settings = encoder_settings_;
  encoder_settings.numberOfTemporalLayers =
      kVideoCodecConfigObserverNumberOfTemporalLayers;
  EXPECT_EQ(0, memcmp(&config.codecSpecific.VP8, &encoder_settings,
                      sizeof(encoder_settings_)));
}
template <>
void VideoCodecConfigObserver<VideoCodecVP9>::VerifyCodecSpecifics(
    const VideoCodec& config) const {
  // Check that the number of temporal layers has propagated properly to
  // VideoCodec.
  EXPECT_EQ(kVideoCodecConfigObserverNumberOfTemporalLayers,
            config.codecSpecific.VP9.numberOfTemporalLayers);

  for (unsigned char i = 0; i < config.numberOfSimulcastStreams; ++i) {
    EXPECT_EQ(kVideoCodecConfigObserverNumberOfTemporalLayers,
              config.simulcastStream[i].numberOfTemporalLayers);
  }

  // Set expected temporal layers as they should have been set when
  // reconfiguring the encoder and not match the set config.
  VideoCodecVP9 encoder_settings = encoder_settings_;
  encoder_settings.numberOfTemporalLayers =
      kVideoCodecConfigObserverNumberOfTemporalLayers;
  EXPECT_EQ(0, memcmp(&config.codecSpecific.VP9, &encoder_settings,
                      sizeof(encoder_settings_)));
}

TEST_F(VideoSendStreamTest, EncoderSetupPropagatesVp8Config) {
  VideoCodecConfigObserver<VideoCodecVP8> test(kVideoCodecVP8, "VP8");
  RunBaseTest(&test);
}

TEST_F(VideoSendStreamTest, EncoderSetupPropagatesVp9Config) {
  VideoCodecConfigObserver<VideoCodecVP9> test(kVideoCodecVP9, "VP9");
  RunBaseTest(&test);
}

TEST_F(VideoSendStreamTest, EncoderSetupPropagatesH264Config) {
  VideoCodecConfigObserver<VideoCodecH264> test(kVideoCodecH264, "H264");
  RunBaseTest(&test);
}

TEST_F(VideoSendStreamTest, RtcpSenderReportContainsMediaBytesSent) {
  class RtcpSenderReportTest : public test::SendTest {
   public:
    RtcpSenderReportTest() : SendTest(kDefaultTimeoutMs),
                             rtp_packets_sent_(0),
                             media_bytes_sent_(0) {}

   private:
    Action OnSendRtp(const uint8_t* packet, size_t length) override {
      RTPHeader header;
      EXPECT_TRUE(parser_->Parse(packet, length, &header));
      ++rtp_packets_sent_;
      media_bytes_sent_ += length - header.headerLength - header.paddingLength;
      return SEND_PACKET;
    }

    Action OnSendRtcp(const uint8_t* packet, size_t length) override {
      RTCPUtility::RTCPParserV2 parser(packet, length, true);
      EXPECT_TRUE(parser.IsValid());

      RTCPUtility::RTCPPacketTypes packet_type = parser.Begin();
      while (packet_type != RTCPUtility::kRtcpNotValidCode) {
        if (packet_type == RTCPUtility::kRtcpSrCode) {
          // Only compare sent media bytes if SenderPacketCount matches the
          // number of sent rtp packets (a new rtp packet could be sent before
          // the rtcp packet).
          if (parser.Packet().SR.SenderOctetCount > 0 &&
              parser.Packet().SR.SenderPacketCount == rtp_packets_sent_) {
            EXPECT_EQ(media_bytes_sent_, parser.Packet().SR.SenderOctetCount);
            observation_complete_->Set();
          }
        }
        packet_type = parser.Iterate();
      }

      return SEND_PACKET;
    }

    void PerformTest() override {
      EXPECT_EQ(kEventSignaled, Wait())
          << "Timed out while waiting for RTCP sender report.";
    }

    size_t rtp_packets_sent_;
    size_t media_bytes_sent_;
  } test;

  RunBaseTest(&test);
}

TEST_F(VideoSendStreamTest, TranslatesTwoLayerScreencastToTargetBitrate) {
  static const int kScreencastTargetBitrateKbps = 200;
  class ScreencastTargetBitrateTest : public test::SendTest,
                                      public test::FakeEncoder {
   public:
    ScreencastTargetBitrateTest()
        : SendTest(kDefaultTimeoutMs),
          test::FakeEncoder(Clock::GetRealTimeClock()) {}

   private:
    int32_t InitEncode(const VideoCodec* config,
                       int32_t number_of_cores,
                       size_t max_payload_size) override {
      EXPECT_EQ(static_cast<unsigned int>(kScreencastTargetBitrateKbps),
                config->targetBitrate);
      observation_complete_->Set();
      return test::FakeEncoder::InitEncode(
          config, number_of_cores, max_payload_size);
    }
    void ModifyConfigs(VideoSendStream::Config* send_config,
                       std::vector<VideoReceiveStream::Config>* receive_configs,
                       VideoEncoderConfig* encoder_config) override {
      send_config->encoder_settings.encoder = this;
      EXPECT_EQ(1u, encoder_config->streams.size());
      EXPECT_TRUE(
          encoder_config->streams[0].temporal_layer_thresholds_bps.empty());
      encoder_config->streams[0].temporal_layer_thresholds_bps.push_back(
          kScreencastTargetBitrateKbps * 1000);
      encoder_config->content_type = VideoEncoderConfig::kScreenshare;
    }

    void PerformTest() override {
      EXPECT_EQ(kEventSignaled, Wait())
          << "Timed out while waiting for the encoder to be initialized.";
    }
  } test;

  RunBaseTest(&test);
}

TEST_F(VideoSendStreamTest, ReconfigureBitratesSetsEncoderBitratesCorrectly) {
  // These are chosen to be "kind of odd" to not be accidentally checked against
  // default values.
  static const int kMinBitrateKbps = 137;
  static const int kStartBitrateKbps = 345;
  static const int kLowerMaxBitrateKbps = 312;
  static const int kMaxBitrateKbps = 413;
  static const int kIncreasedStartBitrateKbps = 451;
  static const int kIncreasedMaxBitrateKbps = 597;
  class EncoderBitrateThresholdObserver : public test::SendTest,
                                          public test::FakeEncoder {
   public:
    EncoderBitrateThresholdObserver()
        : SendTest(kDefaultTimeoutMs),
          FakeEncoder(Clock::GetRealTimeClock()),
          num_initializations_(0) {}

   private:
    int32_t InitEncode(const VideoCodec* codecSettings,
                       int32_t numberOfCores,
                       size_t maxPayloadSize) override {
      if (num_initializations_ == 0) {
        EXPECT_EQ(static_cast<unsigned int>(kMinBitrateKbps),
                  codecSettings->minBitrate);
        EXPECT_EQ(static_cast<unsigned int>(kStartBitrateKbps),
                  codecSettings->startBitrate);
        EXPECT_EQ(static_cast<unsigned int>(kMaxBitrateKbps),
                  codecSettings->maxBitrate);
        observation_complete_->Set();
      } else if (num_initializations_ == 1) {
        EXPECT_EQ(static_cast<unsigned int>(kLowerMaxBitrateKbps),
                  codecSettings->maxBitrate);
        // The start bitrate should be kept (-1) and capped to the max bitrate.
        // Since this is not an end-to-end call no receiver should have been
        // returning a REMB that could lower this estimate.
        EXPECT_EQ(codecSettings->startBitrate, codecSettings->maxBitrate);
      } else if (num_initializations_ == 2) {
        EXPECT_EQ(static_cast<unsigned int>(kIncreasedMaxBitrateKbps),
                  codecSettings->maxBitrate);
        EXPECT_EQ(static_cast<unsigned int>(kIncreasedStartBitrateKbps),
                  codecSettings->startBitrate);
      }
      ++num_initializations_;
      return FakeEncoder::InitEncode(codecSettings, numberOfCores,
                                     maxPayloadSize);
    }

    Call::Config GetSenderCallConfig() override {
      Call::Config config(SendTransport());
      config.bitrate_config.min_bitrate_bps = kMinBitrateKbps * 1000;
      config.bitrate_config.start_bitrate_bps = kStartBitrateKbps * 1000;
      config.bitrate_config.max_bitrate_bps = kMaxBitrateKbps * 1000;
      return config;
    }

    void ModifyConfigs(VideoSendStream::Config* send_config,
                       std::vector<VideoReceiveStream::Config>* receive_configs,
                       VideoEncoderConfig* encoder_config) override {
      send_config->encoder_settings.encoder = this;
      // Set bitrates lower/higher than min/max to make sure they are properly
      // capped.
      encoder_config->streams.front().min_bitrate_bps = kMinBitrateKbps * 1000;
      encoder_config->streams.front().max_bitrate_bps = kMaxBitrateKbps * 1000;
      encoder_config_ = *encoder_config;
    }

    void OnCallsCreated(Call* sender_call, Call* receiver_call) override {
      call_ = sender_call;
    }

    void OnStreamsCreated(
        VideoSendStream* send_stream,
        const std::vector<VideoReceiveStream*>& receive_streams) override {
      send_stream_ = send_stream;
    }

    void PerformTest() override {
      Call::Config::BitrateConfig bitrate_config;
      bitrate_config.start_bitrate_bps = kIncreasedStartBitrateKbps * 1000;
      bitrate_config.max_bitrate_bps = kIncreasedMaxBitrateKbps * 1000;
      call_->SetBitrateConfig(bitrate_config);
      EXPECT_EQ(kEventSignaled, Wait())
          << "Timed out while waiting encoder to be configured.";
      encoder_config_.streams[0].min_bitrate_bps = 0;
      encoder_config_.streams[0].max_bitrate_bps = kLowerMaxBitrateKbps * 1000;
      send_stream_->ReconfigureVideoEncoder(encoder_config_);
      EXPECT_EQ(2, num_initializations_)
          << "Encoder should have been reconfigured with the new value.";
      encoder_config_.streams[0].target_bitrate_bps =
          encoder_config_.streams[0].min_bitrate_bps;
      encoder_config_.streams[0].max_bitrate_bps =
          kIncreasedMaxBitrateKbps * 1000;
      send_stream_->ReconfigureVideoEncoder(encoder_config_);
      EXPECT_EQ(3, num_initializations_)
          << "Encoder should have been reconfigured with the new value.";
    }

    int num_initializations_;
    webrtc::Call* call_;
    webrtc::VideoSendStream* send_stream_;
    webrtc::VideoEncoderConfig encoder_config_;
  } test;

  RunBaseTest(&test);
}

TEST_F(VideoSendStreamTest, ReportsSentResolution) {
  static const size_t kNumStreams = 3;
  // Unusual resolutions to make sure that they are the ones being reported.
  static const struct {
    int width;
    int height;
  } kEncodedResolution[kNumStreams] = {
      {241, 181}, {300, 121}, {121, 221}};
  class ScreencastTargetBitrateTest : public test::SendTest,
                                      public test::FakeEncoder {
   public:
    ScreencastTargetBitrateTest()
        : SendTest(kDefaultTimeoutMs),
          test::FakeEncoder(Clock::GetRealTimeClock()) {}

   private:
    int32_t Encode(const I420VideoFrame& input_image,
                   const CodecSpecificInfo* codecSpecificInfo,
                   const std::vector<VideoFrameType>* frame_types) override {
      CodecSpecificInfo specifics;
      memset(&specifics, 0, sizeof(specifics));
      specifics.codecType = kVideoCodecGeneric;

      uint8_t buffer[16] = {0};
      EncodedImage encoded(buffer, sizeof(buffer), sizeof(buffer));
      encoded._timeStamp = input_image.timestamp();
      encoded.capture_time_ms_ = input_image.render_time_ms();

      for (size_t i = 0; i < kNumStreams; ++i) {
        specifics.codecSpecific.generic.simulcast_idx = static_cast<uint8_t>(i);
        encoded._frameType = (*frame_types)[i];
        encoded._encodedWidth = kEncodedResolution[i].width;
        encoded._encodedHeight = kEncodedResolution[i].height;
        DCHECK(callback_ != nullptr);
        if (callback_->Encoded(encoded, &specifics, nullptr) != 0)
          return -1;
      }

      observation_complete_->Set();
      return 0;
    }
    void ModifyConfigs(VideoSendStream::Config* send_config,
                       std::vector<VideoReceiveStream::Config>* receive_configs,
                       VideoEncoderConfig* encoder_config) override {
      send_config->encoder_settings.encoder = this;
      EXPECT_EQ(kNumStreams, encoder_config->streams.size());
    }

    size_t GetNumStreams() const override { return kNumStreams; }

    void PerformTest() override {
      EXPECT_EQ(kEventSignaled, Wait())
          << "Timed out while waiting for the encoder to send one frame.";
      VideoSendStream::Stats stats = send_stream_->GetStats();

      for (size_t i = 0; i < kNumStreams; ++i) {
        ASSERT_TRUE(stats.substreams.find(kSendSsrcs[i]) !=
                    stats.substreams.end())
            << "No stats for SSRC: " << kSendSsrcs[i]
            << ", stats should exist as soon as frames have been encoded.";
        VideoSendStream::StreamStats ssrc_stats =
            stats.substreams[kSendSsrcs[i]];
        EXPECT_EQ(kEncodedResolution[i].width, ssrc_stats.width);
        EXPECT_EQ(kEncodedResolution[i].height, ssrc_stats.height);
      }
    }

    void OnStreamsCreated(
        VideoSendStream* send_stream,
        const std::vector<VideoReceiveStream*>& receive_streams) override {
      send_stream_ = send_stream;
    }

    VideoSendStream* send_stream_;
  } test;

  RunBaseTest(&test);
}
}  // namespace webrtc
