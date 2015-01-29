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

#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/call.h"
#include "webrtc/common_video/interface/i420_video_frame.h"
#include "webrtc/common_video/interface/native_handle.h"
#include "webrtc/common_video/interface/texture_video_frame.h"
#include "webrtc/frame_callback.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_header_parser.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_sender.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_utility.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/event_wrapper.h"
#include "webrtc/system_wrappers/interface/ref_count.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/system_wrappers/interface/scoped_vector.h"
#include "webrtc/system_wrappers/interface/sleep.h"
#include "webrtc/system_wrappers/interface/thread_wrapper.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/test/call_test.h"
#include "webrtc/test/configurable_frame_size_encoder.h"
#include "webrtc/test/null_transport.h"
#include "webrtc/test/testsupport/perf_test.h"
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
void ExpectEqualFramesVector(const std::vector<I420VideoFrame*>& frames1,
                             const std::vector<I420VideoFrame*>& frames2);
I420VideoFrame* CreateI420VideoFrame(int width, int height, uint8_t data);

class FakeNativeHandle : public NativeHandle {
 public:
  FakeNativeHandle() {}
  virtual ~FakeNativeHandle() {}
  virtual void* GetHandle() { return NULL; }
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
    virtual Action OnSendRtcp(const uint8_t* packet, size_t length) OVERRIDE {
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

    virtual void ModifyConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) OVERRIDE {
      send_config->rtp.c_name = kCName;
    }

    virtual void PerformTest() OVERRIDE {
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

    virtual Action OnSendRtp(const uint8_t* packet, size_t length) OVERRIDE {
      RTPHeader header;
      EXPECT_TRUE(parser_->Parse(packet, length, &header));

      EXPECT_FALSE(header.extension.hasTransmissionTimeOffset);
      EXPECT_TRUE(header.extension.hasAbsoluteSendTime);
      EXPECT_EQ(header.extension.transmissionTimeOffset, 0);
      EXPECT_GT(header.extension.absoluteSendTime, 0u);
      observation_complete_->Set();

      return SEND_PACKET;
    }

    virtual void ModifyConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) OVERRIDE {
      send_config->rtp.extensions.push_back(
          RtpExtension(RtpExtension::kAbsSendTime, kAbsSendTimeExtensionId));
    }

    virtual void PerformTest() OVERRIDE {
      EXPECT_EQ(kEventSignaled, Wait())
          << "Timed out while waiting for single RTP packet.";
    }
  } test;

  RunBaseTest(&test);
}

TEST_F(VideoSendStreamTest, SupportsTransmissionTimeOffset) {
  static const uint8_t kTOffsetExtensionId = 13;
  class TransmissionTimeOffsetObserver : public test::SendTest {
   public:
    TransmissionTimeOffsetObserver()
        : SendTest(kDefaultTimeoutMs), encoder_(Clock::GetRealTimeClock()) {
      EXPECT_TRUE(parser_->RegisterRtpHeaderExtension(
          kRtpExtensionTransmissionTimeOffset, kTOffsetExtensionId));
    }

   private:
    virtual Action OnSendRtp(const uint8_t* packet, size_t length) OVERRIDE {
      RTPHeader header;
      EXPECT_TRUE(parser_->Parse(packet, length, &header));

      EXPECT_TRUE(header.extension.hasTransmissionTimeOffset);
      EXPECT_FALSE(header.extension.hasAbsoluteSendTime);
      EXPECT_GT(header.extension.transmissionTimeOffset, 0);
      EXPECT_EQ(header.extension.absoluteSendTime, 0u);
      observation_complete_->Set();

      return SEND_PACKET;
    }

    virtual void ModifyConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) OVERRIDE {
      send_config->encoder_settings.encoder = &encoder_;
      send_config->rtp.extensions.push_back(
          RtpExtension(RtpExtension::kTOffset, kTOffsetExtensionId));
    }

    virtual void PerformTest() OVERRIDE {
      EXPECT_EQ(kEventSignaled, Wait())
          << "Timed out while waiting for a single RTP packet.";
    }

    class DelayedEncoder : public test::FakeEncoder {
     public:
      explicit DelayedEncoder(Clock* clock) : test::FakeEncoder(clock) {}
      virtual int32_t Encode(
          const I420VideoFrame& input_image,
          const CodecSpecificInfo* codec_specific_info,
          const std::vector<VideoFrameType>* frame_types) OVERRIDE {
        // A delay needs to be introduced to assure that we get a timestamp
        // offset.
        SleepMs(5);
        return FakeEncoder::Encode(
            input_image, codec_specific_info, frame_types);
      }
    };

    DelayedEncoder encoder_;
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

  virtual StatisticianMap GetActiveStatisticians() const OVERRIDE {
    return stats_map_;
  }

  virtual StreamStatistician* GetStatistician(uint32_t ssrc) const OVERRIDE {
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
    virtual bool GetStatistics(RtcpStatistics* statistics,
                               bool reset) OVERRIDE {
      *statistics = stats_;
      return true;
    }
    virtual void GetDataCounters(uint32_t* bytes_received,
                                 uint32_t* packets_received) const OVERRIDE {
      *bytes_received = 0;
      *packets_received = 0;
    }
    virtual uint32_t BitrateReceived() const OVERRIDE { return 0; }
    virtual void ResetStatistics() OVERRIDE {}
    virtual bool IsRetransmitOfOldPacket(const RTPHeader& header,
                                         int min_rtt) const OVERRIDE {
      return false;
    }

    virtual bool IsPacketInOrder(uint16_t sequence_number) const OVERRIDE {
      return true;
    }

    RtcpStatistics stats_;
  };

  scoped_ptr<LossyStatistician> lossy_stats_;
  StatisticianMap stats_map_;
};

TEST_F(VideoSendStreamTest, SwapsI420VideoFrames) {
  static const size_t kWidth = 320;
  static const size_t kHeight = 240;

  test::NullTransport transport;
  Call::Config call_config(&transport);
  CreateSenderCall(call_config);

  CreateSendConfig(1);
  CreateStreams();
  send_stream_->Start();

  I420VideoFrame frame;
  frame.CreateEmptyFrame(
      kWidth, kHeight, kWidth, (kWidth + 1) / 2, (kWidth + 1) / 2);
  uint8_t* old_y_buffer = frame.buffer(kYPlane);

  send_stream_->Input()->SwapFrame(&frame);

  EXPECT_NE(frame.buffer(kYPlane), old_y_buffer);

  DestroyStreams();
}

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
    virtual Action OnSendRtp(const uint8_t* packet, size_t length) OVERRIDE {
      RTPHeader header;
      EXPECT_TRUE(parser_->Parse(packet, length, &header));

      // Send lossy receive reports to trigger FEC enabling.
      if (send_count_++ % 2 != 0) {
        // Receive statistics reporting having lost 50% of the packets.
        FakeReceiveStatistics lossy_receive_stats(
            kSendSsrcs[0], header.sequenceNumber, send_count_ / 2, 127);
        RTCPSender rtcp_sender(
            0, false, Clock::GetRealTimeClock(), &lossy_receive_stats);
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

    virtual void ModifyConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) OVERRIDE {
      send_config->rtp.fec.red_payload_type = kRedPayloadType;
      send_config->rtp.fec.ulpfec_payload_type = kUlpfecPayloadType;
    }

    virtual void PerformTest() OVERRIDE {
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
    virtual Action OnSendRtp(const uint8_t* packet, size_t length) OVERRIDE {
      RTPHeader header;
      EXPECT_TRUE(parser_->Parse(packet, length, &header));

      // Nack second packet after receiving the third one.
      if (++send_count_ == 3) {
        uint16_t nack_sequence_number = header.sequenceNumber - 1;
        nacked_sequence_number_ = nack_sequence_number;
        NullReceiveStatistics null_stats;
        RTCPSender rtcp_sender(
            0, false, Clock::GetRealTimeClock(), &null_stats);
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

    virtual void ModifyConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) OVERRIDE {
      send_config->rtp.nack.rtp_history_ms = kNackRtpHistoryMs;
      send_config->rtp.rtx.payload_type = retransmit_payload_type_;
      if (retransmit_ssrc_ != kSendSsrcs[0])
        send_config->rtp.rtx.ssrcs.push_back(retransmit_ssrc_);
    }

    virtual void PerformTest() OVERRIDE {
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
  static const uint32_t kMaxPacketSize = 128;
  static const uint32_t start = 90;
  static const uint32_t stop = 290;

  // Observer that verifies that the expected number of packets and bytes
  // arrive for each frame size, from start_size to stop_size.
  class FrameFragmentationTest : public test::SendTest,
                                 public EncodedFrameObserver {
   public:
    FrameFragmentationTest(uint32_t max_packet_size,
                           uint32_t start_size,
                           uint32_t stop_size,
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
          current_size_frame_(start_size) {
      // Fragmentation required, this test doesn't make sense without it.
      encoder_.SetFrameSize(start);
      assert(stop_size > max_packet_size);
      transport_adapter_.Enable();
    }

   private:
    virtual Action OnSendRtp(const uint8_t* packet, size_t size) OVERRIDE {
      uint32_t length = static_cast<int>(size);
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
        uint32_t overhead = header.headerLength + header.paddingLength +
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
        RTCPSender rtcp_sender(
            0, false, Clock::GetRealTimeClock(), &lossy_receive_stats);
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
      encoder_.SetFrameSize(current_size_frame_.Value());
    }

    virtual void ModifyConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) OVERRIDE {
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

    virtual void PerformTest() OVERRIDE {
      EXPECT_EQ(kEventSignaled, Wait())
          << "Timed out while observing incoming RTP packets.";
    }

    internal::TransportAdapter transport_adapter_;
    test::ConfigurableFrameSizeEncoder encoder_;

    const uint32_t max_packet_size_;
    const uint32_t stop_size_;
    const bool test_generic_packetization_;
    const bool use_fec_;

    uint32_t packet_count_;
    uint32_t accumulated_size_;
    uint32_t accumulated_payload_;
    bool fec_packet_received_;

    uint32_t current_size_rtp_;
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
    virtual Action OnSendRtcp(const uint8_t* packet, size_t length) OVERRIDE {
      // Receive statistics reporting having lost 0% of the packets.
      // This is needed for the send-side bitrate controller to work properly.
      CriticalSectionScoped lock(crit_.get());
      SendRtcpFeedback(0);  // REMB is only sent if value is > 0.
      return SEND_PACKET;
    }

    virtual Action OnSendRtp(const uint8_t* packet, size_t length) OVERRIDE {
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
    void FrameCallback(I420VideoFrame* video_frame) OVERRIDE {
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

    virtual void SetReceivers(
        PacketReceiver* send_transport_receiver,
        PacketReceiver* receive_transport_receiver) OVERRIDE {
      transport_.SetReceiver(send_transport_receiver);
    }

    virtual void OnStreamsCreated(
        VideoSendStream* send_stream,
        const std::vector<VideoReceiveStream*>& receive_streams) OVERRIDE {
      stream_ = send_stream;
    }

    virtual void ModifyConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) OVERRIDE {
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

    virtual void PerformTest() OVERRIDE {
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
      RTCPSender rtcp_sender(0, false, clock_, &receive_stats);
      EXPECT_EQ(0, rtcp_sender.RegisterSendTransport(&transport_adapter_));

      rtcp_sender.SetRTCPStatus(kRtcpNonCompound);
      rtcp_sender.SetRemoteSSRC(kSendSsrcs[0]);
      if (remb_value > 0) {
        rtcp_sender.SetREMBStatus(true);
        rtcp_sender.SetREMBData(remb_value, 0, NULL);
      }
      RTCPSender::FeedbackState feedback_state;
      EXPECT_EQ(0, rtcp_sender.SendRTCP(feedback_state, kRtcpRr));
    }

    internal::TransportAdapter transport_adapter_;
    test::DirectTransport transport_;
    Clock* const clock_;
    VideoSendStream* stream_;

    const scoped_ptr<CriticalSectionWrapper> crit_;
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
          capturer_(NULL) {
      transport_adapter_.Enable();
    }

   private:
    virtual Action OnSendRtp(const uint8_t* packet, size_t length) OVERRIDE {
      CriticalSectionScoped lock(crit_.get());
      last_packet_time_ms_ = clock_->TimeInMilliseconds();
      capturer_->Stop();
      return SEND_PACKET;
    }

    virtual Action OnSendRtcp(const uint8_t* packet, size_t length) OVERRIDE {
      CriticalSectionScoped lock(crit_.get());
      const int kVideoMutedThresholdMs = 10000;
      if (last_packet_time_ms_ > 0 &&
          clock_->TimeInMilliseconds() - last_packet_time_ms_ >
              kVideoMutedThresholdMs)
        observation_complete_->Set();
      // Receive statistics reporting having lost 50% of the packets.
      FakeReceiveStatistics receive_stats(kSendSsrcs[0], 1, 1, 0);
      RTCPSender rtcp_sender(
          0, false, Clock::GetRealTimeClock(), &receive_stats);
      EXPECT_EQ(0, rtcp_sender.RegisterSendTransport(&transport_adapter_));

      rtcp_sender.SetRTCPStatus(kRtcpNonCompound);
      rtcp_sender.SetRemoteSSRC(kSendSsrcs[0]);

      RTCPSender::FeedbackState feedback_state;

      EXPECT_EQ(0, rtcp_sender.SendRTCP(feedback_state, kRtcpRr));
      return SEND_PACKET;
    }

    virtual void SetReceivers(
        PacketReceiver* send_transport_receiver,
        PacketReceiver* receive_transport_receiver) OVERRIDE {
      RtpRtcpObserver::SetReceivers(send_transport_receiver,
                                    send_transport_receiver);
    }

    virtual size_t GetNumStreams() const OVERRIDE { return 3; }

    virtual void OnFrameGeneratorCapturerCreated(
        test::FrameGeneratorCapturer* frame_generator_capturer) {
      CriticalSectionScoped lock(crit_.get());
      capturer_ = frame_generator_capturer;
    }

    virtual void PerformTest() OVERRIDE {
      EXPECT_EQ(kEventSignaled, Wait())
          << "Timed out while waiting for RTP packets to stop being sent.";
    }

    Clock* const clock_;
    internal::TransportAdapter transport_adapter_;
    const scoped_ptr<CriticalSectionWrapper> crit_;
    int64_t last_packet_time_ms_ GUARDED_BY(crit_);
    test::FrameGeneratorCapturer* capturer_ GUARDED_BY(crit_);
  } test;

  RunBaseTest(&test);
}

TEST_F(VideoSendStreamTest, ProducesStats) {
  class ProducesStats : public test::SendTest {
   public:
    ProducesStats()
        : SendTest(kDefaultTimeoutMs),
          stream_(NULL),
          event_(EventWrapper::Create()) {}

    virtual Action OnSendRtcp(const uint8_t* packet, size_t length) OVERRIDE {
      event_->Set();

      return SEND_PACKET;
    }

   private:
    bool WaitForFilledStats() {
      Clock* clock = Clock::GetRealTimeClock();
      int64_t now = clock->TimeInMilliseconds();
      int64_t stop_time = now + kDefaultTimeoutMs;
      while (now < stop_time) {
        int64_t time_left = stop_time - now;
        if (time_left > 0 && event_->Wait(time_left) == kEventSignaled &&
            CheckStats()) {
          return true;
        }
        now = clock->TimeInMilliseconds();
      }
      return false;
    }

    bool CheckStats() {
      VideoSendStream::Stats stats = stream_->GetStats();
      // Check that all applicable data sources have been used.
      if (stats.input_frame_rate > 0 && stats.encode_frame_rate > 0
          && !stats.substreams.empty()) {
        uint32_t ssrc = stats.substreams.begin()->first;
        EXPECT_NE(
            config_.rtp.ssrcs.end(),
            std::find(
                config_.rtp.ssrcs.begin(), config_.rtp.ssrcs.end(), ssrc));
        // Check for data populated by various sources. RTCP excluded as this
        // data is received from remote side. Tested in call tests instead.
        const SsrcStats& entry = stats.substreams[ssrc];
        if (entry.key_frames > 0u && entry.total_bitrate_bps > 0 &&
            entry.rtp_stats.packets > 0u && entry.avg_delay_ms > 0 &&
            entry.max_delay_ms > 0) {
          return true;
        }
      }
      return false;
    }

    void SetConfig(const VideoSendStream::Config& config) { config_ = config; }

    virtual void ModifyConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) OVERRIDE {
      SetConfig(*send_config);
    }

    virtual void OnStreamsCreated(
        VideoSendStream* send_stream,
        const std::vector<VideoReceiveStream*>& receive_streams) OVERRIDE {
      stream_ = send_stream;
    }

    virtual void PerformTest() OVERRIDE {
      EXPECT_TRUE(WaitForFilledStats())
          << "Timed out waiting for filled statistics.";
    }

    VideoSendStream* stream_;
    VideoSendStream::Config config_;
    scoped_ptr<EventWrapper> event_;
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

    virtual void OnStreamsCreated(
        VideoSendStream* send_stream,
        const std::vector<VideoReceiveStream*>& receive_streams) OVERRIDE {
      stream_ = send_stream;
    }

   private:
    virtual DeliveryStatus DeliverPacket(const uint8_t* packet,
                                         size_t length) OVERRIDE {
      if (RtpHeaderParser::IsRtcp(packet, length))
        return DELIVERY_OK;

      RTPHeader header;
      if (!parser_->Parse(packet, length, &header))
        return DELIVERY_PACKET_ERROR;
      assert(stream_ != NULL);
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
          rtp_rtcp_->SetREMBData(kRembBitrateBps, 1, &header.ssrc);
          rtp_rtcp_->Process();
          bitrate_capped_ = true;
        } else if (bitrate_capped_ &&
                   total_bitrate_bps < kRembRespectedBitrateBps) {
          observation_complete_->Set();
        }
      }
      return DELIVERY_OK;
    }

    virtual void SetReceivers(
        PacketReceiver* send_transport_receiver,
        PacketReceiver* receive_transport_receiver) OVERRIDE {
      RtpRtcpObserver::SetReceivers(this, send_transport_receiver);
    }

    virtual void ModifyConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) OVERRIDE {
      encoder_config->min_transmit_bitrate_bps = kMinTransmitBitrateBps;
    }

    virtual void PerformTest() OVERRIDE {
      EXPECT_EQ(kEventSignaled, Wait())
          << "Timeout while waiting for low bitrate stats after REMB.";
    }

    scoped_ptr<RtpRtcp> rtp_rtcp_;
    internal::TransportAdapter feedback_transport_;
    VideoSendStream* stream_;
    bool bitrate_capped_;
  } test;

  RunBaseTest(&test);
}

TEST_F(VideoSendStreamTest, CapturesTextureAndI420VideoFrames) {
  class FrameObserver : public I420FrameCallback {
   public:
    FrameObserver() : output_frame_event_(EventWrapper::Create()) {}

    void FrameCallback(I420VideoFrame* video_frame) OVERRIDE {
      // Clone the frame because the caller owns it.
      output_frames_.push_back(video_frame->CloneFrame());
      output_frame_event_->Set();
    }

    void WaitOutputFrame() {
      const unsigned long kWaitFrameTimeoutMs = 3000;
      EXPECT_EQ(kEventSignaled, output_frame_event_->Wait(kWaitFrameTimeoutMs))
          << "Timeout while waiting for output frames.";
    }

    const std::vector<I420VideoFrame*>& output_frames() const {
      return output_frames_.get();
    }

   private:
    // Delivered output frames.
    ScopedVector<I420VideoFrame> output_frames_;

    // Indicate an output frame has arrived.
    scoped_ptr<EventWrapper> output_frame_event_;
  };

  // Initialize send stream.
  test::NullTransport transport;
  CreateSenderCall(Call::Config(&transport));

  CreateSendConfig(1);
  FrameObserver observer;
  send_config_.pre_encode_callback = &observer;
  CreateStreams();

  // Prepare five input frames. Send I420VideoFrame and TextureVideoFrame
  // alternatively.
  ScopedVector<I420VideoFrame> input_frames;
  int width = static_cast<int>(encoder_config_.streams[0].width);
  int height = static_cast<int>(encoder_config_.streams[0].height);
  webrtc::RefCountImpl<FakeNativeHandle>* handle1 =
      new webrtc::RefCountImpl<FakeNativeHandle>();
  webrtc::RefCountImpl<FakeNativeHandle>* handle2 =
      new webrtc::RefCountImpl<FakeNativeHandle>();
  webrtc::RefCountImpl<FakeNativeHandle>* handle3 =
      new webrtc::RefCountImpl<FakeNativeHandle>();
  input_frames.push_back(new TextureVideoFrame(handle1, width, height, 1, 1));
  input_frames.push_back(new TextureVideoFrame(handle2, width, height, 2, 2));
  input_frames.push_back(CreateI420VideoFrame(width, height, 1));
  input_frames.push_back(CreateI420VideoFrame(width, height, 2));
  input_frames.push_back(new TextureVideoFrame(handle3, width, height, 3, 3));

  send_stream_->Start();
  for (size_t i = 0; i < input_frames.size(); i++) {
    // Make a copy of the input frame because the buffer will be swapped.
    scoped_ptr<I420VideoFrame> frame(input_frames[i]->CloneFrame());
    send_stream_->Input()->SwapFrame(frame.get());
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
  ExpectEqualFramesVector(input_frames.get(), observer.output_frames());

  DestroyStreams();
}

void ExpectEqualFrames(const I420VideoFrame& frame1,
                       const I420VideoFrame& frame2) {
  if (frame1.native_handle() != NULL || frame2.native_handle() != NULL)
    ExpectEqualTextureFrames(frame1, frame2);
  else
    ExpectEqualBufferFrames(frame1, frame2);
}

void ExpectEqualTextureFrames(const I420VideoFrame& frame1,
                              const I420VideoFrame& frame2) {
  EXPECT_EQ(frame1.native_handle(), frame2.native_handle());
  EXPECT_EQ(frame1.width(), frame2.width());
  EXPECT_EQ(frame1.height(), frame2.height());
}

void ExpectEqualBufferFrames(const I420VideoFrame& frame1,
                             const I420VideoFrame& frame2) {
  EXPECT_EQ(frame1.width(), frame2.width());
  EXPECT_EQ(frame1.height(), frame2.height());
  EXPECT_EQ(frame1.stride(kYPlane), frame2.stride(kYPlane));
  EXPECT_EQ(frame1.stride(kUPlane), frame2.stride(kUPlane));
  EXPECT_EQ(frame1.stride(kVPlane), frame2.stride(kVPlane));
  EXPECT_EQ(frame1.ntp_time_ms(), frame2.ntp_time_ms());
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

void ExpectEqualFramesVector(const std::vector<I420VideoFrame*>& frames1,
                             const std::vector<I420VideoFrame*>& frames2) {
  EXPECT_EQ(frames1.size(), frames2.size());
  for (size_t i = 0; i < std::min(frames1.size(), frames2.size()); ++i)
    ExpectEqualFrames(*frames1[i], *frames2[i]);
}

I420VideoFrame* CreateI420VideoFrame(int width, int height, uint8_t data) {
  I420VideoFrame* frame = new I420VideoFrame();
  const int kSizeY = width * height * 2;
  const int kSizeUV = width * height;
  scoped_ptr<uint8_t[]> buffer(new uint8_t[kSizeY]);
  memset(buffer.get(), data, kSizeY);
  frame->CreateFrame(kSizeY,
                     buffer.get(),
                     kSizeUV,
                     buffer.get(),
                     kSizeUV,
                     buffer.get(),
                     width,
                     height,
                     width,
                     width / 2,
                     width / 2);
  frame->set_timestamp(data);
  frame->set_ntp_time_ms(data);
  frame->set_render_time_ms(data);
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
    virtual int32_t InitEncode(const VideoCodec* codecSettings,
                               int32_t numberOfCores,
                               uint32_t maxPayloadSize) OVERRIDE {
      CriticalSectionScoped lock(crit_.get());
      EXPECT_FALSE(initialized_);
      initialized_ = true;
      released_ = false;
      return 0;
    }

    virtual int32_t Encode(
        const I420VideoFrame& inputImage,
        const CodecSpecificInfo* codecSpecificInfo,
        const std::vector<VideoFrameType>* frame_types) OVERRIDE {
      EXPECT_TRUE(IsReadyForEncode());

      observation_complete_->Set();
      return 0;
    }

    virtual int32_t RegisterEncodeCompleteCallback(
        EncodedImageCallback* callback) OVERRIDE {
      CriticalSectionScoped lock(crit_.get());
      EXPECT_TRUE(initialized_);
      callback_registered_ = true;
      return 0;
    }

    virtual int32_t Release() OVERRIDE {
      CriticalSectionScoped lock(crit_.get());
      EXPECT_TRUE(IsReadyForEncode());
      EXPECT_FALSE(released_);
      initialized_ = false;
      callback_registered_ = false;
      released_ = true;
      ++num_releases_;
      return 0;
    }

    virtual int32_t SetChannelParameters(uint32_t packetLoss,
                                         int rtt) OVERRIDE {
      EXPECT_TRUE(IsReadyForEncode());
      return 0;
    }

    virtual int32_t SetRates(uint32_t newBitRate, uint32_t frameRate) OVERRIDE {
      EXPECT_TRUE(IsReadyForEncode());
      return 0;
    }

    virtual void OnStreamsCreated(
        VideoSendStream* send_stream,
        const std::vector<VideoReceiveStream*>& receive_streams) OVERRIDE {
      // Encoder initialization should be done in stream construction before
      // starting.
      EXPECT_TRUE(IsReadyForEncode());
      stream_ = send_stream;
    }

    virtual void ModifyConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) OVERRIDE {
      send_config->encoder_settings.encoder = this;
      encoder_config_ = *encoder_config;
    }

    virtual void PerformTest() OVERRIDE {
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

    scoped_ptr<CriticalSectionWrapper> crit_;
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
    virtual void ModifyConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) OVERRIDE {
      send_config->encoder_settings.encoder = this;
      encoder_config_ = *encoder_config;
    }

    virtual void OnStreamsCreated(
        VideoSendStream* send_stream,
        const std::vector<VideoReceiveStream*>& receive_streams) OVERRIDE {
      stream_ = send_stream;
    }

    virtual int32_t InitEncode(const VideoCodec* config,
                               int32_t number_of_cores,
                               uint32_t max_payload_size) OVERRIDE {
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

    virtual void PerformTest() OVERRIDE {
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

TEST_F(VideoSendStreamTest, EncoderSetupPropagatesVp8Config) {
  static const size_t kNumberOfTemporalLayers = 4;
  class VideoCodecConfigObserver : public test::SendTest,
                                   public test::FakeEncoder {
   public:
    VideoCodecConfigObserver()
        : SendTest(kDefaultTimeoutMs),
          FakeEncoder(Clock::GetRealTimeClock()),
          num_initializations_(0) {
      memset(&vp8_settings_, 0, sizeof(vp8_settings_));
    }

   private:
    virtual void ModifyConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) OVERRIDE {
      send_config->encoder_settings.encoder = this;
      send_config->encoder_settings.payload_name = "VP8";

      for (size_t i = 0; i < encoder_config->streams.size(); ++i) {
        encoder_config->streams[i].temporal_layer_thresholds_bps.resize(
            kNumberOfTemporalLayers - 1);
      }

      encoder_config->encoder_specific_settings = &vp8_settings_;
      encoder_config_ = *encoder_config;
    }

    virtual void OnStreamsCreated(
        VideoSendStream* send_stream,
        const std::vector<VideoReceiveStream*>& receive_streams) OVERRIDE {
      stream_ = send_stream;
    }

    virtual int32_t InitEncode(const VideoCodec* config,
                               int32_t number_of_cores,
                               uint32_t max_payload_size) OVERRIDE {
      EXPECT_EQ(kVideoCodecVP8, config->codecType);

      // Check that the number of temporal layers has propagated properly to
      // VideoCodec.
      EXPECT_EQ(kNumberOfTemporalLayers,
                config->codecSpecific.VP8.numberOfTemporalLayers);

      for (unsigned char i = 0; i < config->numberOfSimulcastStreams; ++i) {
        EXPECT_EQ(kNumberOfTemporalLayers,
                  config->simulcastStream[i].numberOfTemporalLayers);
      }

      // Set expected temporal layers as they should have been set when
      // reconfiguring the encoder and not match the set config.
      vp8_settings_.numberOfTemporalLayers = kNumberOfTemporalLayers;
      EXPECT_EQ(0,
                memcmp(&config->codecSpecific.VP8,
                       &vp8_settings_,
                       sizeof(vp8_settings_)));
      ++num_initializations_;
      return FakeEncoder::InitEncode(config, number_of_cores, max_payload_size);
    }

    virtual void PerformTest() OVERRIDE {
      EXPECT_EQ(1u, num_initializations_) << "VideoEncoder not initialized.";

      vp8_settings_.denoisingOn = true;
      stream_->ReconfigureVideoEncoder(encoder_config_);
      EXPECT_EQ(2u, num_initializations_)
          << "ReconfigureVideoEncoder did not reinitialize the encoder with "
             "new encoder settings.";
    }

    int32_t Encode(const I420VideoFrame& input_image,
                   const CodecSpecificInfo* codec_specific_info,
                   const std::vector<VideoFrameType>* frame_types) OVERRIDE {
      // Silently skip the encode, FakeEncoder::Encode doesn't produce VP8.
      return 0;
    }

    VideoCodecVP8 vp8_settings_;
    size_t num_initializations_;
    VideoSendStream* stream_;
    VideoEncoderConfig encoder_config_;
  } test;

  RunBaseTest(&test);
}

TEST_F(VideoSendStreamTest, RtcpSenderReportContainsMediaBytesSent) {
  class RtcpByeTest : public test::SendTest {
   public:
    RtcpByeTest() : SendTest(kDefaultTimeoutMs), media_bytes_sent_(0) {}

   private:
    virtual Action OnSendRtp(const uint8_t* packet, size_t length) OVERRIDE {
      RTPHeader header;
      EXPECT_TRUE(parser_->Parse(packet, length, &header));
      media_bytes_sent_ += length - header.headerLength - header.paddingLength;
      return SEND_PACKET;
    }

    virtual Action OnSendRtcp(const uint8_t* packet, size_t length) OVERRIDE {
      RTCPUtility::RTCPParserV2 parser(packet, length, true);
      EXPECT_TRUE(parser.IsValid());

      RTCPUtility::RTCPPacketTypes packet_type = parser.Begin();
      uint32_t sender_octet_count = 0;
      while (packet_type != RTCPUtility::kRtcpNotValidCode) {
        if (packet_type == RTCPUtility::kRtcpSrCode) {
          sender_octet_count = parser.Packet().SR.SenderOctetCount;
          EXPECT_EQ(sender_octet_count, media_bytes_sent_);
          if (sender_octet_count > 0)
            observation_complete_->Set();
        }

        packet_type = parser.Iterate();
      }

      return SEND_PACKET;
    }

    virtual void PerformTest() OVERRIDE {
      EXPECT_EQ(kEventSignaled, Wait())
          << "Timed out while waiting for RTCP sender report.";
    }

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
    virtual int32_t InitEncode(const VideoCodec* config,
                               int32_t number_of_cores,
                               uint32_t max_payload_size) {
      EXPECT_EQ(static_cast<unsigned int>(kScreencastTargetBitrateKbps),
                config->targetBitrate);
      observation_complete_->Set();
      return test::FakeEncoder::InitEncode(
          config, number_of_cores, max_payload_size);
    }
    virtual void ModifyConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) OVERRIDE {
      send_config->encoder_settings.encoder = this;
      EXPECT_EQ(1u, encoder_config->streams.size());
      EXPECT_TRUE(
          encoder_config->streams[0].temporal_layer_thresholds_bps.empty());
      encoder_config->streams[0].temporal_layer_thresholds_bps.push_back(
          kScreencastTargetBitrateKbps * 1000);
      encoder_config->content_type = VideoEncoderConfig::kScreenshare;
    }

    virtual void PerformTest() OVERRIDE {
      EXPECT_EQ(kEventSignaled, Wait())
          << "Timed out while waiting for the encoder to be initialized.";
    }
  } test;

  RunBaseTest(&test);
}
}  // namespace webrtc
