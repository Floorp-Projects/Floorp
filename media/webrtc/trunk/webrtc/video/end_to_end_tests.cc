/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <algorithm>
#include <map>
#include <sstream>
#include <string>

#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/base/checks.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/call.h"
#include "webrtc/frame_callback.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_utility.h"
#include "webrtc/modules/video_coding/codecs/vp8/include/vp8.h"
#include "webrtc/modules/video_coding/codecs/vp9/include/vp9.h"
#include "webrtc/modules/video_coding/main/interface/video_coding_defines.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/event_wrapper.h"
#include "webrtc/system_wrappers/interface/metrics.h"
#include "webrtc/system_wrappers/interface/sleep.h"
#include "webrtc/test/call_test.h"
#include "webrtc/test/direct_transport.h"
#include "webrtc/test/encoder_settings.h"
#include "webrtc/test/fake_audio_device.h"
#include "webrtc/test/fake_decoder.h"
#include "webrtc/test/fake_encoder.h"
#include "webrtc/test/frame_generator.h"
#include "webrtc/test/frame_generator_capturer.h"
#include "webrtc/test/histogram.h"
#include "webrtc/test/null_transport.h"
#include "webrtc/test/rtcp_packet_parser.h"
#include "webrtc/test/rtp_rtcp_observer.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/test/testsupport/gtest_disable.h"
#include "webrtc/test/testsupport/perf_test.h"
#include "webrtc/video/transport_adapter.h"
#include "webrtc/video_encoder.h"

namespace webrtc {

static const unsigned long kSilenceTimeoutMs = 2000;

class EndToEndTest : public test::CallTest {
 public:
  EndToEndTest() {}

  virtual ~EndToEndTest() {
    EXPECT_EQ(nullptr, send_stream_);
    EXPECT_TRUE(receive_streams_.empty());
  }

 protected:
  class UnusedTransport : public newapi::Transport {
   private:
    bool SendRtp(const uint8_t* packet, size_t length) override {
      ADD_FAILURE() << "Unexpected RTP sent.";
      return false;
    }

    bool SendRtcp(const uint8_t* packet, size_t length) override {
      ADD_FAILURE() << "Unexpected RTCP sent.";
      return false;
    }
  };

  void DecodesRetransmittedFrame(bool retransmit_over_rtx);
  void ReceivesPliAndRecovers(int rtp_history_ms);
  void RespectsRtcpMode(newapi::RtcpMode rtcp_mode);
  void TestXrReceiverReferenceTimeReport(bool enable_rrtr);
  void TestSendsSetSsrcs(size_t num_ssrcs, bool send_single_ssrc_first);
  void TestRtpStatePreservation(bool use_rtx);
  void TestReceivedFecPacketsNotNacked(const FakeNetworkPipe::Config& config);
};

TEST_F(EndToEndTest, ReceiverCanBeStartedTwice) {
  test::NullTransport transport;
  CreateCalls(Call::Config(&transport), Call::Config(&transport));

  CreateSendConfig(1);
  CreateMatchingReceiveConfigs();

  CreateStreams();

  receive_streams_[0]->Start();
  receive_streams_[0]->Start();

  DestroyStreams();
}

TEST_F(EndToEndTest, ReceiverCanBeStoppedTwice) {
  test::NullTransport transport;
  CreateCalls(Call::Config(&transport), Call::Config(&transport));

  CreateSendConfig(1);
  CreateMatchingReceiveConfigs();

  CreateStreams();

  receive_streams_[0]->Stop();
  receive_streams_[0]->Stop();

  DestroyStreams();
}

TEST_F(EndToEndTest, RendersSingleDelayedFrame) {
  static const int kWidth = 320;
  static const int kHeight = 240;
  // This constant is chosen to be higher than the timeout in the video_render
  // module. This makes sure that frames aren't dropped if there are no other
  // frames in the queue.
  static const int kDelayRenderCallbackMs = 1000;

  class Renderer : public VideoRenderer {
   public:
    Renderer() : event_(EventWrapper::Create()) {}

    void RenderFrame(const I420VideoFrame& video_frame,
                     int /*time_to_render_ms*/) override {
      event_->Set();
    }

    bool IsTextureSupported() const override { return false; }

    EventTypeWrapper Wait() { return event_->Wait(kDefaultTimeoutMs); }

    rtc::scoped_ptr<EventWrapper> event_;
  } renderer;

  class TestFrameCallback : public I420FrameCallback {
   public:
    TestFrameCallback() : event_(EventWrapper::Create()) {}

    EventTypeWrapper Wait() { return event_->Wait(kDefaultTimeoutMs); }

   private:
    void FrameCallback(I420VideoFrame* frame) override {
      SleepMs(kDelayRenderCallbackMs);
      event_->Set();
    }

    rtc::scoped_ptr<EventWrapper> event_;
  };

  test::DirectTransport sender_transport, receiver_transport;

  CreateCalls(Call::Config(&sender_transport),
              Call::Config(&receiver_transport));

  sender_transport.SetReceiver(receiver_call_->Receiver());
  receiver_transport.SetReceiver(sender_call_->Receiver());

  CreateSendConfig(1);
  CreateMatchingReceiveConfigs();

  TestFrameCallback pre_render_callback;
  receive_configs_[0].pre_render_callback = &pre_render_callback;
  receive_configs_[0].renderer = &renderer;

  CreateStreams();
  Start();

  // Create frames that are smaller than the send width/height, this is done to
  // check that the callbacks are done after processing video.
  rtc::scoped_ptr<test::FrameGenerator> frame_generator(
      test::FrameGenerator::CreateChromaGenerator(kWidth, kHeight));
  send_stream_->Input()->IncomingCapturedFrame(*frame_generator->NextFrame());
  EXPECT_EQ(kEventSignaled, pre_render_callback.Wait())
      << "Timed out while waiting for pre-render callback.";
  EXPECT_EQ(kEventSignaled, renderer.Wait())
      << "Timed out while waiting for the frame to render.";

  Stop();

  sender_transport.StopSending();
  receiver_transport.StopSending();

  DestroyStreams();
}

TEST_F(EndToEndTest, TransmitsFirstFrame) {
  class Renderer : public VideoRenderer {
   public:
    Renderer() : event_(EventWrapper::Create()) {}

    void RenderFrame(const I420VideoFrame& video_frame,
                     int /*time_to_render_ms*/) override {
      event_->Set();
    }
    bool IsTextureSupported() const override { return false; }

    EventTypeWrapper Wait() { return event_->Wait(kDefaultTimeoutMs); }

    rtc::scoped_ptr<EventWrapper> event_;
  } renderer;

  test::DirectTransport sender_transport, receiver_transport;

  CreateCalls(Call::Config(&sender_transport),
              Call::Config(&receiver_transport));

  sender_transport.SetReceiver(receiver_call_->Receiver());
  receiver_transport.SetReceiver(sender_call_->Receiver());

  CreateSendConfig(1);
  CreateMatchingReceiveConfigs();
  receive_configs_[0].renderer = &renderer;

  CreateStreams();
  Start();

  rtc::scoped_ptr<test::FrameGenerator> frame_generator(
      test::FrameGenerator::CreateChromaGenerator(
          encoder_config_.streams[0].width, encoder_config_.streams[0].height));
  send_stream_->Input()->IncomingCapturedFrame(*frame_generator->NextFrame());

  EXPECT_EQ(kEventSignaled, renderer.Wait())
      << "Timed out while waiting for the frame to render.";

  Stop();

  sender_transport.StopSending();
  receiver_transport.StopSending();

  DestroyStreams();
}

TEST_F(EndToEndTest, SendsAndReceivesVP9) {
  class VP9Observer : public test::EndToEndTest, public VideoRenderer {
   public:
    VP9Observer()
        : EndToEndTest(2 * kDefaultTimeoutMs),
          encoder_(VideoEncoder::Create(VideoEncoder::kVp9)),
          decoder_(VP9Decoder::Create()),
          frame_counter_(0) {}

    void PerformTest() override {
      EXPECT_EQ(kEventSignaled, Wait())
          << "Timed out while waiting for enough frames to be decoded.";
    }

    void ModifyConfigs(VideoSendStream::Config* send_config,
                       std::vector<VideoReceiveStream::Config>* receive_configs,
                       VideoEncoderConfig* encoder_config) override {
      send_config->encoder_settings.encoder = encoder_.get();
      send_config->encoder_settings.payload_name = "VP9";
      send_config->encoder_settings.payload_type = VCM_VP9_PAYLOAD_TYPE;
      encoder_config->streams[0].min_bitrate_bps = 50000;
      encoder_config->streams[0].target_bitrate_bps =
          encoder_config->streams[0].max_bitrate_bps = 2000000;

      (*receive_configs)[0].renderer = this;
      (*receive_configs)[0].decoders.resize(1);
      (*receive_configs)[0].decoders[0].payload_type =
          send_config->encoder_settings.payload_type;
      (*receive_configs)[0].decoders[0].payload_name =
          send_config->encoder_settings.payload_name;
      (*receive_configs)[0].decoders[0].decoder = decoder_.get();
    }

    void RenderFrame(const I420VideoFrame& video_frame,
                     int time_to_render_ms) override {
      const int kRequiredFrames = 500;
      if (++frame_counter_ == kRequiredFrames)
        observation_complete_->Set();
    }

    bool IsTextureSupported() const override { return false; }

   private:
    rtc::scoped_ptr<webrtc::VideoEncoder> encoder_;
    rtc::scoped_ptr<webrtc::VideoDecoder> decoder_;
    int frame_counter_;
  } test;

  RunBaseTest(&test);
}

TEST_F(EndToEndTest, SendsAndReceivesH264) {
  class H264Observer : public test::EndToEndTest, public VideoRenderer {
   public:
    H264Observer()
        : EndToEndTest(2 * kDefaultTimeoutMs),
          fake_encoder_(Clock::GetRealTimeClock()),
          frame_counter_(0) {}

    void PerformTest() override {
      EXPECT_EQ(kEventSignaled, Wait())
          << "Timed out while waiting for enough frames to be decoded.";
    }

    void ModifyConfigs(VideoSendStream::Config* send_config,
                       std::vector<VideoReceiveStream::Config>* receive_configs,
                       VideoEncoderConfig* encoder_config) override {
      send_config->rtp.nack.rtp_history_ms =
          (*receive_configs)[0].rtp.nack.rtp_history_ms = kNackRtpHistoryMs;
      send_config->encoder_settings.encoder = &fake_encoder_;
      send_config->encoder_settings.payload_name = "H264";
      send_config->encoder_settings.payload_type = kFakeSendPayloadType;
      encoder_config->streams[0].min_bitrate_bps = 50000;
      encoder_config->streams[0].target_bitrate_bps =
          encoder_config->streams[0].max_bitrate_bps = 2000000;

      (*receive_configs)[0].renderer = this;
      (*receive_configs)[0].decoders.resize(1);
      (*receive_configs)[0].decoders[0].payload_type =
          send_config->encoder_settings.payload_type;
      (*receive_configs)[0].decoders[0].payload_name =
          send_config->encoder_settings.payload_name;
      (*receive_configs)[0].decoders[0].decoder = &fake_decoder_;
    }

    void RenderFrame(const I420VideoFrame& video_frame,
                     int time_to_render_ms) override {
      const int kRequiredFrames = 500;
      if (++frame_counter_ == kRequiredFrames)
        observation_complete_->Set();
    }

    bool IsTextureSupported() const override { return false; }

   private:
    test::FakeH264Decoder fake_decoder_;
    test::FakeH264Encoder fake_encoder_;
    int frame_counter_;
  } test;

  RunBaseTest(&test);
}

TEST_F(EndToEndTest, ReceiverUsesLocalSsrc) {
  class SyncRtcpObserver : public test::EndToEndTest {
   public:
    SyncRtcpObserver() : EndToEndTest(kDefaultTimeoutMs) {}

    Action OnReceiveRtcp(const uint8_t* packet, size_t length) override {
      RTCPUtility::RTCPParserV2 parser(packet, length, true);
      EXPECT_TRUE(parser.IsValid());
      uint32_t ssrc = 0;
      ssrc |= static_cast<uint32_t>(packet[4]) << 24;
      ssrc |= static_cast<uint32_t>(packet[5]) << 16;
      ssrc |= static_cast<uint32_t>(packet[6]) << 8;
      ssrc |= static_cast<uint32_t>(packet[7]) << 0;
      EXPECT_EQ(kReceiverLocalSsrc, ssrc);
      observation_complete_->Set();

      return SEND_PACKET;
    }

    void PerformTest() override {
      EXPECT_EQ(kEventSignaled, Wait())
          << "Timed out while waiting for a receiver RTCP packet to be sent.";
    }
  } test;

  RunBaseTest(&test);
}

TEST_F(EndToEndTest, ReceivesAndRetransmitsNack) {
  static const int kNumberOfNacksToObserve = 2;
  static const int kLossBurstSize = 2;
  static const int kPacketsBetweenLossBursts = 9;
  class NackObserver : public test::EndToEndTest {
   public:
    NackObserver()
        : EndToEndTest(kLongTimeoutMs),
          rtp_parser_(RtpHeaderParser::Create()),
          sent_rtp_packets_(0),
          packets_left_to_drop_(0),
          nacks_left_(kNumberOfNacksToObserve) {}

   private:
    Action OnSendRtp(const uint8_t* packet, size_t length) override {
      RTPHeader header;
      EXPECT_TRUE(rtp_parser_->Parse(packet, length, &header));

      // Never drop retransmitted packets.
      if (dropped_packets_.find(header.sequenceNumber) !=
          dropped_packets_.end()) {
        retransmitted_packets_.insert(header.sequenceNumber);
        if (nacks_left_ == 0 &&
            retransmitted_packets_.size() == dropped_packets_.size()) {
          observation_complete_->Set();
        }
        return SEND_PACKET;
      }

      ++sent_rtp_packets_;

      // Enough NACKs received, stop dropping packets.
      if (nacks_left_ == 0)
        return SEND_PACKET;

      // Check if it's time for a new loss burst.
      if (sent_rtp_packets_ % kPacketsBetweenLossBursts == 0)
        packets_left_to_drop_ = kLossBurstSize;

      if (packets_left_to_drop_ > 0) {
        --packets_left_to_drop_;
        dropped_packets_.insert(header.sequenceNumber);
        return DROP_PACKET;
      }

      return SEND_PACKET;
    }

    Action OnReceiveRtcp(const uint8_t* packet, size_t length) override {
      RTCPUtility::RTCPParserV2 parser(packet, length, true);
      EXPECT_TRUE(parser.IsValid());

      RTCPUtility::RTCPPacketTypes packet_type = parser.Begin();
      while (packet_type != RTCPUtility::kRtcpNotValidCode) {
        if (packet_type == RTCPUtility::kRtcpRtpfbNackCode) {
          --nacks_left_;
          break;
        }
        packet_type = parser.Iterate();
      }
      return SEND_PACKET;
    }

    void ModifyConfigs(VideoSendStream::Config* send_config,
                       std::vector<VideoReceiveStream::Config>* receive_configs,
                       VideoEncoderConfig* encoder_config) override {
      send_config->rtp.nack.rtp_history_ms = kNackRtpHistoryMs;
      (*receive_configs)[0].rtp.nack.rtp_history_ms = kNackRtpHistoryMs;
    }

    void PerformTest() override {
      EXPECT_EQ(kEventSignaled, Wait())
          << "Timed out waiting for packets to be NACKed, retransmitted and "
             "rendered.";
    }

    rtc::scoped_ptr<RtpHeaderParser> rtp_parser_;
    std::set<uint16_t> dropped_packets_;
    std::set<uint16_t> retransmitted_packets_;
    uint64_t sent_rtp_packets_;
    int packets_left_to_drop_;
    int nacks_left_;
  } test;

  RunBaseTest(&test);
}

TEST_F(EndToEndTest, CanReceiveFec) {
  class FecRenderObserver : public test::EndToEndTest, public VideoRenderer {
   public:
    FecRenderObserver()
        : EndToEndTest(kDefaultTimeoutMs), state_(kFirstPacket) {}

   private:
    Action OnSendRtp(const uint8_t* packet, size_t length) override
        EXCLUSIVE_LOCKS_REQUIRED(crit_) {
      RTPHeader header;
      EXPECT_TRUE(parser_->Parse(packet, length, &header));

      EXPECT_EQ(kRedPayloadType, header.payloadType);
      int encapsulated_payload_type =
          static_cast<int>(packet[header.headerLength]);
      if (encapsulated_payload_type != kFakeSendPayloadType)
        EXPECT_EQ(kUlpfecPayloadType, encapsulated_payload_type);

      if (protected_sequence_numbers_.count(header.sequenceNumber) != 0) {
        // Retransmitted packet, should not count.
        protected_sequence_numbers_.erase(header.sequenceNumber);
        EXPECT_GT(protected_timestamps_.count(header.timestamp), 0u);
        protected_timestamps_.erase(header.timestamp);
        return SEND_PACKET;
      }

      switch (state_) {
        case kFirstPacket:
          state_ = kDropEveryOtherPacketUntilFec;
          break;
        case kDropEveryOtherPacketUntilFec:
          if (encapsulated_payload_type == kUlpfecPayloadType) {
            state_ = kDropNextMediaPacket;
            return SEND_PACKET;
          }
          if (header.sequenceNumber % 2 == 0)
            return DROP_PACKET;
          break;
        case kDropNextMediaPacket:
          if (encapsulated_payload_type == kFakeSendPayloadType) {
            protected_sequence_numbers_.insert(header.sequenceNumber);
            protected_timestamps_.insert(header.timestamp);
            state_ = kDropEveryOtherPacketUntilFec;
            return DROP_PACKET;
          }
          break;
      }

      return SEND_PACKET;
    }

    void RenderFrame(const I420VideoFrame& video_frame,
                     int time_to_render_ms) override {
      CriticalSectionScoped lock(crit_.get());
      // Rendering frame with timestamp of packet that was dropped -> FEC
      // protection worked.
      if (protected_timestamps_.count(video_frame.timestamp()) != 0)
        observation_complete_->Set();
    }

    bool IsTextureSupported() const override { return false; }

    enum {
      kFirstPacket,
      kDropEveryOtherPacketUntilFec,
      kDropNextMediaPacket,
    } state_;

    void ModifyConfigs(VideoSendStream::Config* send_config,
                       std::vector<VideoReceiveStream::Config>* receive_configs,
                       VideoEncoderConfig* encoder_config) override {
      // TODO(pbos): Run this test with combined NACK/FEC enabled as well.
      // int rtp_history_ms = 1000;
      // (*receive_configs)[0].rtp.nack.rtp_history_ms = rtp_history_ms;
      // send_config->rtp.nack.rtp_history_ms = rtp_history_ms;
      send_config->rtp.fec.red_payload_type = kRedPayloadType;
      send_config->rtp.fec.ulpfec_payload_type = kUlpfecPayloadType;

      (*receive_configs)[0].rtp.fec.red_payload_type = kRedPayloadType;
      (*receive_configs)[0].rtp.fec.ulpfec_payload_type = kUlpfecPayloadType;
      (*receive_configs)[0].renderer = this;
    }

    void PerformTest() override {
      EXPECT_EQ(kEventSignaled, Wait())
          << "Timed out waiting for dropped frames frames to be rendered.";
    }

    std::set<uint32_t> protected_sequence_numbers_ GUARDED_BY(crit_);
    std::set<uint32_t> protected_timestamps_ GUARDED_BY(crit_);
  } test;

  RunBaseTest(&test);
}

// Flacky on all platforms. See webrtc:4328.
TEST_F(EndToEndTest, DISABLED_ReceivedFecPacketsNotNacked) {
  // At low RTT (< kLowRttNackMs) -> NACK only, no FEC.
  // Configure some network delay.
  const int kNetworkDelayMs = 50;
  FakeNetworkPipe::Config config;
  config.queue_delay_ms = kNetworkDelayMs;
  TestReceivedFecPacketsNotNacked(config);
}

void EndToEndTest::TestReceivedFecPacketsNotNacked(
    const FakeNetworkPipe::Config& config) {
  class FecNackObserver : public test::EndToEndTest {
   public:
    explicit FecNackObserver(const FakeNetworkPipe::Config& config)
        : EndToEndTest(kDefaultTimeoutMs, config),
          state_(kFirstPacket),
          fec_sequence_number_(0),
          has_last_sequence_number_(false),
          last_sequence_number_(0) {}

   private:
    Action OnSendRtp(const uint8_t* packet, size_t length) override {
      RTPHeader header;
      EXPECT_TRUE(parser_->Parse(packet, length, &header));
      EXPECT_EQ(kRedPayloadType, header.payloadType);

      int encapsulated_payload_type =
          static_cast<int>(packet[header.headerLength]);
      if (encapsulated_payload_type != kFakeSendPayloadType)
        EXPECT_EQ(kUlpfecPayloadType, encapsulated_payload_type);

      if (has_last_sequence_number_ &&
          !IsNewerSequenceNumber(header.sequenceNumber,
                                 last_sequence_number_)) {
        // Drop retransmitted packets.
        return DROP_PACKET;
      }
      last_sequence_number_ = header.sequenceNumber;
      has_last_sequence_number_ = true;

      bool fec_packet = encapsulated_payload_type == kUlpfecPayloadType;
      switch (state_) {
        case kFirstPacket:
          state_ = kDropEveryOtherPacketUntilFec;
          break;
        case kDropEveryOtherPacketUntilFec:
          if (fec_packet) {
            state_ = kDropAllMediaPacketsUntilFec;
          } else if (header.sequenceNumber % 2 == 0) {
            return DROP_PACKET;
          }
          break;
        case kDropAllMediaPacketsUntilFec:
          if (!fec_packet)
            return DROP_PACKET;
          fec_sequence_number_ = header.sequenceNumber;
          state_ = kVerifyFecPacketNotInNackList;
          break;
        case kVerifyFecPacketNotInNackList:
          // Continue to drop packets. Make sure no frame can be decoded.
          if (fec_packet || header.sequenceNumber % 2 == 0)
            return DROP_PACKET;
          break;
      }
      return SEND_PACKET;
    }

    Action OnReceiveRtcp(const uint8_t* packet, size_t length) override {
      if (state_ == kVerifyFecPacketNotInNackList) {
        test::RtcpPacketParser rtcp_parser;
        rtcp_parser.Parse(packet, length);
        std::vector<uint16_t> nacks = rtcp_parser.nack_item()->last_nack_list();
        if (!nacks.empty() &&
            IsNewerSequenceNumber(nacks.back(), fec_sequence_number_)) {
          EXPECT_TRUE(std::find(
              nacks.begin(), nacks.end(), fec_sequence_number_) == nacks.end());
          observation_complete_->Set();
        }
      }
      return SEND_PACKET;
    }

    // TODO(holmer): Investigate why we don't send FEC packets when the bitrate
    // is 10 kbps.
    Call::Config GetSenderCallConfig() override {
      Call::Config config(SendTransport());
      const int kMinBitrateBps = 30000;
      config.bitrate_config.min_bitrate_bps = kMinBitrateBps;
      return config;
    }

    void ModifyConfigs(VideoSendStream::Config* send_config,
                       std::vector<VideoReceiveStream::Config>* receive_configs,
                       VideoEncoderConfig* encoder_config) override {
      // Configure hybrid NACK/FEC.
      send_config->rtp.nack.rtp_history_ms = kNackRtpHistoryMs;
      send_config->rtp.fec.red_payload_type = kRedPayloadType;
      send_config->rtp.fec.ulpfec_payload_type = kUlpfecPayloadType;
      (*receive_configs)[0].rtp.nack.rtp_history_ms = kNackRtpHistoryMs;
      (*receive_configs)[0].rtp.fec.red_payload_type = kRedPayloadType;
      (*receive_configs)[0].rtp.fec.ulpfec_payload_type = kUlpfecPayloadType;
    }

    void PerformTest() override {
      EXPECT_EQ(kEventSignaled, Wait())
          << "Timed out while waiting for FEC packets to be received.";
    }

    enum {
      kFirstPacket,
      kDropEveryOtherPacketUntilFec,
      kDropAllMediaPacketsUntilFec,
      kVerifyFecPacketNotInNackList,
    } state_;

    uint16_t fec_sequence_number_;
    bool has_last_sequence_number_;
    uint16_t last_sequence_number_;
  } test(config);

  RunBaseTest(&test);
}

// This test drops second RTP packet with a marker bit set, makes sure it's
// retransmitted and renders. Retransmission SSRCs are also checked.
void EndToEndTest::DecodesRetransmittedFrame(bool retransmit_over_rtx) {
  static const int kDroppedFrameNumber = 2;
  class RetransmissionObserver : public test::EndToEndTest,
                                 public I420FrameCallback {
   public:
    explicit RetransmissionObserver(bool expect_rtx)
        : EndToEndTest(kDefaultTimeoutMs),
          retransmission_ssrc_(expect_rtx ? kSendRtxSsrcs[0] : kSendSsrcs[0]),
          retransmission_payload_type_(expect_rtx ? kSendRtxPayloadType
                                                  : kFakeSendPayloadType),
          marker_bits_observed_(0),
          retransmitted_timestamp_(0),
          frame_retransmitted_(false) {}

   private:
    Action OnSendRtp(const uint8_t* packet, size_t length) override {
      RTPHeader header;
      EXPECT_TRUE(parser_->Parse(packet, length, &header));

      if (header.timestamp == retransmitted_timestamp_) {
        EXPECT_EQ(retransmission_ssrc_, header.ssrc);
        EXPECT_EQ(retransmission_payload_type_, header.payloadType);
        frame_retransmitted_ = true;
        return SEND_PACKET;
      }

      EXPECT_EQ(kSendSsrcs[0], header.ssrc);
      EXPECT_EQ(kFakeSendPayloadType, header.payloadType);

      // Found the second frame's final packet, drop this and expect a
      // retransmission.
      if (header.markerBit && ++marker_bits_observed_ == kDroppedFrameNumber) {
        retransmitted_timestamp_ = header.timestamp;
        return DROP_PACKET;
      }

      return SEND_PACKET;
    }

    void FrameCallback(I420VideoFrame* frame) override {
      CriticalSectionScoped lock(crit_.get());
      if (frame->timestamp() == retransmitted_timestamp_) {
        EXPECT_TRUE(frame_retransmitted_);
        observation_complete_->Set();
      }
    }

    void ModifyConfigs(VideoSendStream::Config* send_config,
                       std::vector<VideoReceiveStream::Config>* receive_configs,
                       VideoEncoderConfig* encoder_config) override {
      send_config->rtp.nack.rtp_history_ms = kNackRtpHistoryMs;
      (*receive_configs)[0].pre_render_callback = this;
      (*receive_configs)[0].rtp.nack.rtp_history_ms = kNackRtpHistoryMs;
      if (retransmission_ssrc_ == kSendRtxSsrcs[0]) {
        send_config->rtp.rtx.ssrcs.push_back(kSendRtxSsrcs[0]);
        send_config->rtp.rtx.payload_type = kSendRtxPayloadType;
        (*receive_configs)[0].rtp.rtx[kSendRtxPayloadType].ssrc =
            kSendRtxSsrcs[0];
        (*receive_configs)[0].rtp.rtx[kSendRtxPayloadType].payload_type =
            kSendRtxPayloadType;
      }
    }

    void PerformTest() override {
      EXPECT_EQ(kEventSignaled, Wait())
          << "Timed out while waiting for retransmission to render.";
    }

    const uint32_t retransmission_ssrc_;
    const int retransmission_payload_type_;
    int marker_bits_observed_;
    uint32_t retransmitted_timestamp_;
    bool frame_retransmitted_;
  } test(retransmit_over_rtx);

  RunBaseTest(&test);
}

TEST_F(EndToEndTest, DecodesRetransmittedFrame) {
  DecodesRetransmittedFrame(false);
}

TEST_F(EndToEndTest, DecodesRetransmittedFrameOverRtx) {
  DecodesRetransmittedFrame(true);
}

TEST_F(EndToEndTest, UsesFrameCallbacks) {
  static const int kWidth = 320;
  static const int kHeight = 240;

  class Renderer : public VideoRenderer {
   public:
    Renderer() : event_(EventWrapper::Create()) {}

    void RenderFrame(const I420VideoFrame& video_frame,
                     int /*time_to_render_ms*/) override {
      EXPECT_EQ(0, *video_frame.buffer(kYPlane))
          << "Rendered frame should have zero luma which is applied by the "
             "pre-render callback.";
      event_->Set();
    }

    bool IsTextureSupported() const override { return false; }

    EventTypeWrapper Wait() { return event_->Wait(kDefaultTimeoutMs); }
    rtc::scoped_ptr<EventWrapper> event_;
  } renderer;

  class TestFrameCallback : public I420FrameCallback {
   public:
    TestFrameCallback(int expected_luma_byte, int next_luma_byte)
        : event_(EventWrapper::Create()),
          expected_luma_byte_(expected_luma_byte),
          next_luma_byte_(next_luma_byte) {}

    EventTypeWrapper Wait() { return event_->Wait(kDefaultTimeoutMs); }

   private:
    virtual void FrameCallback(I420VideoFrame* frame) {
      EXPECT_EQ(kWidth, frame->width())
          << "Width not as expected, callback done before resize?";
      EXPECT_EQ(kHeight, frame->height())
          << "Height not as expected, callback done before resize?";

      // Previous luma specified, observed luma should be fairly close.
      if (expected_luma_byte_ != -1) {
        EXPECT_NEAR(expected_luma_byte_, *frame->buffer(kYPlane), 10);
      }

      memset(frame->buffer(kYPlane),
             next_luma_byte_,
             frame->allocated_size(kYPlane));

      event_->Set();
    }

    rtc::scoped_ptr<EventWrapper> event_;
    int expected_luma_byte_;
    int next_luma_byte_;
  };

  TestFrameCallback pre_encode_callback(-1, 255);  // Changes luma to 255.
  TestFrameCallback pre_render_callback(255, 0);  // Changes luma from 255 to 0.

  test::DirectTransport sender_transport, receiver_transport;

  CreateCalls(Call::Config(&sender_transport),
              Call::Config(&receiver_transport));

  sender_transport.SetReceiver(receiver_call_->Receiver());
  receiver_transport.SetReceiver(sender_call_->Receiver());

  CreateSendConfig(1);
  rtc::scoped_ptr<VideoEncoder> encoder(
      VideoEncoder::Create(VideoEncoder::kVp8));
  send_config_.encoder_settings.encoder = encoder.get();
  send_config_.encoder_settings.payload_name = "VP8";
  ASSERT_EQ(1u, encoder_config_.streams.size()) << "Test setup error.";
  encoder_config_.streams[0].width = kWidth;
  encoder_config_.streams[0].height = kHeight;
  send_config_.pre_encode_callback = &pre_encode_callback;

  CreateMatchingReceiveConfigs();
  receive_configs_[0].pre_render_callback = &pre_render_callback;
  receive_configs_[0].renderer = &renderer;

  CreateStreams();
  Start();

  // Create frames that are smaller than the send width/height, this is done to
  // check that the callbacks are done after processing video.
  rtc::scoped_ptr<test::FrameGenerator> frame_generator(
      test::FrameGenerator::CreateChromaGenerator(kWidth / 2, kHeight / 2));
  send_stream_->Input()->IncomingCapturedFrame(*frame_generator->NextFrame());

  EXPECT_EQ(kEventSignaled, pre_encode_callback.Wait())
      << "Timed out while waiting for pre-encode callback.";
  EXPECT_EQ(kEventSignaled, pre_render_callback.Wait())
      << "Timed out while waiting for pre-render callback.";
  EXPECT_EQ(kEventSignaled, renderer.Wait())
      << "Timed out while waiting for the frame to render.";

  Stop();

  sender_transport.StopSending();
  receiver_transport.StopSending();

  DestroyStreams();
}

void EndToEndTest::ReceivesPliAndRecovers(int rtp_history_ms) {
  static const int kPacketsToDrop = 1;

  class PliObserver : public test::EndToEndTest, public VideoRenderer {
   public:
    explicit PliObserver(int rtp_history_ms)
        : EndToEndTest(kLongTimeoutMs),
          rtp_history_ms_(rtp_history_ms),
          nack_enabled_(rtp_history_ms > 0),
          highest_dropped_timestamp_(0),
          frames_to_drop_(0),
          received_pli_(false) {}

   private:
    Action OnSendRtp(const uint8_t* packet, size_t length) override {
      RTPHeader header;
      EXPECT_TRUE(parser_->Parse(packet, length, &header));

      // Drop all retransmitted packets to force a PLI.
      if (header.timestamp <= highest_dropped_timestamp_)
        return DROP_PACKET;

      if (frames_to_drop_ > 0) {
        highest_dropped_timestamp_ = header.timestamp;
        --frames_to_drop_;
        return DROP_PACKET;
      }

      return SEND_PACKET;
    }

    Action OnReceiveRtcp(const uint8_t* packet, size_t length) override {
      RTCPUtility::RTCPParserV2 parser(packet, length, true);
      EXPECT_TRUE(parser.IsValid());

      for (RTCPUtility::RTCPPacketTypes packet_type = parser.Begin();
           packet_type != RTCPUtility::kRtcpNotValidCode;
           packet_type = parser.Iterate()) {
        if (!nack_enabled_)
          EXPECT_NE(packet_type, RTCPUtility::kRtcpRtpfbNackCode);

        if (packet_type == RTCPUtility::kRtcpPsfbPliCode) {
          received_pli_ = true;
          break;
        }
      }
      return SEND_PACKET;
    }

    void RenderFrame(const I420VideoFrame& video_frame,
                     int time_to_render_ms) override {
      CriticalSectionScoped lock(crit_.get());
      if (received_pli_ &&
          video_frame.timestamp() > highest_dropped_timestamp_) {
        observation_complete_->Set();
      }
      if (!received_pli_)
        frames_to_drop_ = kPacketsToDrop;
    }

    bool IsTextureSupported() const override { return false; }

    void ModifyConfigs(VideoSendStream::Config* send_config,
                       std::vector<VideoReceiveStream::Config>* receive_configs,
                       VideoEncoderConfig* encoder_config) override {
      send_config->rtp.nack.rtp_history_ms = rtp_history_ms_;
      (*receive_configs)[0].rtp.nack.rtp_history_ms = rtp_history_ms_;
      (*receive_configs)[0].renderer = this;
    }

    void PerformTest() override {
      EXPECT_EQ(kEventSignaled, Wait()) << "Timed out waiting for PLI to be "
                                           "received and a frame to be "
                                           "rendered afterwards.";
    }

    int rtp_history_ms_;
    bool nack_enabled_;
    uint32_t highest_dropped_timestamp_;
    int frames_to_drop_;
    bool received_pli_;
  } test(rtp_history_ms);

  RunBaseTest(&test);
}

TEST_F(EndToEndTest, ReceivesPliAndRecoversWithNack) {
  ReceivesPliAndRecovers(1000);
}

// TODO(pbos): Enable this when 2250 is resolved.
TEST_F(EndToEndTest, DISABLED_ReceivesPliAndRecoversWithoutNack) {
  ReceivesPliAndRecovers(0);
}

TEST_F(EndToEndTest, UnknownRtpPacketGivesUnknownSsrcReturnCode) {
  class PacketInputObserver : public PacketReceiver {
   public:
    explicit PacketInputObserver(PacketReceiver* receiver)
        : receiver_(receiver), delivered_packet_(EventWrapper::Create()) {}

    EventTypeWrapper Wait() {
      return delivered_packet_->Wait(kDefaultTimeoutMs);
    }

   private:
    DeliveryStatus DeliverPacket(const uint8_t* packet,
                                 size_t length) override {
      if (RtpHeaderParser::IsRtcp(packet, length)) {
        return receiver_->DeliverPacket(packet, length);
      } else {
        DeliveryStatus delivery_status =
            receiver_->DeliverPacket(packet, length);
        EXPECT_EQ(DELIVERY_UNKNOWN_SSRC, delivery_status);
        delivered_packet_->Set();
        return delivery_status;
      }
    }

    PacketReceiver* receiver_;
    rtc::scoped_ptr<EventWrapper> delivered_packet_;
  };

  test::DirectTransport send_transport, receive_transport;

  CreateCalls(Call::Config(&send_transport), Call::Config(&receive_transport));
  PacketInputObserver input_observer(receiver_call_->Receiver());

  send_transport.SetReceiver(&input_observer);
  receive_transport.SetReceiver(sender_call_->Receiver());

  CreateSendConfig(1);
  CreateMatchingReceiveConfigs();

  CreateStreams();
  CreateFrameGeneratorCapturer();
  Start();

  receiver_call_->DestroyVideoReceiveStream(receive_streams_[0]);
  receive_streams_.clear();

  // Wait() waits for a received packet.
  EXPECT_EQ(kEventSignaled, input_observer.Wait());

  Stop();

  DestroyStreams();

  send_transport.StopSending();
  receive_transport.StopSending();
}

void EndToEndTest::RespectsRtcpMode(newapi::RtcpMode rtcp_mode) {
  static const int kNumCompoundRtcpPacketsToObserve = 10;
  class RtcpModeObserver : public test::EndToEndTest {
   public:
    explicit RtcpModeObserver(newapi::RtcpMode rtcp_mode)
        : EndToEndTest(kDefaultTimeoutMs),
          rtcp_mode_(rtcp_mode),
          sent_rtp_(0),
          sent_rtcp_(0) {}

   private:
    Action OnSendRtp(const uint8_t* packet, size_t length) override {
      if (++sent_rtp_ % 3 == 0)
        return DROP_PACKET;

      return SEND_PACKET;
    }

    Action OnReceiveRtcp(const uint8_t* packet, size_t length) override {
      ++sent_rtcp_;
      RTCPUtility::RTCPParserV2 parser(packet, length, true);
      EXPECT_TRUE(parser.IsValid());

      RTCPUtility::RTCPPacketTypes packet_type = parser.Begin();
      bool has_report_block = false;
      while (packet_type != RTCPUtility::kRtcpNotValidCode) {
        EXPECT_NE(RTCPUtility::kRtcpSrCode, packet_type);
        if (packet_type == RTCPUtility::kRtcpRrCode) {
          has_report_block = true;
          break;
        }
        packet_type = parser.Iterate();
      }

      switch (rtcp_mode_) {
        case newapi::kRtcpCompound:
          if (!has_report_block) {
            ADD_FAILURE() << "Received RTCP packet without receiver report for "
                             "kRtcpCompound.";
            observation_complete_->Set();
          }

          if (sent_rtcp_ >= kNumCompoundRtcpPacketsToObserve)
            observation_complete_->Set();

          break;
        case newapi::kRtcpReducedSize:
          if (!has_report_block)
            observation_complete_->Set();
          break;
      }

      return SEND_PACKET;
    }

    void ModifyConfigs(VideoSendStream::Config* send_config,
                       std::vector<VideoReceiveStream::Config>* receive_configs,
                       VideoEncoderConfig* encoder_config) override {
      send_config->rtp.nack.rtp_history_ms = kNackRtpHistoryMs;
      (*receive_configs)[0].rtp.nack.rtp_history_ms = kNackRtpHistoryMs;
      (*receive_configs)[0].rtp.rtcp_mode = rtcp_mode_;
    }

    void PerformTest() override {
      EXPECT_EQ(kEventSignaled, Wait())
          << (rtcp_mode_ == newapi::kRtcpCompound
                  ? "Timed out before observing enough compound packets."
                  : "Timed out before receiving a non-compound RTCP packet.");
    }

    newapi::RtcpMode rtcp_mode_;
    int sent_rtp_;
    int sent_rtcp_;
  } test(rtcp_mode);

  RunBaseTest(&test);
}

TEST_F(EndToEndTest, UsesRtcpCompoundMode) {
  RespectsRtcpMode(newapi::kRtcpCompound);
}

TEST_F(EndToEndTest, UsesRtcpReducedSizeMode) {
  RespectsRtcpMode(newapi::kRtcpReducedSize);
}

// Test sets up a Call multiple senders with different resolutions and SSRCs.
// Another is set up to receive all three of these with different renderers.
// Each renderer verifies that it receives the expected resolution, and as soon
// as every renderer has received a frame, the test finishes.
TEST_F(EndToEndTest, SendsAndReceivesMultipleStreams) {
  static const size_t kNumStreams = 3;

  class VideoOutputObserver : public VideoRenderer {
   public:
    VideoOutputObserver(test::FrameGeneratorCapturer** capturer,
                        int width,
                        int height)
        : capturer_(capturer),
          width_(width),
          height_(height),
          done_(EventWrapper::Create()) {}

    void RenderFrame(const I420VideoFrame& video_frame,
                     int time_to_render_ms) override {
      EXPECT_EQ(width_, video_frame.width());
      EXPECT_EQ(height_, video_frame.height());
      (*capturer_)->Stop();
      done_->Set();
    }

    bool IsTextureSupported() const override { return false; }

    EventTypeWrapper Wait() { return done_->Wait(kDefaultTimeoutMs); }

   private:
    test::FrameGeneratorCapturer** capturer_;
    int width_;
    int height_;
    rtc::scoped_ptr<EventWrapper> done_;
  };

  struct {
    uint32_t ssrc;
    int width;
    int height;
  } codec_settings[kNumStreams] = {{1, 640, 480}, {2, 320, 240}, {3, 240, 160}};

  test::DirectTransport sender_transport, receiver_transport;
  rtc::scoped_ptr<Call> sender_call(
      Call::Create(Call::Config(&sender_transport)));
  rtc::scoped_ptr<Call> receiver_call(
      Call::Create(Call::Config(&receiver_transport)));
  sender_transport.SetReceiver(receiver_call->Receiver());
  receiver_transport.SetReceiver(sender_call->Receiver());

  VideoSendStream* send_streams[kNumStreams];
  VideoReceiveStream* receive_streams[kNumStreams];

  VideoOutputObserver* observers[kNumStreams];
  test::FrameGeneratorCapturer* frame_generators[kNumStreams];

  rtc::scoped_ptr<VideoEncoder> encoders[kNumStreams];
  for (size_t i = 0; i < kNumStreams; ++i)
    encoders[i].reset(VideoEncoder::Create(VideoEncoder::kVp8));

  ScopedVector<VideoDecoder> allocated_decoders;
  for (size_t i = 0; i < kNumStreams; ++i) {
    uint32_t ssrc = codec_settings[i].ssrc;
    int width = codec_settings[i].width;
    int height = codec_settings[i].height;
    observers[i] = new VideoOutputObserver(&frame_generators[i], width, height);

    VideoSendStream::Config send_config;
    send_config.rtp.ssrcs.push_back(ssrc);
    send_config.encoder_settings.encoder = encoders[i].get();
    send_config.encoder_settings.payload_name = "VP8";
    send_config.encoder_settings.payload_type = 124;
    VideoEncoderConfig encoder_config;
    encoder_config.streams = test::CreateVideoStreams(1);
    VideoStream* stream = &encoder_config.streams[0];
    stream->width = width;
    stream->height = height;
    stream->max_framerate = 5;
    stream->min_bitrate_bps = stream->target_bitrate_bps =
        stream->max_bitrate_bps = 100000;
    send_streams[i] =
        sender_call->CreateVideoSendStream(send_config, encoder_config);
    send_streams[i]->Start();

    VideoReceiveStream::Config receive_config;
    receive_config.renderer = observers[i];
    receive_config.rtp.remote_ssrc = ssrc;
    receive_config.rtp.local_ssrc = kReceiverLocalSsrc;
    VideoReceiveStream::Decoder decoder =
        test::CreateMatchingDecoder(send_config.encoder_settings);
    allocated_decoders.push_back(decoder.decoder);
    receive_config.decoders.push_back(decoder);
    receive_streams[i] =
        receiver_call->CreateVideoReceiveStream(receive_config);
    receive_streams[i]->Start();

    frame_generators[i] = test::FrameGeneratorCapturer::Create(
        send_streams[i]->Input(), width, height, 30, Clock::GetRealTimeClock());
    frame_generators[i]->Start();
  }

  for (size_t i = 0; i < kNumStreams; ++i) {
    EXPECT_EQ(kEventSignaled, observers[i]->Wait())
        << "Timed out while waiting for observer " << i << " to render.";
  }

  for (size_t i = 0; i < kNumStreams; ++i) {
    frame_generators[i]->Stop();
    sender_call->DestroyVideoSendStream(send_streams[i]);
    receiver_call->DestroyVideoReceiveStream(receive_streams[i]);
    delete frame_generators[i];
    delete observers[i];
  }

  sender_transport.StopSending();
  receiver_transport.StopSending();
}

TEST_F(EndToEndTest, ObserversEncodedFrames) {
  class EncodedFrameTestObserver : public EncodedFrameObserver {
   public:
    EncodedFrameTestObserver()
        : length_(0),
          frame_type_(kFrameEmpty),
          called_(EventWrapper::Create()) {}
    virtual ~EncodedFrameTestObserver() {}

    virtual void EncodedFrameCallback(const EncodedFrame& encoded_frame) {
      frame_type_ = encoded_frame.frame_type_;
      length_ = encoded_frame.length_;
      buffer_.reset(new uint8_t[length_]);
      memcpy(buffer_.get(), encoded_frame.data_, length_);
      called_->Set();
    }

    EventTypeWrapper Wait() { return called_->Wait(kDefaultTimeoutMs); }

    void ExpectEqualFrames(const EncodedFrameTestObserver& observer) {
      ASSERT_EQ(length_, observer.length_)
          << "Observed frames are of different lengths.";
      EXPECT_EQ(frame_type_, observer.frame_type_)
          << "Observed frames have different frame types.";
      EXPECT_EQ(0, memcmp(buffer_.get(), observer.buffer_.get(), length_))
          << "Observed encoded frames have different content.";
    }

   private:
    rtc::scoped_ptr<uint8_t[]> buffer_;
    size_t length_;
    FrameType frame_type_;
    rtc::scoped_ptr<EventWrapper> called_;
  };

  EncodedFrameTestObserver post_encode_observer;
  EncodedFrameTestObserver pre_decode_observer;

  test::DirectTransport sender_transport, receiver_transport;

  CreateCalls(Call::Config(&sender_transport),
              Call::Config(&receiver_transport));

  sender_transport.SetReceiver(receiver_call_->Receiver());
  receiver_transport.SetReceiver(sender_call_->Receiver());

  CreateSendConfig(1);
  CreateMatchingReceiveConfigs();
  send_config_.post_encode_callback = &post_encode_observer;
  receive_configs_[0].pre_decode_callback = &pre_decode_observer;

  CreateStreams();
  Start();

  rtc::scoped_ptr<test::FrameGenerator> frame_generator(
      test::FrameGenerator::CreateChromaGenerator(
          encoder_config_.streams[0].width, encoder_config_.streams[0].height));
  send_stream_->Input()->IncomingCapturedFrame(*frame_generator->NextFrame());

  EXPECT_EQ(kEventSignaled, post_encode_observer.Wait())
      << "Timed out while waiting for send-side encoded-frame callback.";

  EXPECT_EQ(kEventSignaled, pre_decode_observer.Wait())
      << "Timed out while waiting for pre-decode encoded-frame callback.";

  post_encode_observer.ExpectEqualFrames(pre_decode_observer);

  Stop();

  sender_transport.StopSending();
  receiver_transport.StopSending();

  DestroyStreams();
}

TEST_F(EndToEndTest, ReceiveStreamSendsRemb) {
  class RembObserver : public test::EndToEndTest {
   public:
    RembObserver() : EndToEndTest(kDefaultTimeoutMs) {}

    Action OnReceiveRtcp(const uint8_t* packet, size_t length) override {
      RTCPUtility::RTCPParserV2 parser(packet, length, true);
      EXPECT_TRUE(parser.IsValid());

      bool received_psfb = false;
      bool received_remb = false;
      RTCPUtility::RTCPPacketTypes packet_type = parser.Begin();
      while (packet_type != RTCPUtility::kRtcpNotValidCode) {
        if (packet_type == RTCPUtility::kRtcpPsfbRembCode) {
          const RTCPUtility::RTCPPacket& packet = parser.Packet();
          EXPECT_EQ(packet.PSFBAPP.SenderSSRC, kReceiverLocalSsrc);
          received_psfb = true;
        } else if (packet_type == RTCPUtility::kRtcpPsfbRembItemCode) {
          const RTCPUtility::RTCPPacket& packet = parser.Packet();
          EXPECT_GT(packet.REMBItem.BitRate, 0u);
          EXPECT_EQ(packet.REMBItem.NumberOfSSRCs, 1u);
          EXPECT_EQ(packet.REMBItem.SSRCs[0], kSendSsrcs[0]);
          received_remb = true;
        }
        packet_type = parser.Iterate();
      }
      if (received_psfb && received_remb)
        observation_complete_->Set();
      return SEND_PACKET;
    }
    void PerformTest() override {
      EXPECT_EQ(kEventSignaled, Wait()) << "Timed out while waiting for a "
                                           "receiver RTCP REMB packet to be "
                                           "sent.";
    }
  } test;

  RunBaseTest(&test);
}

TEST_F(EndToEndTest, VerifyBandwidthStats) {
  class RtcpObserver : public test::EndToEndTest, public PacketReceiver {
   public:
    RtcpObserver()
        : EndToEndTest(kDefaultTimeoutMs),
          sender_call_(nullptr),
          receiver_call_(nullptr),
          has_seen_pacer_delay_(false) {}

    DeliveryStatus DeliverPacket(const uint8_t* packet,
                                 size_t length) override {
      Call::Stats sender_stats = sender_call_->GetStats();
      Call::Stats receiver_stats = receiver_call_->GetStats();
      if (!has_seen_pacer_delay_)
        has_seen_pacer_delay_ = sender_stats.pacer_delay_ms > 0;
      if (sender_stats.send_bandwidth_bps > 0 &&
          receiver_stats.recv_bandwidth_bps > 0 && has_seen_pacer_delay_) {
        observation_complete_->Set();
      }
      return receiver_call_->Receiver()->DeliverPacket(packet, length);
    }

    void OnCallsCreated(Call* sender_call, Call* receiver_call) override {
      sender_call_ = sender_call;
      receiver_call_ = receiver_call;
    }

    void PerformTest() override {
      EXPECT_EQ(kEventSignaled, Wait()) << "Timed out while waiting for "
                                           "non-zero bandwidth stats.";
    }

    void SetReceivers(PacketReceiver* send_transport_receiver,
                      PacketReceiver* receive_transport_receiver) override {
      test::RtpRtcpObserver::SetReceivers(this, receive_transport_receiver);
    }

   private:
    Call* sender_call_;
    Call* receiver_call_;
    bool has_seen_pacer_delay_;
  } test;

  RunBaseTest(&test);
}

TEST_F(EndToEndTest, VerifyNackStats) {
  static const int kPacketNumberToDrop = 200;
  class NackObserver : public test::EndToEndTest {
   public:
    NackObserver()
        : EndToEndTest(kLongTimeoutMs),
          sent_rtp_packets_(0),
          dropped_rtp_packet_(0),
          dropped_rtp_packet_requested_(false),
          send_stream_(nullptr),
          start_runtime_ms_(-1) {}

   private:
    Action OnSendRtp(const uint8_t* packet, size_t length) override {
      if (++sent_rtp_packets_ == kPacketNumberToDrop) {
        rtc::scoped_ptr<RtpHeaderParser> parser(RtpHeaderParser::Create());
        RTPHeader header;
        EXPECT_TRUE(parser->Parse(packet, length, &header));
        dropped_rtp_packet_ = header.sequenceNumber;
        return DROP_PACKET;
      }
      VerifyStats();
      return SEND_PACKET;
    }

    Action OnReceiveRtcp(const uint8_t* packet, size_t length) override {
      test::RtcpPacketParser rtcp_parser;
      rtcp_parser.Parse(packet, length);
      std::vector<uint16_t> nacks = rtcp_parser.nack_item()->last_nack_list();
      if (!nacks.empty() && std::find(
          nacks.begin(), nacks.end(), dropped_rtp_packet_) != nacks.end()) {
        dropped_rtp_packet_requested_ = true;
      }
      return SEND_PACKET;
    }

    void VerifyStats() {
      if (!dropped_rtp_packet_requested_)
        return;
      int send_stream_nack_packets = 0;
      int receive_stream_nack_packets = 0;
      VideoSendStream::Stats stats = send_stream_->GetStats();
      for (std::map<uint32_t, VideoSendStream::StreamStats>::const_iterator it =
           stats.substreams.begin(); it != stats.substreams.end(); ++it) {
        const VideoSendStream::StreamStats& stream_stats = it->second;
        send_stream_nack_packets +=
            stream_stats.rtcp_packet_type_counts.nack_packets;
      }
      for (size_t i = 0; i < receive_streams_.size(); ++i) {
        VideoReceiveStream::Stats stats = receive_streams_[i]->GetStats();
        receive_stream_nack_packets +=
            stats.rtcp_packet_type_counts.nack_packets;
      }
      if (send_stream_nack_packets >= 1 && receive_stream_nack_packets >= 1) {
        // NACK packet sent on receive stream and received on sent stream.
        if (MinMetricRunTimePassed())
          observation_complete_->Set();
      }
    }

    bool MinMetricRunTimePassed() {
      int64_t now = Clock::GetRealTimeClock()->TimeInMilliseconds();
      if (start_runtime_ms_ == -1) {
        start_runtime_ms_ = now;
        return false;
      }
      int64_t elapsed_sec = (now - start_runtime_ms_) / 1000;
      return elapsed_sec > metrics::kMinRunTimeInSeconds;
    }

    void ModifyConfigs(VideoSendStream::Config* send_config,
                       std::vector<VideoReceiveStream::Config>* receive_configs,
                       VideoEncoderConfig* encoder_config) override {
      send_config->rtp.nack.rtp_history_ms = kNackRtpHistoryMs;
      (*receive_configs)[0].rtp.nack.rtp_history_ms = kNackRtpHistoryMs;
    }

    void OnStreamsCreated(
        VideoSendStream* send_stream,
        const std::vector<VideoReceiveStream*>& receive_streams) override {
      send_stream_ = send_stream;
      receive_streams_ = receive_streams;
    }

    void PerformTest() override {
      EXPECT_EQ(kEventSignaled, Wait())
          << "Timed out waiting for packet to be NACKed.";
    }

    uint64_t sent_rtp_packets_;
    uint16_t dropped_rtp_packet_;
    bool dropped_rtp_packet_requested_;
    std::vector<VideoReceiveStream*> receive_streams_;
    VideoSendStream* send_stream_;
    int64_t start_runtime_ms_;
  } test;

  RunBaseTest(&test);

  EXPECT_NE(-1, test::LastHistogramSample(
      "WebRTC.Video.UniqueNackRequestsSentInPercent"));
  EXPECT_NE(-1, test::LastHistogramSample(
      "WebRTC.Video.UniqueNackRequestsReceivedInPercent"));
  EXPECT_GT(test::LastHistogramSample(
      "WebRTC.Video.NackPacketsSentPerMinute"), 0);
  EXPECT_GT(test::LastHistogramSample(
      "WebRTC.Video.NackPacketsReceivedPerMinute"), 0);
}

void EndToEndTest::TestXrReceiverReferenceTimeReport(bool enable_rrtr) {
  static const int kNumRtcpReportPacketsToObserve = 5;
  class RtcpXrObserver : public test::EndToEndTest {
   public:
    explicit RtcpXrObserver(bool enable_rrtr)
        : EndToEndTest(kDefaultTimeoutMs),
          enable_rrtr_(enable_rrtr),
          sent_rtcp_sr_(0),
          sent_rtcp_rr_(0),
          sent_rtcp_rrtr_(0),
          sent_rtcp_dlrr_(0) {}

   private:
    // Receive stream should send RR packets (and RRTR packets if enabled).
    Action OnReceiveRtcp(const uint8_t* packet, size_t length) override {
      RTCPUtility::RTCPParserV2 parser(packet, length, true);
      EXPECT_TRUE(parser.IsValid());

      RTCPUtility::RTCPPacketTypes packet_type = parser.Begin();
      while (packet_type != RTCPUtility::kRtcpNotValidCode) {
        if (packet_type == RTCPUtility::kRtcpRrCode) {
          ++sent_rtcp_rr_;
        } else if (packet_type ==
                   RTCPUtility::kRtcpXrReceiverReferenceTimeCode) {
          ++sent_rtcp_rrtr_;
        }
        EXPECT_NE(packet_type, RTCPUtility::kRtcpSrCode);
        EXPECT_NE(packet_type, RTCPUtility::kRtcpXrDlrrReportBlockItemCode);
        packet_type = parser.Iterate();
      }
      return SEND_PACKET;
    }
    // Send stream should send SR packets (and DLRR packets if enabled).
    virtual Action OnSendRtcp(const uint8_t* packet, size_t length) {
      RTCPUtility::RTCPParserV2 parser(packet, length, true);
      EXPECT_TRUE(parser.IsValid());

      RTCPUtility::RTCPPacketTypes packet_type = parser.Begin();
      while (packet_type != RTCPUtility::kRtcpNotValidCode) {
        if (packet_type == RTCPUtility::kRtcpSrCode) {
          ++sent_rtcp_sr_;
        } else if (packet_type == RTCPUtility::kRtcpXrDlrrReportBlockItemCode) {
          ++sent_rtcp_dlrr_;
        }
        EXPECT_NE(packet_type, RTCPUtility::kRtcpXrReceiverReferenceTimeCode);
        packet_type = parser.Iterate();
      }
      if (sent_rtcp_sr_ > kNumRtcpReportPacketsToObserve &&
          sent_rtcp_rr_ > kNumRtcpReportPacketsToObserve) {
        if (enable_rrtr_) {
          EXPECT_GT(sent_rtcp_rrtr_, 0);
          EXPECT_GT(sent_rtcp_dlrr_, 0);
        } else {
          EXPECT_EQ(0, sent_rtcp_rrtr_);
          EXPECT_EQ(0, sent_rtcp_dlrr_);
        }
        observation_complete_->Set();
      }
      return SEND_PACKET;
    }

    void ModifyConfigs(VideoSendStream::Config* send_config,
                       std::vector<VideoReceiveStream::Config>* receive_configs,
                       VideoEncoderConfig* encoder_config) override {
      (*receive_configs)[0].rtp.rtcp_mode = newapi::kRtcpReducedSize;
      (*receive_configs)[0].rtp.rtcp_xr.receiver_reference_time_report =
          enable_rrtr_;
    }

    void PerformTest() override {
      EXPECT_EQ(kEventSignaled, Wait())
          << "Timed out while waiting for RTCP SR/RR packets to be sent.";
    }

    bool enable_rrtr_;
    int sent_rtcp_sr_;
    int sent_rtcp_rr_;
    int sent_rtcp_rrtr_;
    int sent_rtcp_dlrr_;
  } test(enable_rrtr);

  RunBaseTest(&test);
}

void EndToEndTest::TestSendsSetSsrcs(size_t num_ssrcs,
                                     bool send_single_ssrc_first) {
  class SendsSetSsrcs : public test::EndToEndTest {
   public:
    SendsSetSsrcs(const uint32_t* ssrcs,
                  size_t num_ssrcs,
                  bool send_single_ssrc_first)
        : EndToEndTest(kDefaultTimeoutMs),
          num_ssrcs_(num_ssrcs),
          send_single_ssrc_first_(send_single_ssrc_first),
          ssrcs_to_observe_(num_ssrcs),
          expect_single_ssrc_(send_single_ssrc_first) {
      for (size_t i = 0; i < num_ssrcs; ++i)
        valid_ssrcs_[ssrcs[i]] = true;
    }

   private:
    Action OnSendRtp(const uint8_t* packet, size_t length) override {
      RTPHeader header;
      EXPECT_TRUE(parser_->Parse(packet, length, &header));

      EXPECT_TRUE(valid_ssrcs_[header.ssrc])
          << "Received unknown SSRC: " << header.ssrc;

      if (!valid_ssrcs_[header.ssrc])
        observation_complete_->Set();

      if (!is_observed_[header.ssrc]) {
        is_observed_[header.ssrc] = true;
        --ssrcs_to_observe_;
        if (expect_single_ssrc_) {
          expect_single_ssrc_ = false;
          observation_complete_->Set();
        }
      }

      if (ssrcs_to_observe_ == 0)
        observation_complete_->Set();

      return SEND_PACKET;
    }

    size_t GetNumStreams() const override { return num_ssrcs_; }

    void ModifyConfigs(VideoSendStream::Config* send_config,
                       std::vector<VideoReceiveStream::Config>* receive_configs,
                       VideoEncoderConfig* encoder_config) override {
      if (num_ssrcs_ > 1) {
        // Set low simulcast bitrates to not have to wait for bandwidth ramp-up.
        for (size_t i = 0; i < encoder_config->streams.size(); ++i) {
          encoder_config->streams[i].min_bitrate_bps = 10000;
          encoder_config->streams[i].target_bitrate_bps = 15000;
          encoder_config->streams[i].max_bitrate_bps = 20000;
        }
      }

      encoder_config_all_streams_ = *encoder_config;
      if (send_single_ssrc_first_)
        encoder_config->streams.resize(1);
    }

    void OnStreamsCreated(
        VideoSendStream* send_stream,
        const std::vector<VideoReceiveStream*>& receive_streams) override {
      send_stream_ = send_stream;
    }

    void PerformTest() override {
      EXPECT_EQ(kEventSignaled, Wait())
          << "Timed out while waiting for "
          << (send_single_ssrc_first_ ? "first SSRC." : "SSRCs.");

      if (send_single_ssrc_first_) {
        // Set full simulcast and continue with the rest of the SSRCs.
        send_stream_->ReconfigureVideoEncoder(encoder_config_all_streams_);
        EXPECT_EQ(kEventSignaled, Wait())
            << "Timed out while waiting on additional SSRCs.";
      }
    }

   private:
    std::map<uint32_t, bool> valid_ssrcs_;
    std::map<uint32_t, bool> is_observed_;

    const size_t num_ssrcs_;
    const bool send_single_ssrc_first_;

    size_t ssrcs_to_observe_;
    bool expect_single_ssrc_;

    VideoSendStream* send_stream_;
    VideoEncoderConfig encoder_config_all_streams_;
  } test(kSendSsrcs, num_ssrcs, send_single_ssrc_first);

  RunBaseTest(&test);
}

TEST_F(EndToEndTest, ReportsSetEncoderRates) {
  class EncoderRateStatsTest : public test::EndToEndTest,
                               public test::FakeEncoder {
   public:
    EncoderRateStatsTest()
        : EndToEndTest(kDefaultTimeoutMs),
          FakeEncoder(Clock::GetRealTimeClock()) {}

    void OnStreamsCreated(
        VideoSendStream* send_stream,
        const std::vector<VideoReceiveStream*>& receive_streams) override {
      send_stream_ = send_stream;
    }

    void ModifyConfigs(VideoSendStream::Config* send_config,
                       std::vector<VideoReceiveStream::Config>* receive_configs,
                       VideoEncoderConfig* encoder_config) override {
      send_config->encoder_settings.encoder = this;
    }

    int32_t SetRates(uint32_t new_target_bitrate, uint32_t framerate) override {
      // Make sure not to trigger on any default zero bitrates.
      if (new_target_bitrate == 0)
        return 0;
      CriticalSectionScoped lock(crit_.get());
      bitrate_kbps_ = new_target_bitrate;
      observation_complete_->Set();
      return 0;
    }

    void PerformTest() override {
      ASSERT_EQ(kEventSignaled, Wait())
          << "Timed out while waiting for encoder SetRates() call.";
      // Wait for GetStats to report a corresponding bitrate.
      for (unsigned int i = 0; i < kDefaultTimeoutMs; ++i) {
        VideoSendStream::Stats stats = send_stream_->GetStats();
        {
          CriticalSectionScoped lock(crit_.get());
          if ((stats.target_media_bitrate_bps + 500) / 1000 ==
              static_cast<int>(bitrate_kbps_)) {
            return;
          }
        }
        SleepMs(1);
      }
      FAIL()
          << "Timed out waiting for stats reporting the currently set bitrate.";
    }

   private:
    VideoSendStream* send_stream_;
    uint32_t bitrate_kbps_ GUARDED_BY(crit_);
  } test;

  RunBaseTest(&test);
}

TEST_F(EndToEndTest, GetStats) {
  static const int kStartBitrateBps = 3000000;
  class StatsObserver : public test::EndToEndTest, public I420FrameCallback {
   public:
    explicit StatsObserver(const FakeNetworkPipe::Config& config)
        : EndToEndTest(kLongTimeoutMs, config),
          send_stream_(nullptr),
          expected_send_ssrcs_(),
          check_stats_event_(EventWrapper::Create()) {}

   private:
    Action OnSendRtp(const uint8_t* packet, size_t length) override {
      check_stats_event_->Set();
      return SEND_PACKET;
    }

    Action OnSendRtcp(const uint8_t* packet, size_t length) override {
      check_stats_event_->Set();
      return SEND_PACKET;
    }

    Action OnReceiveRtp(const uint8_t* packet, size_t length) override {
      check_stats_event_->Set();
      return SEND_PACKET;
    }

    Action OnReceiveRtcp(const uint8_t* packet, size_t length) override {
      check_stats_event_->Set();
      return SEND_PACKET;
    }

    void FrameCallback(I420VideoFrame* video_frame) override {
      // Ensure that we have at least 5ms send side delay.
      int64_t render_time = video_frame->render_time_ms();
      if (render_time > 0)
        video_frame->set_render_time_ms(render_time - 5);
    }

    bool CheckReceiveStats() {
      for (size_t i = 0; i < receive_streams_.size(); ++i) {
        VideoReceiveStream::Stats stats = receive_streams_[i]->GetStats();
        EXPECT_EQ(expected_receive_ssrcs_[i], stats.ssrc);

        // Make sure all fields have been populated.
        // TODO(pbos): Use CompoundKey if/when we ever know that all stats are
        // always filled for all receivers.
        receive_stats_filled_["IncomingRate"] |=
            stats.network_frame_rate != 0 || stats.total_bitrate_bps != 0;

        receive_stats_filled_["FrameCallback"] |= stats.decode_frame_rate != 0;

        receive_stats_filled_["FrameRendered"] |= stats.render_frame_rate != 0;

        receive_stats_filled_["StatisticsUpdated"] |=
            stats.rtcp_stats.cumulative_lost != 0 ||
            stats.rtcp_stats.extended_max_sequence_number != 0 ||
            stats.rtcp_stats.fraction_lost != 0 || stats.rtcp_stats.jitter != 0;

        receive_stats_filled_["DataCountersUpdated"] |=
            stats.rtp_stats.transmitted.payload_bytes != 0 ||
            stats.rtp_stats.fec.packets != 0 ||
            stats.rtp_stats.transmitted.header_bytes != 0 ||
            stats.rtp_stats.transmitted.packets != 0 ||
            stats.rtp_stats.transmitted.padding_bytes != 0 ||
            stats.rtp_stats.retransmitted.packets != 0;

        receive_stats_filled_["CodecStats"] |=
            stats.target_delay_ms != 0 || stats.discarded_packets != 0;

        receive_stats_filled_["FrameCounts"] |=
            stats.frame_counts.key_frames != 0 ||
            stats.frame_counts.delta_frames != 0;

        receive_stats_filled_["CName"] |= stats.c_name != "";

        receive_stats_filled_["RtcpPacketTypeCount"] |=
            stats.rtcp_packet_type_counts.fir_packets != 0 ||
            stats.rtcp_packet_type_counts.nack_packets != 0 ||
            stats.rtcp_packet_type_counts.pli_packets != 0 ||
            stats.rtcp_packet_type_counts.nack_requests != 0 ||
            stats.rtcp_packet_type_counts.unique_nack_requests != 0;
      }

      return AllStatsFilled(receive_stats_filled_);
    }

    bool CheckSendStats() {
      DCHECK(send_stream_ != nullptr);
      VideoSendStream::Stats stats = send_stream_->GetStats();

      send_stats_filled_["NumStreams"] |=
          stats.substreams.size() == expected_send_ssrcs_.size();

      send_stats_filled_["CpuOveruseMetrics"] |=
          stats.avg_encode_time_ms != 0 || stats.encode_usage_percent != 0;

      for (std::map<uint32_t, VideoSendStream::StreamStats>::const_iterator it =
               stats.substreams.begin();
           it != stats.substreams.end(); ++it) {
        EXPECT_TRUE(expected_send_ssrcs_.find(it->first) !=
                    expected_send_ssrcs_.end());

        send_stats_filled_[CompoundKey("CapturedFrameRate", it->first)] |=
            stats.input_frame_rate != 0;

        const VideoSendStream::StreamStats& stream_stats = it->second;

        send_stats_filled_[CompoundKey("StatisticsUpdated", it->first)] |=
            stream_stats.rtcp_stats.cumulative_lost != 0 ||
            stream_stats.rtcp_stats.extended_max_sequence_number != 0 ||
            stream_stats.rtcp_stats.fraction_lost != 0;

        send_stats_filled_[CompoundKey("DataCountersUpdated", it->first)] |=
            stream_stats.rtp_stats.fec.packets != 0 ||
            stream_stats.rtp_stats.transmitted.padding_bytes != 0 ||
            stream_stats.rtp_stats.retransmitted.packets != 0 ||
            stream_stats.rtp_stats.transmitted.packets != 0;

        send_stats_filled_[CompoundKey("BitrateStatisticsObserver",
                                       it->first)] |=
            stream_stats.total_bitrate_bps != 0;

        send_stats_filled_[CompoundKey("FrameCountObserver", it->first)] |=
            stream_stats.frame_counts.delta_frames != 0 ||
            stream_stats.frame_counts.key_frames != 0;

        send_stats_filled_[CompoundKey("OutgoingRate", it->first)] |=
            stats.encode_frame_rate != 0;

        send_stats_filled_[CompoundKey("Delay", it->first)] |=
            stream_stats.avg_delay_ms != 0 || stream_stats.max_delay_ms != 0;

        // TODO(pbos): Use CompoundKey when the test makes sure that all SSRCs
        // report dropped packets.
        send_stats_filled_["RtcpPacketTypeCount"] |=
            stream_stats.rtcp_packet_type_counts.fir_packets != 0 ||
            stream_stats.rtcp_packet_type_counts.nack_packets != 0 ||
            stream_stats.rtcp_packet_type_counts.pli_packets != 0 ||
            stream_stats.rtcp_packet_type_counts.nack_requests != 0 ||
            stream_stats.rtcp_packet_type_counts.unique_nack_requests != 0;
      }

      return AllStatsFilled(send_stats_filled_);
    }

    std::string CompoundKey(const char* name, uint32_t ssrc) {
      std::ostringstream oss;
      oss << name << "_" << ssrc;
      return oss.str();
    }

    bool AllStatsFilled(const std::map<std::string, bool>& stats_map) {
      for (std::map<std::string, bool>::const_iterator it = stats_map.begin();
           it != stats_map.end();
           ++it) {
        if (!it->second)
          return false;
      }
      return true;
    }

    Call::Config GetSenderCallConfig() override {
      Call::Config config = EndToEndTest::GetSenderCallConfig();
      config.bitrate_config.start_bitrate_bps = kStartBitrateBps;
      return config;
    }

    void ModifyConfigs(VideoSendStream::Config* send_config,
                       std::vector<VideoReceiveStream::Config>* receive_configs,
                       VideoEncoderConfig* encoder_config) override {
      send_config->pre_encode_callback = this;  // Used to inject delay.
      expected_cname_ = send_config->rtp.c_name = "SomeCName";

      const std::vector<uint32_t>& ssrcs = send_config->rtp.ssrcs;
      for (size_t i = 0; i < ssrcs.size(); ++i) {
        expected_send_ssrcs_.insert(ssrcs[i]);
        expected_receive_ssrcs_.push_back(
            (*receive_configs)[i].rtp.remote_ssrc);
      }
    }

    size_t GetNumStreams() const override { return kNumSsrcs; }

    void OnStreamsCreated(
        VideoSendStream* send_stream,
        const std::vector<VideoReceiveStream*>& receive_streams) override {
      send_stream_ = send_stream;
      receive_streams_ = receive_streams;
    }

    void PerformTest() override {
      Clock* clock = Clock::GetRealTimeClock();
      int64_t now = clock->TimeInMilliseconds();
      int64_t stop_time = now + test::CallTest::kLongTimeoutMs;
      bool receive_ok = false;
      bool send_ok = false;

      while (now < stop_time) {
        if (!receive_ok)
          receive_ok = CheckReceiveStats();
        if (!send_ok)
          send_ok = CheckSendStats();

        if (receive_ok && send_ok)
          return;

        int64_t time_until_timout_ = stop_time - now;
        if (time_until_timout_ > 0)
          check_stats_event_->Wait(time_until_timout_);
        now = clock->TimeInMilliseconds();
      }

      ADD_FAILURE() << "Timed out waiting for filled stats.";
      for (std::map<std::string, bool>::const_iterator it =
               receive_stats_filled_.begin();
           it != receive_stats_filled_.end();
           ++it) {
        if (!it->second) {
          ADD_FAILURE() << "Missing receive stats: " << it->first;
        }
      }

      for (std::map<std::string, bool>::const_iterator it =
               send_stats_filled_.begin();
           it != send_stats_filled_.end();
           ++it) {
        if (!it->second) {
          ADD_FAILURE() << "Missing send stats: " << it->first;
        }
      }
    }

    std::vector<VideoReceiveStream*> receive_streams_;
    std::map<std::string, bool> receive_stats_filled_;

    VideoSendStream* send_stream_;
    std::map<std::string, bool> send_stats_filled_;

    std::vector<uint32_t> expected_receive_ssrcs_;
    std::set<uint32_t> expected_send_ssrcs_;
    std::string expected_cname_;

    rtc::scoped_ptr<EventWrapper> check_stats_event_;
  };

  FakeNetworkPipe::Config network_config;
  network_config.loss_percent = 5;

  StatsObserver test(network_config);
  RunBaseTest(&test);
}

TEST_F(EndToEndTest, ReceiverReferenceTimeReportEnabled) {
  TestXrReceiverReferenceTimeReport(true);
}

TEST_F(EndToEndTest, ReceiverReferenceTimeReportDisabled) {
  TestXrReceiverReferenceTimeReport(false);
}

TEST_F(EndToEndTest, TestReceivedRtpPacketStats) {
  static const size_t kNumRtpPacketsToSend = 5;
  class ReceivedRtpStatsObserver : public test::EndToEndTest {
   public:
    ReceivedRtpStatsObserver()
        : EndToEndTest(kDefaultTimeoutMs),
          receive_stream_(nullptr),
          sent_rtp_(0) {}

   private:
    void OnStreamsCreated(
        VideoSendStream* send_stream,
        const std::vector<VideoReceiveStream*>& receive_streams) override {
      receive_stream_ = receive_streams[0];
    }

    Action OnSendRtp(const uint8_t* packet, size_t length) override {
      if (sent_rtp_ >= kNumRtpPacketsToSend) {
        VideoReceiveStream::Stats stats = receive_stream_->GetStats();
        if (kNumRtpPacketsToSend == stats.rtp_stats.transmitted.packets) {
          observation_complete_->Set();
        }
        return DROP_PACKET;
      }
      ++sent_rtp_;
      return SEND_PACKET;
    }

    void PerformTest() override {
      EXPECT_EQ(kEventSignaled, Wait())
          << "Timed out while verifying number of received RTP packets.";
    }

    VideoReceiveStream* receive_stream_;
    uint32_t sent_rtp_;
  } test;

  RunBaseTest(&test);
}

TEST_F(EndToEndTest, SendsSetSsrc) { TestSendsSetSsrcs(1, false); }

TEST_F(EndToEndTest, SendsSetSimulcastSsrcs) {
  TestSendsSetSsrcs(kNumSsrcs, false);
}

TEST_F(EndToEndTest, CanSwitchToUseAllSsrcs) {
  TestSendsSetSsrcs(kNumSsrcs, true);
}

TEST_F(EndToEndTest, DISABLED_RedundantPayloadsTransmittedOnAllSsrcs) {
  class ObserveRedundantPayloads: public test::EndToEndTest {
   public:
    ObserveRedundantPayloads()
        : EndToEndTest(kDefaultTimeoutMs), ssrcs_to_observe_(kNumSsrcs) {
          for (size_t i = 0; i < kNumSsrcs; ++i) {
            registered_rtx_ssrc_[kSendRtxSsrcs[i]] = true;
          }
        }

   private:
    Action OnSendRtp(const uint8_t* packet, size_t length) override {
      RTPHeader header;
      EXPECT_TRUE(parser_->Parse(packet, length, &header));

      if (!registered_rtx_ssrc_[header.ssrc])
        return SEND_PACKET;

      EXPECT_LE(header.headerLength + header.paddingLength, length);
      const bool packet_is_redundant_payload =
          header.headerLength + header.paddingLength < length;

      if (!packet_is_redundant_payload)
        return SEND_PACKET;

      if (!observed_redundant_retransmission_[header.ssrc]) {
        observed_redundant_retransmission_[header.ssrc] = true;
        if (--ssrcs_to_observe_ == 0)
          observation_complete_->Set();
      }

      return SEND_PACKET;
    }

    size_t GetNumStreams() const override { return kNumSsrcs; }

    void ModifyConfigs(VideoSendStream::Config* send_config,
                       std::vector<VideoReceiveStream::Config>* receive_configs,
                       VideoEncoderConfig* encoder_config) override {
      // Set low simulcast bitrates to not have to wait for bandwidth ramp-up.
      for (size_t i = 0; i < encoder_config->streams.size(); ++i) {
        encoder_config->streams[i].min_bitrate_bps = 10000;
        encoder_config->streams[i].target_bitrate_bps = 15000;
        encoder_config->streams[i].max_bitrate_bps = 20000;
      }

      send_config->rtp.rtx.payload_type = kSendRtxPayloadType;

      for (size_t i = 0; i < kNumSsrcs; ++i)
        send_config->rtp.rtx.ssrcs.push_back(kSendRtxSsrcs[i]);

      // Significantly higher than max bitrates for all video streams -> forcing
      // padding to trigger redundant padding on all RTX SSRCs.
      encoder_config->min_transmit_bitrate_bps = 100000;
    }

    void PerformTest() override {
      EXPECT_EQ(kEventSignaled, Wait())
          << "Timed out while waiting for redundant payloads on all SSRCs.";
    }

   private:
    size_t ssrcs_to_observe_;
    std::map<uint32_t, bool> observed_redundant_retransmission_;
    std::map<uint32_t, bool> registered_rtx_ssrc_;
  } test;

  RunBaseTest(&test);
}

void EndToEndTest::TestRtpStatePreservation(bool use_rtx) {
  static const uint32_t kMaxSequenceNumberGap = 100;
  static const uint64_t kMaxTimestampGap = kDefaultTimeoutMs * 90;
  class RtpSequenceObserver : public test::RtpRtcpObserver {
   public:
    explicit RtpSequenceObserver(bool use_rtx)
        : test::RtpRtcpObserver(kDefaultTimeoutMs),
          crit_(CriticalSectionWrapper::CreateCriticalSection()),
          ssrcs_to_observe_(kNumSsrcs) {
      for (size_t i = 0; i < kNumSsrcs; ++i) {
        configured_ssrcs_[kSendSsrcs[i]] = true;
        if (use_rtx)
          configured_ssrcs_[kSendRtxSsrcs[i]] = true;
      }
    }

    void ResetExpectedSsrcs(size_t num_expected_ssrcs) {
      CriticalSectionScoped lock(crit_.get());
      ssrc_observed_.clear();
      ssrcs_to_observe_ = num_expected_ssrcs;
    }

   private:
    Action OnSendRtp(const uint8_t* packet, size_t length) override {
      RTPHeader header;
      EXPECT_TRUE(parser_->Parse(packet, length, &header));
      const uint32_t ssrc = header.ssrc;
      const uint16_t sequence_number = header.sequenceNumber;
      const uint32_t timestamp = header.timestamp;
      const bool only_padding =
          header.headerLength + header.paddingLength == length;

      EXPECT_TRUE(configured_ssrcs_[ssrc])
          << "Received SSRC that wasn't configured: " << ssrc;

      std::map<uint32_t, uint16_t>::iterator it =
          last_observed_sequence_number_.find(header.ssrc);
      if (it == last_observed_sequence_number_.end()) {
        last_observed_sequence_number_[ssrc] = sequence_number;
        last_observed_timestamp_[ssrc] = timestamp;
      } else {
        // Verify sequence numbers are reasonably close.
        uint32_t extended_sequence_number = sequence_number;
        // Check for roll-over.
        if (sequence_number < last_observed_sequence_number_[ssrc])
          extended_sequence_number += 0xFFFFu + 1;
        EXPECT_LE(
            extended_sequence_number - last_observed_sequence_number_[ssrc],
            kMaxSequenceNumberGap)
            << "Gap in sequence numbers ("
            << last_observed_sequence_number_[ssrc] << " -> " << sequence_number
            << ") too large for SSRC: " << ssrc << ".";
        last_observed_sequence_number_[ssrc] = sequence_number;

        // TODO(pbos): Remove this check if we ever have monotonically
        // increasing timestamps. Right now padding packets add a delta which
        // can cause reordering between padding packets and regular packets,
        // hence we drop padding-only packets to not flake.
        if (only_padding) {
          // Verify that timestamps are reasonably close.
          uint64_t extended_timestamp = timestamp;
          // Check for roll-over.
          if (timestamp < last_observed_timestamp_[ssrc])
            extended_timestamp += static_cast<uint64_t>(0xFFFFFFFFu) + 1;
          EXPECT_LE(extended_timestamp - last_observed_timestamp_[ssrc],
                    kMaxTimestampGap)
              << "Gap in timestamps (" << last_observed_timestamp_[ssrc]
              << " -> " << timestamp << ") too large for SSRC: " << ssrc << ".";
        }
        last_observed_timestamp_[ssrc] = timestamp;
      }

      CriticalSectionScoped lock(crit_.get());
      // Wait for media packets on all ssrcs.
      if (!ssrc_observed_[ssrc] && !only_padding) {
        ssrc_observed_[ssrc] = true;
        if (--ssrcs_to_observe_ == 0)
          observation_complete_->Set();
      }

      return SEND_PACKET;
    }

    std::map<uint32_t, uint16_t> last_observed_sequence_number_;
    std::map<uint32_t, uint32_t> last_observed_timestamp_;
    std::map<uint32_t, bool> configured_ssrcs_;

    rtc::scoped_ptr<CriticalSectionWrapper> crit_;
    size_t ssrcs_to_observe_ GUARDED_BY(crit_);
    std::map<uint32_t, bool> ssrc_observed_ GUARDED_BY(crit_);
  } observer(use_rtx);

  CreateCalls(Call::Config(observer.SendTransport()),
              Call::Config(observer.ReceiveTransport()));
  observer.SetReceivers(sender_call_->Receiver(), nullptr);

  CreateSendConfig(kNumSsrcs);

  if (use_rtx) {
    for (size_t i = 0; i < kNumSsrcs; ++i) {
      send_config_.rtp.rtx.ssrcs.push_back(kSendRtxSsrcs[i]);
    }
    send_config_.rtp.rtx.payload_type = kSendRtxPayloadType;
  }

  // Lower bitrates so that all streams send initially.
  for (size_t i = 0; i < encoder_config_.streams.size(); ++i) {
    encoder_config_.streams[i].min_bitrate_bps = 10000;
    encoder_config_.streams[i].target_bitrate_bps = 15000;
    encoder_config_.streams[i].max_bitrate_bps = 20000;
  }

  // Use the same total bitrates when sending a single stream to avoid lowering
  // the bitrate estimate and requiring a subsequent rampup.
  VideoEncoderConfig one_stream = encoder_config_;
  one_stream.streams.resize(1);
  for (size_t i = 1; i < encoder_config_.streams.size(); ++i) {
    one_stream.streams.front().min_bitrate_bps +=
        encoder_config_.streams[i].min_bitrate_bps;
    one_stream.streams.front().target_bitrate_bps +=
        encoder_config_.streams[i].target_bitrate_bps;
    one_stream.streams.front().max_bitrate_bps +=
        encoder_config_.streams[i].max_bitrate_bps;
  }

  CreateMatchingReceiveConfigs();

  CreateStreams();
  CreateFrameGeneratorCapturer();

  Start();
  EXPECT_EQ(kEventSignaled, observer.Wait())
      << "Timed out waiting for all SSRCs to send packets.";

  // Test stream resetting more than once to make sure that the state doesn't
  // get set once (this could be due to using std::map::insert for instance).
  for (size_t i = 0; i < 3; ++i) {
    frame_generator_capturer_->Stop();
    sender_call_->DestroyVideoSendStream(send_stream_);

    // Re-create VideoSendStream with only one stream.
    send_stream_ =
        sender_call_->CreateVideoSendStream(send_config_, one_stream);
    send_stream_->Start();
    CreateFrameGeneratorCapturer();
    frame_generator_capturer_->Start();

    observer.ResetExpectedSsrcs(1);
    EXPECT_EQ(kEventSignaled, observer.Wait())
        << "Timed out waiting for single RTP packet.";

    // Reconfigure back to use all streams.
    send_stream_->ReconfigureVideoEncoder(encoder_config_);
    observer.ResetExpectedSsrcs(kNumSsrcs);
    EXPECT_EQ(kEventSignaled, observer.Wait())
        << "Timed out waiting for all SSRCs to send packets.";

    // Reconfigure down to one stream.
    send_stream_->ReconfigureVideoEncoder(one_stream);
    observer.ResetExpectedSsrcs(1);
    EXPECT_EQ(kEventSignaled, observer.Wait())
        << "Timed out waiting for single RTP packet.";

    // Reconfigure back to use all streams.
    send_stream_->ReconfigureVideoEncoder(encoder_config_);
    observer.ResetExpectedSsrcs(kNumSsrcs);
    EXPECT_EQ(kEventSignaled, observer.Wait())
        << "Timed out waiting for all SSRCs to send packets.";
  }

  observer.StopSending();

  Stop();
  DestroyStreams();
}

TEST_F(EndToEndTest, DISABLED_RestartingSendStreamPreservesRtpState) {
  TestRtpStatePreservation(false);
}

TEST_F(EndToEndTest, RestartingSendStreamPreservesRtpStatesWithRtx) {
  TestRtpStatePreservation(true);
}

TEST_F(EndToEndTest, RespectsNetworkState) {
  // TODO(pbos): Remove accepted downtime packets etc. when signaling network
  // down blocks until no more packets will be sent.

  // Pacer will send from its packet list and then send required padding before
  // checking paused_ again. This should be enough for one round of pacing,
  // otherwise increase.
  static const int kNumAcceptedDowntimeRtp = 5;
  // A single RTCP may be in the pipeline.
  static const int kNumAcceptedDowntimeRtcp = 1;
  class NetworkStateTest : public test::EndToEndTest, public test::FakeEncoder {
   public:
    NetworkStateTest()
        : EndToEndTest(kDefaultTimeoutMs),
          FakeEncoder(Clock::GetRealTimeClock()),
          test_crit_(CriticalSectionWrapper::CreateCriticalSection()),
          encoded_frames_(EventWrapper::Create()),
          packet_event_(EventWrapper::Create()),
          sender_state_(Call::kNetworkUp),
          sender_rtp_(0),
          sender_rtcp_(0),
          receiver_rtcp_(0),
          down_frames_(0) {}

    Action OnSendRtp(const uint8_t* packet, size_t length) override {
      CriticalSectionScoped lock(test_crit_.get());
      ++sender_rtp_;
      packet_event_->Set();
      return SEND_PACKET;
    }

    Action OnSendRtcp(const uint8_t* packet, size_t length) override {
      CriticalSectionScoped lock(test_crit_.get());
      ++sender_rtcp_;
      packet_event_->Set();
      return SEND_PACKET;
    }

    Action OnReceiveRtp(const uint8_t* packet, size_t length) override {
      ADD_FAILURE() << "Unexpected receiver RTP, should not be sending.";
      return SEND_PACKET;
    }

    Action OnReceiveRtcp(const uint8_t* packet, size_t length) override {
      CriticalSectionScoped lock(test_crit_.get());
      ++receiver_rtcp_;
      packet_event_->Set();
      return SEND_PACKET;
    }

    void OnCallsCreated(Call* sender_call, Call* receiver_call) override {
      sender_call_ = sender_call;
      receiver_call_ = receiver_call;
    }

    void ModifyConfigs(VideoSendStream::Config* send_config,
                       std::vector<VideoReceiveStream::Config>* receive_configs,
                       VideoEncoderConfig* encoder_config) override {
      send_config->encoder_settings.encoder = this;
    }

    void PerformTest() override {
      EXPECT_EQ(kEventSignaled, encoded_frames_->Wait(kDefaultTimeoutMs))
          << "No frames received by the encoder.";
      // Wait for packets from both sender/receiver.
      WaitForPacketsOrSilence(false, false);

      // Sender-side network down.
      sender_call_->SignalNetworkState(Call::kNetworkDown);
      {
        CriticalSectionScoped lock(test_crit_.get());
        // After network goes down we shouldn't be encoding more frames.
        sender_state_ = Call::kNetworkDown;
      }
      // Wait for receiver-packets and no sender packets.
      WaitForPacketsOrSilence(true, false);

      // Receiver-side network down.
      receiver_call_->SignalNetworkState(Call::kNetworkDown);
      WaitForPacketsOrSilence(true, true);

      // Network back up again for both.
      {
        CriticalSectionScoped lock(test_crit_.get());
        // It's OK to encode frames again, as we're about to bring up the
        // network.
        sender_state_ = Call::kNetworkUp;
      }
      sender_call_->SignalNetworkState(Call::kNetworkUp);
      receiver_call_->SignalNetworkState(Call::kNetworkUp);
      WaitForPacketsOrSilence(false, false);
    }

    int32_t Encode(const I420VideoFrame& input_image,
                   const CodecSpecificInfo* codec_specific_info,
                   const std::vector<VideoFrameType>* frame_types) override {
      {
        CriticalSectionScoped lock(test_crit_.get());
        if (sender_state_ == Call::kNetworkDown) {
          ++down_frames_;
          EXPECT_LE(down_frames_, 1)
              << "Encoding more than one frame while network is down.";
          if (down_frames_ > 1)
            encoded_frames_->Set();
        } else {
          encoded_frames_->Set();
        }
      }
      return test::FakeEncoder::Encode(
          input_image, codec_specific_info, frame_types);
    }

   private:
    void WaitForPacketsOrSilence(bool sender_down, bool receiver_down) {
      int64_t initial_time_ms = clock_->TimeInMilliseconds();
      int initial_sender_rtp;
      int initial_sender_rtcp;
      int initial_receiver_rtcp;
      {
        CriticalSectionScoped lock(test_crit_.get());
        initial_sender_rtp = sender_rtp_;
        initial_sender_rtcp = sender_rtcp_;
        initial_receiver_rtcp = receiver_rtcp_;
      }
      bool sender_done = false;
      bool receiver_done = false;
      while(!sender_done || !receiver_done) {
        packet_event_->Wait(kSilenceTimeoutMs);
        int64_t time_now_ms = clock_->TimeInMilliseconds();
        CriticalSectionScoped lock(test_crit_.get());
        if (sender_down) {
          ASSERT_LE(sender_rtp_ - initial_sender_rtp, kNumAcceptedDowntimeRtp)
              << "RTP sent during sender-side downtime.";
          ASSERT_LE(sender_rtcp_ - initial_sender_rtcp,
                    kNumAcceptedDowntimeRtcp)
              << "RTCP sent during sender-side downtime.";
          if (time_now_ms - initial_time_ms >=
              static_cast<int64_t>(kSilenceTimeoutMs)) {
            sender_done = true;
          }
        } else {
          if (sender_rtp_ > initial_sender_rtp)
            sender_done = true;
        }
        if (receiver_down) {
          ASSERT_LE(receiver_rtcp_ - initial_receiver_rtcp,
                    kNumAcceptedDowntimeRtcp)
              << "RTCP sent during receiver-side downtime.";
          if (time_now_ms - initial_time_ms >=
              static_cast<int64_t>(kSilenceTimeoutMs)) {
            receiver_done = true;
          }
        } else {
          if (receiver_rtcp_ > initial_receiver_rtcp)
            receiver_done = true;
        }
      }
    }

    const rtc::scoped_ptr<CriticalSectionWrapper> test_crit_;
    const rtc::scoped_ptr<EventWrapper> encoded_frames_;
    const rtc::scoped_ptr<EventWrapper> packet_event_;
    Call* sender_call_;
    Call* receiver_call_;
    Call::NetworkState sender_state_ GUARDED_BY(test_crit_);
    int sender_rtp_ GUARDED_BY(test_crit_);
    int sender_rtcp_ GUARDED_BY(test_crit_);
    int receiver_rtcp_ GUARDED_BY(test_crit_);
    int down_frames_ GUARDED_BY(test_crit_);
  } test;

  RunBaseTest(&test);
}

TEST_F(EndToEndTest, CallReportsRttForSender) {
  static const int kSendDelayMs = 30;
  static const int kReceiveDelayMs = 70;

  FakeNetworkPipe::Config config;
  config.queue_delay_ms = kSendDelayMs;
  test::DirectTransport sender_transport(config);
  config.queue_delay_ms = kReceiveDelayMs;
  test::DirectTransport receiver_transport(config);

  CreateCalls(Call::Config(&sender_transport),
              Call::Config(&receiver_transport));

  sender_transport.SetReceiver(receiver_call_->Receiver());
  receiver_transport.SetReceiver(sender_call_->Receiver());

  CreateSendConfig(1);
  CreateMatchingReceiveConfigs();

  CreateStreams();
  CreateFrameGeneratorCapturer();
  Start();

  int64_t start_time_ms = clock_->TimeInMilliseconds();
  while (true) {
    Call::Stats stats = sender_call_->GetStats();
    ASSERT_GE(start_time_ms + kDefaultTimeoutMs,
              clock_->TimeInMilliseconds())
        << "No RTT stats before timeout!";
    if (stats.rtt_ms != -1) {
      EXPECT_GE(stats.rtt_ms, kSendDelayMs + kReceiveDelayMs);
      break;
    }
    SleepMs(10);
  }

  Stop();
  DestroyStreams();
}

TEST_F(EndToEndTest, NewSendStreamsRespectNetworkDown) {
  class UnusedEncoder : public test::FakeEncoder {
    public:
     UnusedEncoder() : FakeEncoder(Clock::GetRealTimeClock()) {}
     int32_t Encode(const I420VideoFrame& input_image,
                    const CodecSpecificInfo* codec_specific_info,
                    const std::vector<VideoFrameType>* frame_types) override {
      ADD_FAILURE() << "Unexpected frame encode.";
      return test::FakeEncoder::Encode(
          input_image, codec_specific_info, frame_types);
    }
  };

  UnusedTransport transport;
  CreateSenderCall(Call::Config(&transport));
  sender_call_->SignalNetworkState(Call::kNetworkDown);

  CreateSendConfig(1);
  UnusedEncoder unused_encoder;
  send_config_.encoder_settings.encoder = &unused_encoder;
  CreateStreams();
  CreateFrameGeneratorCapturer();

  Start();
  SleepMs(kSilenceTimeoutMs);
  Stop();

  DestroyStreams();
}

TEST_F(EndToEndTest, NewReceiveStreamsRespectNetworkDown) {
  test::DirectTransport sender_transport;
  CreateSenderCall(Call::Config(&sender_transport));
  UnusedTransport transport;
  CreateReceiverCall(Call::Config(&transport));
  sender_transport.SetReceiver(receiver_call_->Receiver());

  receiver_call_->SignalNetworkState(Call::kNetworkDown);

  CreateSendConfig(1);
  CreateMatchingReceiveConfigs();
  CreateStreams();
  CreateFrameGeneratorCapturer();

  Start();
  SleepMs(kSilenceTimeoutMs);
  Stop();

  sender_transport.StopSending();

  DestroyStreams();
}

// TODO(pbos): Remove this regression test when VideoEngine is no longer used as
// a backend. This is to test that we hand channels back properly.
TEST_F(EndToEndTest, CanCreateAndDestroyManyVideoStreams) {
  test::NullTransport transport;
  rtc::scoped_ptr<Call> call(Call::Create(Call::Config(&transport)));
  test::FakeDecoder fake_decoder;
  test::FakeEncoder fake_encoder(Clock::GetRealTimeClock());
  for (size_t i = 0; i < 100; ++i) {
    VideoSendStream::Config send_config;
    send_config.encoder_settings.encoder = &fake_encoder;
    send_config.encoder_settings.payload_name = "FAKE";
    send_config.encoder_settings.payload_type = 123;

    VideoEncoderConfig encoder_config;
    encoder_config.streams = test::CreateVideoStreams(1);
    send_config.rtp.ssrcs.push_back(1);
    VideoSendStream* send_stream =
        call->CreateVideoSendStream(send_config, encoder_config);
    call->DestroyVideoSendStream(send_stream);

    VideoReceiveStream::Config receive_config;
    receive_config.rtp.remote_ssrc = 1;
    receive_config.rtp.local_ssrc = kReceiverLocalSsrc;
    VideoReceiveStream::Decoder decoder;
    decoder.decoder = &fake_decoder;
    decoder.payload_type = 123;
    decoder.payload_name = "FAKE";
    receive_config.decoders.push_back(decoder);
    VideoReceiveStream* receive_stream =
        call->CreateVideoReceiveStream(receive_config);
    call->DestroyVideoReceiveStream(receive_stream);
  }
}

}  // namespace webrtc
