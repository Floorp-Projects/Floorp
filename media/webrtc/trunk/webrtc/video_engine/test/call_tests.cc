/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <assert.h>

#include <map>

#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/modules/rtp_rtcp/interface/rtp_header_parser.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_utility.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/event_wrapper.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/video_engine/new_include/call.h"
#include "webrtc/video_engine/test/common/direct_transport.h"
#include "webrtc/video_engine/test/common/fake_decoder.h"
#include "webrtc/video_engine/test/common/fake_encoder.h"
#include "webrtc/video_engine/test/common/frame_generator_capturer.h"
#include "webrtc/video_engine/test/common/generate_ssrcs.h"
#include "webrtc/video_engine/test/common/rtp_rtcp_observer.h"

namespace webrtc {

struct CallTestParams {
  size_t width, height;
  struct {
    unsigned int min, start, max;
  } bitrate;
};

class CallTest : public ::testing::TestWithParam<CallTestParams> {
 public:
  CallTest()
      : send_stream_(NULL),
        receive_stream_(NULL),
        fake_encoder_(Clock::GetRealTimeClock()) {}

  ~CallTest() {
    EXPECT_EQ(NULL, send_stream_);
    EXPECT_EQ(NULL, receive_stream_);
  }

 protected:
  void CreateCalls(newapi::Transport* sender_transport,
                   newapi::Transport* receiver_transport) {
    Call::Config sender_config(sender_transport);
    Call::Config receiver_config(receiver_transport);
    sender_call_.reset(Call::Create(sender_config));
    receiver_call_.reset(Call::Create(receiver_config));
  }

  void CreateTestConfigs() {
    send_config_ = sender_call_->GetDefaultSendConfig();
    receive_config_ = receiver_call_->GetDefaultReceiveConfig();

    test::GenerateRandomSsrcs(&send_config_, &reserved_ssrcs_);
    send_config_.encoder = &fake_encoder_;
    send_config_.internal_source = false;
    test::FakeEncoder::SetCodecSettings(&send_config_.codec, 1);

    receive_config_.codecs.clear();
    receive_config_.codecs.push_back(send_config_.codec);
    ExternalVideoDecoder decoder;
    decoder.decoder = &fake_decoder_;
    decoder.payload_type = send_config_.codec.plType;
    receive_config_.external_decoders.push_back(decoder);
    receive_config_.rtp.ssrc = send_config_.rtp.ssrcs[0];
  }

  void CreateStreams() {
    assert(send_stream_ == NULL);
    assert(receive_stream_ == NULL);

    send_stream_ = sender_call_->CreateSendStream(send_config_);
    receive_stream_ = receiver_call_->CreateReceiveStream(receive_config_);
  }

  void CreateFrameGenerator() {
    frame_generator_capturer_.reset(
        test::FrameGeneratorCapturer::Create(send_stream_->Input(),
                                             send_config_.codec.width,
                                             send_config_.codec.height,
                                             30,
                                             Clock::GetRealTimeClock()));
  }

  void StartSending() {
    receive_stream_->StartReceive();
    send_stream_->StartSend();
    frame_generator_capturer_->Start();
  }

  void StopSending() {
    frame_generator_capturer_->Stop();
    if (send_stream_ != NULL)
      send_stream_->StopSend();
    if (receive_stream_ != NULL)
      receive_stream_->StopReceive();
  }

  void DestroyStreams() {
    if (send_stream_ != NULL)
      sender_call_->DestroySendStream(send_stream_);
    if (receive_stream_ != NULL)
      receiver_call_->DestroyReceiveStream(receive_stream_);
    send_stream_ = NULL;
    receive_stream_ = NULL;
  }

  void ReceivesPliAndRecovers(int rtp_history_ms);

  scoped_ptr<Call> sender_call_;
  scoped_ptr<Call> receiver_call_;

  VideoSendStream::Config send_config_;
  VideoReceiveStream::Config receive_config_;

  VideoSendStream* send_stream_;
  VideoReceiveStream* receive_stream_;

  scoped_ptr<test::FrameGeneratorCapturer> frame_generator_capturer_;

  test::FakeEncoder fake_encoder_;
  test::FakeDecoder fake_decoder_;

  std::map<uint32_t, bool> reserved_ssrcs_;
};

// TODO(pbos): What are sane values here for bitrate? Are we missing any
// important resolutions?
CallTestParams video_1080p = {1920, 1080, {300, 600, 800}};
CallTestParams video_720p = {1280, 720, {300, 600, 800}};
CallTestParams video_vga = {640, 480, {300, 600, 800}};
CallTestParams video_qvga = {320, 240, {300, 600, 800}};
CallTestParams video_4cif = {704, 576, {300, 600, 800}};
CallTestParams video_cif = {352, 288, {300, 600, 800}};
CallTestParams video_qcif = {176, 144, {300, 600, 800}};

class NackObserver : public test::RtpRtcpObserver {
  static const int kNumberOfNacksToObserve = 4;
  static const int kInverseProbabilityToStartLossBurst = 20;
  static const int kMaxLossBurst = 10;

 public:
  NackObserver()
      : test::RtpRtcpObserver(120 * 1000),
        rtp_parser_(RtpHeaderParser::Create()),
        drop_burst_count_(0),
        sent_rtp_packets_(0),
        nacks_left_(kNumberOfNacksToObserve) {}

 private:
  virtual Action OnSendRtp(const uint8_t* packet, size_t length) OVERRIDE {
    EXPECT_FALSE(RtpHeaderParser::IsRtcp(packet, static_cast<int>(length)));

    RTPHeader header;
    EXPECT_TRUE(rtp_parser_->Parse(packet, static_cast<int>(length), &header));

    // Never drop retransmitted packets.
    if (dropped_packets_.find(header.sequenceNumber) !=
        dropped_packets_.end()) {
      retransmitted_packets_.insert(header.sequenceNumber);
      return SEND_PACKET;
    }

    // Enough NACKs received, stop dropping packets.
    if (nacks_left_ == 0) {
      ++sent_rtp_packets_;
      return SEND_PACKET;
    }

    // Still dropping packets.
    if (drop_burst_count_ > 0) {
      --drop_burst_count_;
      dropped_packets_.insert(header.sequenceNumber);
      return DROP_PACKET;
    }

    // Should we start dropping packets?
    if (sent_rtp_packets_ > 0 &&
        rand() % kInverseProbabilityToStartLossBurst == 0) {
      drop_burst_count_ = rand() % kMaxLossBurst;
      dropped_packets_.insert(header.sequenceNumber);
      return DROP_PACKET;
    }

    ++sent_rtp_packets_;
    return SEND_PACKET;
  }

  virtual Action OnReceiveRtcp(const uint8_t* packet, size_t length) OVERRIDE {
    RTCPUtility::RTCPParserV2 parser(packet, length, true);
    EXPECT_TRUE(parser.IsValid());

    bool received_nack = false;
    RTCPUtility::RTCPPacketTypes packet_type = parser.Begin();
    while (packet_type != RTCPUtility::kRtcpNotValidCode) {
      if (packet_type == RTCPUtility::kRtcpRtpfbNackCode)
        received_nack = true;

      packet_type = parser.Iterate();
    }

    if (received_nack) {
      ReceivedNack();
    } else {
      RtcpWithoutNack();
    }
    return SEND_PACKET;
  }

 private:
  void ReceivedNack() {
    if (nacks_left_ > 0)
      --nacks_left_;
    rtcp_without_nack_count_ = 0;
  }

  void RtcpWithoutNack() {
    if (nacks_left_ > 0)
      return;
    ++rtcp_without_nack_count_;

    // All packets retransmitted and no recent NACKs.
    if (dropped_packets_.size() == retransmitted_packets_.size() &&
        rtcp_without_nack_count_ >= kRequiredRtcpsWithoutNack) {
      observation_complete_->Set();
    }
  }

  scoped_ptr<RtpHeaderParser> rtp_parser_;
  std::set<uint16_t> dropped_packets_;
  std::set<uint16_t> retransmitted_packets_;
  int drop_burst_count_;
  uint64_t sent_rtp_packets_;
  int nacks_left_;
  int rtcp_without_nack_count_;
  static const int kRequiredRtcpsWithoutNack = 2;
};

TEST_P(CallTest, ReceivesAndRetransmitsNack) {
  NackObserver observer;

  CreateCalls(observer.SendTransport(), observer.ReceiveTransport());

  observer.SetReceivers(receiver_call_->Receiver(), sender_call_->Receiver());

  CreateTestConfigs();
  int rtp_history_ms = 1000;
  send_config_.rtp.nack.rtp_history_ms = rtp_history_ms;
  receive_config_.rtp.nack.rtp_history_ms = rtp_history_ms;

  CreateStreams();
  CreateFrameGenerator();

  StartSending();

  // Wait() waits for an event triggered when NACKs have been received, NACKed
  // packets retransmitted and frames rendered again.
  EXPECT_EQ(kEventSignaled, observer.Wait());

  StopSending();

  observer.StopSending();

  DestroyStreams();
}

class PliObserver : public test::RtpRtcpObserver, public VideoRenderer {
  static const int kInverseDropProbability = 16;

 public:
  explicit PliObserver(bool nack_enabled)
      : test::RtpRtcpObserver(120 * 1000),
        rtp_header_parser_(RtpHeaderParser::Create()),
        nack_enabled_(nack_enabled),
        first_retransmitted_timestamp_(0),
        last_send_timestamp_(0),
        rendered_frame_(false),
        received_pli_(false) {}

  virtual Action OnSendRtp(const uint8_t* packet, size_t length) OVERRIDE {
    RTPHeader header;
    EXPECT_TRUE(
        rtp_header_parser_->Parse(packet, static_cast<int>(length), &header));

    // Drop all NACK retransmissions. This is to force transmission of a PLI.
    if (header.timestamp < last_send_timestamp_)
      return DROP_PACKET;

    if (received_pli_) {
      if (first_retransmitted_timestamp_ == 0) {
        first_retransmitted_timestamp_ = header.timestamp;
      }
    } else if (rendered_frame_ && rand() % kInverseDropProbability == 0) {
      return DROP_PACKET;
    }

    last_send_timestamp_ = header.timestamp;
    return SEND_PACKET;
  }

  virtual Action OnReceiveRtcp(const uint8_t* packet, size_t length) OVERRIDE {
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

  virtual void RenderFrame(const I420VideoFrame& video_frame,
                           int time_to_render_ms) OVERRIDE {
    CriticalSectionScoped crit_(lock_.get());
    if (first_retransmitted_timestamp_ != 0 &&
        video_frame.timestamp() > first_retransmitted_timestamp_) {
      EXPECT_TRUE(received_pli_);
      observation_complete_->Set();
    }
    rendered_frame_ = true;
  }

 private:
  scoped_ptr<RtpHeaderParser> rtp_header_parser_;
  bool nack_enabled_;

  uint32_t first_retransmitted_timestamp_;
  uint32_t last_send_timestamp_;

  bool rendered_frame_;
  bool received_pli_;
};

void CallTest::ReceivesPliAndRecovers(int rtp_history_ms) {
  PliObserver observer(rtp_history_ms > 0);

  CreateCalls(observer.SendTransport(), observer.ReceiveTransport());

  observer.SetReceivers(receiver_call_->Receiver(), sender_call_->Receiver());

  CreateTestConfigs();
  send_config_.rtp.nack.rtp_history_ms = rtp_history_ms;
  receive_config_.rtp.nack.rtp_history_ms = rtp_history_ms;
  receive_config_.renderer = &observer;

  CreateStreams();
  CreateFrameGenerator();

  StartSending();

  // Wait() waits for an event triggered when Pli has been received and frames
  // have been rendered afterwards.
  EXPECT_EQ(kEventSignaled, observer.Wait());

  StopSending();

  observer.StopSending();

  DestroyStreams();
}

TEST_P(CallTest, ReceivesPliAndRecoversWithNack) {
  ReceivesPliAndRecovers(1000);
}

// TODO(pbos): Enable this when 2250 is resolved.
TEST_P(CallTest, DISABLED_ReceivesPliAndRecoversWithoutNack) {
  ReceivesPliAndRecovers(0);
}

TEST_P(CallTest, SurvivesIncomingRtpPacketsToDestroyedReceiveStream) {
  class PacketInputObserver : public PacketReceiver {
   public:
    explicit PacketInputObserver(PacketReceiver* receiver)
        : receiver_(receiver), delivered_packet_(EventWrapper::Create()) {}

    EventTypeWrapper Wait() { return delivered_packet_->Wait(30 * 1000); }

   private:
    virtual bool DeliverPacket(const uint8_t* packet, size_t length) {
      if (RtpHeaderParser::IsRtcp(packet, static_cast<int>(length))) {
        return receiver_->DeliverPacket(packet, length);
      } else {
        EXPECT_FALSE(receiver_->DeliverPacket(packet, length));
        delivered_packet_->Set();
        return false;
      }
    }

    PacketReceiver* receiver_;
    scoped_ptr<EventWrapper> delivered_packet_;
  };

  test::DirectTransport send_transport, receive_transport;

  CreateCalls(&send_transport, &receive_transport);
  PacketInputObserver input_observer(receiver_call_->Receiver());

  send_transport.SetReceiver(&input_observer);
  receive_transport.SetReceiver(sender_call_->Receiver());

  CreateTestConfigs();

  CreateStreams();
  CreateFrameGenerator();

  StartSending();

  receiver_call_->DestroyReceiveStream(receive_stream_);
  receive_stream_ = NULL;

  // Wait() waits for a received packet.
  EXPECT_EQ(kEventSignaled, input_observer.Wait());

  StopSending();

  DestroyStreams();

  send_transport.StopSending();
  receive_transport.StopSending();
}

INSTANTIATE_TEST_CASE_P(CallTest, CallTest, ::testing::Values(video_vga));
}  // namespace webrtc
