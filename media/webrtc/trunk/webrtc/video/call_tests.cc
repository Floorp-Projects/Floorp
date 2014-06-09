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

#include <algorithm>
#include <map>
#include <sstream>
#include <string>

#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/call.h"
#include "webrtc/frame_callback.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_utility.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/event_wrapper.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/system_wrappers/interface/sleep.h"
#include "webrtc/test/direct_transport.h"
#include "webrtc/test/fake_audio_device.h"
#include "webrtc/test/fake_decoder.h"
#include "webrtc/test/fake_encoder.h"
#include "webrtc/test/frame_generator.h"
#include "webrtc/test/frame_generator_capturer.h"
#include "webrtc/test/null_transport.h"
#include "webrtc/test/rtp_rtcp_observer.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/test/testsupport/perf_test.h"
#include "webrtc/video/transport_adapter.h"

namespace webrtc {

static unsigned int kDefaultTimeoutMs = 30 * 1000;
static unsigned int kLongTimeoutMs = 120 * 1000;
static const uint32_t kSendSsrc = 0x654321;
static const uint32_t kSendRtxSsrc = 0x424242;
static const uint32_t kReceiverLocalSsrc = 0x123456;
static const uint8_t kSendPayloadType = 125;
static const uint8_t kSendRtxPayloadType = 126;

class CallTest : public ::testing::Test {
 public:
  CallTest()
      : send_stream_(NULL),
        receive_stream_(NULL),
        fake_encoder_(Clock::GetRealTimeClock()) {}

  virtual ~CallTest() {
    EXPECT_EQ(NULL, send_stream_);
    EXPECT_EQ(NULL, receive_stream_);
  }

 protected:
  void CreateCalls(const Call::Config& sender_config,
                   const Call::Config& receiver_config) {
    sender_call_.reset(Call::Create(sender_config));
    receiver_call_.reset(Call::Create(receiver_config));
  }

  void CreateTestConfigs() {
    send_config_ = sender_call_->GetDefaultSendConfig();
    receive_config_ = receiver_call_->GetDefaultReceiveConfig();

    send_config_.rtp.ssrcs.push_back(kSendSsrc);
    send_config_.encoder = &fake_encoder_;
    send_config_.internal_source = false;
    test::FakeEncoder::SetCodecSettings(&send_config_.codec, 1);
    send_config_.codec.plType = kSendPayloadType;

    receive_config_.codecs.clear();
    receive_config_.codecs.push_back(send_config_.codec);
    ExternalVideoDecoder decoder;
    decoder.decoder = &fake_decoder_;
    decoder.payload_type = send_config_.codec.plType;
    receive_config_.external_decoders.push_back(decoder);
    receive_config_.rtp.remote_ssrc = send_config_.rtp.ssrcs[0];
    receive_config_.rtp.local_ssrc = kReceiverLocalSsrc;
  }

  void CreateStreams() {
    assert(send_stream_ == NULL);
    assert(receive_stream_ == NULL);

    send_stream_ = sender_call_->CreateVideoSendStream(send_config_);
    receive_stream_ = receiver_call_->CreateVideoReceiveStream(receive_config_);
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
    receive_stream_->StartReceiving();
    send_stream_->StartSending();
    if (frame_generator_capturer_.get() != NULL)
      frame_generator_capturer_->Start();
  }

  void StopSending() {
    if (frame_generator_capturer_.get() != NULL)
      frame_generator_capturer_->Stop();
    if (send_stream_ != NULL)
      send_stream_->StopSending();
    if (receive_stream_ != NULL)
      receive_stream_->StopReceiving();
  }

  void DestroyStreams() {
    if (send_stream_ != NULL)
      sender_call_->DestroyVideoSendStream(send_stream_);
    if (receive_stream_ != NULL)
      receiver_call_->DestroyVideoReceiveStream(receive_stream_);
    send_stream_ = NULL;
    receive_stream_ = NULL;
  }

  void DecodesRetransmittedFrame(bool retransmit_over_rtx);
  void ReceivesPliAndRecovers(int rtp_history_ms);
  void RespectsRtcpMode(newapi::RtcpMode rtcp_mode);
  void TestXrReceiverReferenceTimeReport(bool enable_rrtr);

  scoped_ptr<Call> sender_call_;
  scoped_ptr<Call> receiver_call_;

  VideoSendStream::Config send_config_;
  VideoReceiveStream::Config receive_config_;

  VideoSendStream* send_stream_;
  VideoReceiveStream* receive_stream_;

  scoped_ptr<test::FrameGeneratorCapturer> frame_generator_capturer_;

  test::FakeEncoder fake_encoder_;
  test::FakeDecoder fake_decoder_;
};

class NackObserver : public test::RtpRtcpObserver {
  static const int kNumberOfNacksToObserve = 2;
  static const int kLossBurstSize = 2;
  static const int kPacketsBetweenLossBursts = 9;

 public:
  NackObserver()
      : test::RtpRtcpObserver(kLongTimeoutMs),
        rtp_parser_(RtpHeaderParser::Create()),
        sent_rtp_packets_(0),
        packets_left_to_drop_(0),
        nacks_left_(kNumberOfNacksToObserve) {}

 private:
  virtual Action OnSendRtp(const uint8_t* packet, size_t length) OVERRIDE {
    RTPHeader header;
    EXPECT_TRUE(rtp_parser_->Parse(packet, static_cast<int>(length), &header));

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

  virtual Action OnReceiveRtcp(const uint8_t* packet, size_t length) OVERRIDE {
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

 private:
  scoped_ptr<RtpHeaderParser> rtp_parser_;
  std::set<uint16_t> dropped_packets_;
  std::set<uint16_t> retransmitted_packets_;
  uint64_t sent_rtp_packets_;
  int packets_left_to_drop_;
  int nacks_left_;
};

TEST_F(CallTest, UsesTraceCallback) {
  const unsigned int kSenderTraceFilter = kTraceDebug;
  const unsigned int kReceiverTraceFilter = kTraceDefault & (~kTraceDebug);
  class TraceObserver : public TraceCallback {
   public:
    explicit TraceObserver(unsigned int filter)
        : filter_(filter), messages_left_(50), done_(EventWrapper::Create()) {}

    virtual void Print(TraceLevel level,
                       const char* message,
                       int length) OVERRIDE {
      EXPECT_EQ(0u, level & (~filter_));
      if (--messages_left_ == 0)
        done_->Set();
    }

    EventTypeWrapper Wait() { return done_->Wait(kDefaultTimeoutMs); }

   private:
    unsigned int filter_;
    unsigned int messages_left_;
    scoped_ptr<EventWrapper> done_;
  } sender_trace(kSenderTraceFilter), receiver_trace(kReceiverTraceFilter);

  test::DirectTransport send_transport, receive_transport;
  Call::Config sender_call_config(&send_transport);
  sender_call_config.trace_callback = &sender_trace;
  sender_call_config.trace_filter = kSenderTraceFilter;
  Call::Config receiver_call_config(&receive_transport);
  receiver_call_config.trace_callback = &receiver_trace;
  receiver_call_config.trace_filter = kReceiverTraceFilter;
  CreateCalls(sender_call_config, receiver_call_config);
  send_transport.SetReceiver(receiver_call_->Receiver());
  receive_transport.SetReceiver(sender_call_->Receiver());

  CreateTestConfigs();

  CreateStreams();
  CreateFrameGenerator();
  StartSending();

  // Wait() waits for a couple of trace callbacks to occur.
  EXPECT_EQ(kEventSignaled, sender_trace.Wait());
  EXPECT_EQ(kEventSignaled, receiver_trace.Wait());

  StopSending();
  send_transport.StopSending();
  receive_transport.StopSending();
  DestroyStreams();

  // The TraceCallback instance MUST outlive Calls, destroy Calls explicitly.
  sender_call_.reset();
  receiver_call_.reset();
}

TEST_F(CallTest, ReceiverCanBeStartedTwice) {
  test::NullTransport transport;
  CreateCalls(Call::Config(&transport), Call::Config(&transport));

  CreateTestConfigs();
  CreateStreams();

  receive_stream_->StartReceiving();
  receive_stream_->StartReceiving();

  DestroyStreams();
}

TEST_F(CallTest, ReceiverCanBeStoppedTwice) {
  test::NullTransport transport;
  CreateCalls(Call::Config(&transport), Call::Config(&transport));

  CreateTestConfigs();
  CreateStreams();

  receive_stream_->StopReceiving();
  receive_stream_->StopReceiving();

  DestroyStreams();
}

TEST_F(CallTest, RendersSingleDelayedFrame) {
  static const int kWidth = 320;
  static const int kHeight = 240;
  // This constant is chosen to be higher than the timeout in the video_render
  // module. This makes sure that frames aren't dropped if there are no other
  // frames in the queue.
  static const int kDelayRenderCallbackMs = 1000;

  class Renderer : public VideoRenderer {
   public:
    Renderer() : event_(EventWrapper::Create()) {}

    virtual void RenderFrame(const I420VideoFrame& video_frame,
                             int /*time_to_render_ms*/) OVERRIDE {
      event_->Set();
    }

    EventTypeWrapper Wait() { return event_->Wait(kDefaultTimeoutMs); }

    scoped_ptr<EventWrapper> event_;
  } renderer;

  class TestFrameCallback : public I420FrameCallback {
   public:
    TestFrameCallback() : event_(EventWrapper::Create()) {}

    EventTypeWrapper Wait() { return event_->Wait(kDefaultTimeoutMs); }

   private:
    virtual void FrameCallback(I420VideoFrame* frame) OVERRIDE {
      SleepMs(kDelayRenderCallbackMs);
      event_->Set();
    }

    scoped_ptr<EventWrapper> event_;
  };

  test::DirectTransport sender_transport, receiver_transport;

  CreateCalls(Call::Config(&sender_transport),
              Call::Config(&receiver_transport));

  sender_transport.SetReceiver(receiver_call_->Receiver());
  receiver_transport.SetReceiver(sender_call_->Receiver());

  CreateTestConfigs();

  TestFrameCallback pre_render_callback;
  receive_config_.pre_render_callback = &pre_render_callback;
  receive_config_.renderer = &renderer;

  CreateStreams();
  StartSending();

  // Create frames that are smaller than the send width/height, this is done to
  // check that the callbacks are done after processing video.
  scoped_ptr<test::FrameGenerator> frame_generator(
      test::FrameGenerator::Create(kWidth, kHeight));
  send_stream_->Input()->SwapFrame(frame_generator->NextFrame());
  EXPECT_EQ(kEventSignaled, pre_render_callback.Wait())
      << "Timed out while waiting for pre-render callback.";
  EXPECT_EQ(kEventSignaled, renderer.Wait())
      << "Timed out while waiting for the frame to render.";

  StopSending();

  sender_transport.StopSending();
  receiver_transport.StopSending();

  DestroyStreams();
}

TEST_F(CallTest, TransmitsFirstFrame) {
  class Renderer : public VideoRenderer {
   public:
    Renderer() : event_(EventWrapper::Create()) {}

    virtual void RenderFrame(const I420VideoFrame& video_frame,
                             int /*time_to_render_ms*/) OVERRIDE {
      event_->Set();
    }

    EventTypeWrapper Wait() { return event_->Wait(kDefaultTimeoutMs); }

    scoped_ptr<EventWrapper> event_;
  } renderer;

  test::DirectTransport sender_transport, receiver_transport;

  CreateCalls(Call::Config(&sender_transport),
              Call::Config(&receiver_transport));

  sender_transport.SetReceiver(receiver_call_->Receiver());
  receiver_transport.SetReceiver(sender_call_->Receiver());

  CreateTestConfigs();
  receive_config_.renderer = &renderer;

  CreateStreams();
  StartSending();

  scoped_ptr<test::FrameGenerator> frame_generator(test::FrameGenerator::Create(
      send_config_.codec.width, send_config_.codec.height));
  send_stream_->Input()->SwapFrame(frame_generator->NextFrame());

  EXPECT_EQ(kEventSignaled, renderer.Wait())
      << "Timed out while waiting for the frame to render.";

  StopSending();

  sender_transport.StopSending();
  receiver_transport.StopSending();

  DestroyStreams();
}

TEST_F(CallTest, ReceiverUsesLocalSsrc) {
  class SyncRtcpObserver : public test::RtpRtcpObserver {
   public:
    SyncRtcpObserver() : test::RtpRtcpObserver(kDefaultTimeoutMs) {}

    virtual Action OnReceiveRtcp(const uint8_t* packet,
                                 size_t length) OVERRIDE {
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
  } observer;

  CreateCalls(Call::Config(observer.SendTransport()),
              Call::Config(observer.ReceiveTransport()));

  observer.SetReceivers(receiver_call_->Receiver(), sender_call_->Receiver());

  CreateTestConfigs();

  CreateStreams();
  CreateFrameGenerator();
  StartSending();

  EXPECT_EQ(kEventSignaled, observer.Wait())
      << "Timed out while waiting for a receiver RTCP packet to be sent.";

  StopSending();

  observer.StopSending();

  DestroyStreams();
}

TEST_F(CallTest, ReceivesAndRetransmitsNack) {
  NackObserver observer;

  CreateCalls(Call::Config(observer.SendTransport()),
              Call::Config(observer.ReceiveTransport()));

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

// This test drops second RTP packet with a marker bit set, makes sure it's
// retransmitted and renders. Retransmission SSRCs are also checked.
void CallTest::DecodesRetransmittedFrame(bool retransmit_over_rtx) {
  static const int kDroppedFrameNumber = 2;
  class RetransmissionObserver : public test::RtpRtcpObserver,
                                 public I420FrameCallback {
   public:
    RetransmissionObserver(bool expect_rtx)
        : RtpRtcpObserver(kDefaultTimeoutMs),
          retransmission_ssrc_(expect_rtx ? kSendRtxSsrc : kSendSsrc),
          retransmission_payload_type_(expect_rtx ? kSendRtxPayloadType
                                                  : kSendPayloadType),
          marker_bits_observed_(0),
          retransmitted_timestamp_(0),
          frame_retransmitted_(false) {}

   private:
    virtual Action OnSendRtp(const uint8_t* packet, size_t length) OVERRIDE {
      RTPHeader header;
      EXPECT_TRUE(parser_->Parse(packet, static_cast<int>(length), &header));

      if (header.timestamp == retransmitted_timestamp_) {
        EXPECT_EQ(retransmission_ssrc_, header.ssrc);
        EXPECT_EQ(retransmission_payload_type_, header.payloadType);
        frame_retransmitted_ = true;
        return SEND_PACKET;
      }

      EXPECT_EQ(kSendSsrc, header.ssrc);
      EXPECT_EQ(kSendPayloadType, header.payloadType);

      // Found the second frame's final packet, drop this and expect a
      // retransmission.
      if (header.markerBit && ++marker_bits_observed_ == kDroppedFrameNumber) {
        retransmitted_timestamp_ = header.timestamp;
        return DROP_PACKET;
      }

      return SEND_PACKET;
    }

    virtual void FrameCallback(I420VideoFrame* frame) OVERRIDE {
      CriticalSectionScoped crit_(lock_.get());
      if (frame->timestamp() == retransmitted_timestamp_) {
        EXPECT_TRUE(frame_retransmitted_);
        observation_complete_->Set();
      }
    }

    const uint32_t retransmission_ssrc_;
    const int retransmission_payload_type_;
    int marker_bits_observed_;
    uint32_t retransmitted_timestamp_;
    bool frame_retransmitted_;
  } observer(retransmit_over_rtx);

  CreateCalls(Call::Config(observer.SendTransport()),
              Call::Config(observer.ReceiveTransport()));

  observer.SetReceivers(receiver_call_->Receiver(), sender_call_->Receiver());

  CreateTestConfigs();
  send_config_.rtp.nack.rtp_history_ms =
      receive_config_.rtp.nack.rtp_history_ms = 1000;
  if (retransmit_over_rtx) {
    send_config_.rtp.rtx.ssrcs.push_back(kSendRtxSsrc);
    send_config_.rtp.rtx.payload_type = kSendRtxPayloadType;
    int payload_type = send_config_.codec.plType;
    receive_config_.rtp.rtx[payload_type].ssrc = kSendRtxSsrc;
    receive_config_.rtp.rtx[payload_type].payload_type = kSendRtxPayloadType;
  }
  receive_config_.pre_render_callback = &observer;

  CreateStreams();
  CreateFrameGenerator();
  StartSending();

  EXPECT_EQ(kEventSignaled, observer.Wait())
      << "Timed out while waiting for retransmission to render.";

  StopSending();
  observer.StopSending();
  DestroyStreams();
}

TEST_F(CallTest, DecodesRetransmittedFrame) {
  DecodesRetransmittedFrame(false);
}

TEST_F(CallTest, DecodesRetransmittedFrameOverRtx) {
  DecodesRetransmittedFrame(true);
}

TEST_F(CallTest, UsesFrameCallbacks) {
  static const int kWidth = 320;
  static const int kHeight = 240;

  class Renderer : public VideoRenderer {
   public:
    Renderer() : event_(EventWrapper::Create()) {}

    virtual void RenderFrame(const I420VideoFrame& video_frame,
                             int /*time_to_render_ms*/) OVERRIDE {
      EXPECT_EQ(0, *video_frame.buffer(kYPlane))
          << "Rendered frame should have zero luma which is applied by the "
             "pre-render callback.";
      event_->Set();
    }

    EventTypeWrapper Wait() { return event_->Wait(kDefaultTimeoutMs); }
    scoped_ptr<EventWrapper> event_;
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

    scoped_ptr<EventWrapper> event_;
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

  CreateTestConfigs();
  send_config_.encoder = NULL;
  send_config_.codec = sender_call_->GetVideoCodecs()[0];
  send_config_.codec.width = kWidth;
  send_config_.codec.height = kHeight;
  send_config_.pre_encode_callback = &pre_encode_callback;
  receive_config_.pre_render_callback = &pre_render_callback;
  receive_config_.renderer = &renderer;

  CreateStreams();
  StartSending();

  // Create frames that are smaller than the send width/height, this is done to
  // check that the callbacks are done after processing video.
  scoped_ptr<test::FrameGenerator> frame_generator(
      test::FrameGenerator::Create(kWidth / 2, kHeight / 2));
  send_stream_->Input()->SwapFrame(frame_generator->NextFrame());

  EXPECT_EQ(kEventSignaled, pre_encode_callback.Wait())
      << "Timed out while waiting for pre-encode callback.";
  EXPECT_EQ(kEventSignaled, pre_render_callback.Wait())
      << "Timed out while waiting for pre-render callback.";
  EXPECT_EQ(kEventSignaled, renderer.Wait())
      << "Timed out while waiting for the frame to render.";

  StopSending();

  sender_transport.StopSending();
  receiver_transport.StopSending();

  DestroyStreams();
}

class PliObserver : public test::RtpRtcpObserver, public VideoRenderer {
  static const int kInverseDropProbability = 16;

 public:
  explicit PliObserver(bool nack_enabled)
      : test::RtpRtcpObserver(kLongTimeoutMs),
        nack_enabled_(nack_enabled),
        highest_dropped_timestamp_(0),
        frames_to_drop_(0),
        received_pli_(false) {}

  virtual Action OnSendRtp(const uint8_t* packet, size_t length) OVERRIDE {
    RTPHeader header;
    EXPECT_TRUE(parser_->Parse(packet, static_cast<int>(length), &header));

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
    if (received_pli_ && video_frame.timestamp() > highest_dropped_timestamp_) {
      observation_complete_->Set();
    }
    if (!received_pli_)
      frames_to_drop_ = kPacketsToDrop;
  }

 private:
  static const int kPacketsToDrop = 1;

  bool nack_enabled_;
  uint32_t highest_dropped_timestamp_;
  int frames_to_drop_;
  bool received_pli_;
};

void CallTest::ReceivesPliAndRecovers(int rtp_history_ms) {
  PliObserver observer(rtp_history_ms > 0);

  CreateCalls(Call::Config(observer.SendTransport()),
              Call::Config(observer.ReceiveTransport()));

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

TEST_F(CallTest, ReceivesPliAndRecoversWithNack) {
  ReceivesPliAndRecovers(1000);
}

// TODO(pbos): Enable this when 2250 is resolved.
TEST_F(CallTest, DISABLED_ReceivesPliAndRecoversWithoutNack) {
  ReceivesPliAndRecovers(0);
}

TEST_F(CallTest, SurvivesIncomingRtpPacketsToDestroyedReceiveStream) {
  class PacketInputObserver : public PacketReceiver {
   public:
    explicit PacketInputObserver(PacketReceiver* receiver)
        : receiver_(receiver), delivered_packet_(EventWrapper::Create()) {}

    EventTypeWrapper Wait() {
      return delivered_packet_->Wait(kDefaultTimeoutMs);
    }

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

  CreateCalls(Call::Config(&send_transport), Call::Config(&receive_transport));
  PacketInputObserver input_observer(receiver_call_->Receiver());

  send_transport.SetReceiver(&input_observer);
  receive_transport.SetReceiver(sender_call_->Receiver());

  CreateTestConfigs();

  CreateStreams();
  CreateFrameGenerator();
  StartSending();

  receiver_call_->DestroyVideoReceiveStream(receive_stream_);
  receive_stream_ = NULL;

  // Wait() waits for a received packet.
  EXPECT_EQ(kEventSignaled, input_observer.Wait());

  StopSending();

  DestroyStreams();

  send_transport.StopSending();
  receive_transport.StopSending();
}

void CallTest::RespectsRtcpMode(newapi::RtcpMode rtcp_mode) {
  static const int kRtpHistoryMs = 1000;
  static const int kNumCompoundRtcpPacketsToObserve = 10;
  class RtcpModeObserver : public test::RtpRtcpObserver {
   public:
    explicit RtcpModeObserver(newapi::RtcpMode rtcp_mode)
        : test::RtpRtcpObserver(kDefaultTimeoutMs),
          rtcp_mode_(rtcp_mode),
          sent_rtp_(0),
          sent_rtcp_(0) {}

   private:
    virtual Action OnSendRtp(const uint8_t* packet, size_t length) OVERRIDE {
      if (++sent_rtp_ % 3 == 0)
        return DROP_PACKET;

      return SEND_PACKET;
    }

    virtual Action OnReceiveRtcp(const uint8_t* packet,
                                 size_t length) OVERRIDE {
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

    newapi::RtcpMode rtcp_mode_;
    int sent_rtp_;
    int sent_rtcp_;
  } observer(rtcp_mode);

  CreateCalls(Call::Config(observer.SendTransport()),
              Call::Config(observer.ReceiveTransport()));

  observer.SetReceivers(receiver_call_->Receiver(), sender_call_->Receiver());

  CreateTestConfigs();
  send_config_.rtp.nack.rtp_history_ms = kRtpHistoryMs;
  receive_config_.rtp.nack.rtp_history_ms = kRtpHistoryMs;
  receive_config_.rtp.rtcp_mode = rtcp_mode;

  CreateStreams();
  CreateFrameGenerator();
  StartSending();

  EXPECT_EQ(kEventSignaled, observer.Wait())
      << (rtcp_mode == newapi::kRtcpCompound
              ? "Timed out before observing enough compound packets."
              : "Timed out before receiving a non-compound RTCP packet.");

  StopSending();
  observer.StopSending();
  DestroyStreams();
}

TEST_F(CallTest, UsesRtcpCompoundMode) {
  RespectsRtcpMode(newapi::kRtcpCompound);
}

TEST_F(CallTest, UsesRtcpReducedSizeMode) {
  RespectsRtcpMode(newapi::kRtcpReducedSize);
}

// Test sets up a Call multiple senders with different resolutions and SSRCs.
// Another is set up to receive all three of these with different renderers.
// Each renderer verifies that it receives the expected resolution, and as soon
// as every renderer has received a frame, the test finishes.
TEST_F(CallTest, SendsAndReceivesMultipleStreams) {
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

    virtual void RenderFrame(const I420VideoFrame& video_frame,
                             int time_to_render_ms) OVERRIDE {
      EXPECT_EQ(width_, video_frame.width());
      EXPECT_EQ(height_, video_frame.height());
      (*capturer_)->Stop();
      done_->Set();
    }

    void Wait() { done_->Wait(kDefaultTimeoutMs); }

   private:
    test::FrameGeneratorCapturer** capturer_;
    int width_;
    int height_;
    scoped_ptr<EventWrapper> done_;
  };

  struct {
    uint32_t ssrc;
    int width;
    int height;
  } codec_settings[kNumStreams] = {{1, 640, 480}, {2, 320, 240}, {3, 240, 160}};

  test::DirectTransport sender_transport, receiver_transport;
  scoped_ptr<Call> sender_call(Call::Create(Call::Config(&sender_transport)));
  scoped_ptr<Call> receiver_call(
      Call::Create(Call::Config(&receiver_transport)));
  sender_transport.SetReceiver(receiver_call->Receiver());
  receiver_transport.SetReceiver(sender_call->Receiver());

  VideoSendStream* send_streams[kNumStreams];
  VideoReceiveStream* receive_streams[kNumStreams];

  VideoOutputObserver* observers[kNumStreams];
  test::FrameGeneratorCapturer* frame_generators[kNumStreams];

  for (size_t i = 0; i < kNumStreams; ++i) {
    uint32_t ssrc = codec_settings[i].ssrc;
    int width = codec_settings[i].width;
    int height = codec_settings[i].height;
    observers[i] = new VideoOutputObserver(&frame_generators[i], width, height);

    VideoReceiveStream::Config receive_config =
        receiver_call->GetDefaultReceiveConfig();
    receive_config.renderer = observers[i];
    receive_config.rtp.remote_ssrc = ssrc;
    receive_config.rtp.local_ssrc = kReceiverLocalSsrc;
    receive_streams[i] =
        receiver_call->CreateVideoReceiveStream(receive_config);
    receive_streams[i]->StartReceiving();

    VideoSendStream::Config send_config = sender_call->GetDefaultSendConfig();
    send_config.rtp.ssrcs.push_back(ssrc);
    send_config.codec.width = width;
    send_config.codec.height = height;
    send_streams[i] = sender_call->CreateVideoSendStream(send_config);
    send_streams[i]->StartSending();

    frame_generators[i] = test::FrameGeneratorCapturer::Create(
        send_streams[i]->Input(), width, height, 30, Clock::GetRealTimeClock());
    frame_generators[i]->Start();
  }

  for (size_t i = 0; i < kNumStreams; ++i) {
    observers[i]->Wait();
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
};

TEST_F(CallTest, ObserversEncodedFrames) {
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
    scoped_ptr<uint8_t[]> buffer_;
    size_t length_;
    FrameType frame_type_;
    scoped_ptr<EventWrapper> called_;
  };

  EncodedFrameTestObserver post_encode_observer;
  EncodedFrameTestObserver pre_decode_observer;

  test::DirectTransport sender_transport, receiver_transport;

  CreateCalls(Call::Config(&sender_transport),
              Call::Config(&receiver_transport));

  sender_transport.SetReceiver(receiver_call_->Receiver());
  receiver_transport.SetReceiver(sender_call_->Receiver());

  CreateTestConfigs();
  send_config_.post_encode_callback = &post_encode_observer;
  receive_config_.pre_decode_callback = &pre_decode_observer;

  CreateStreams();
  StartSending();

  scoped_ptr<test::FrameGenerator> frame_generator(test::FrameGenerator::Create(
      send_config_.codec.width, send_config_.codec.height));
  send_stream_->Input()->SwapFrame(frame_generator->NextFrame());

  EXPECT_EQ(kEventSignaled, post_encode_observer.Wait())
      << "Timed out while waiting for send-side encoded-frame callback.";

  EXPECT_EQ(kEventSignaled, pre_decode_observer.Wait())
      << "Timed out while waiting for pre-decode encoded-frame callback.";

  post_encode_observer.ExpectEqualFrames(pre_decode_observer);

  StopSending();

  sender_transport.StopSending();
  receiver_transport.StopSending();

  DestroyStreams();
}

TEST_F(CallTest, ReceiveStreamSendsRemb) {
  class RembObserver : public test::RtpRtcpObserver {
   public:
    RembObserver() : test::RtpRtcpObserver(kDefaultTimeoutMs) {}

    virtual Action OnReceiveRtcp(const uint8_t* packet,
                                 size_t length) OVERRIDE {
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
          EXPECT_EQ(packet.REMBItem.SSRCs[0], kSendSsrc);
          received_remb = true;
        }
        packet_type = parser.Iterate();
      }
      if (received_psfb && received_remb)
        observation_complete_->Set();
      return SEND_PACKET;
    }
  } observer;

  CreateCalls(Call::Config(observer.SendTransport()),
              Call::Config(observer.ReceiveTransport()));
  observer.SetReceivers(receiver_call_->Receiver(), sender_call_->Receiver());
  CreateTestConfigs();
  CreateStreams();
  CreateFrameGenerator();
  StartSending();

  EXPECT_EQ(kEventSignaled, observer.Wait())
      << "Timed out while waiting for a receiver RTCP REMB packet to be sent.";

  StopSending();
  observer.StopSending();
  DestroyStreams();
}

void CallTest::TestXrReceiverReferenceTimeReport(bool enable_rrtr) {
  static const int kNumRtcpReportPacketsToObserve = 5;
  class RtcpXrObserver : public test::RtpRtcpObserver {
   public:
    explicit RtcpXrObserver(bool enable_rrtr)
        : test::RtpRtcpObserver(kDefaultTimeoutMs),
          enable_rrtr_(enable_rrtr),
          sent_rtcp_sr_(0),
          sent_rtcp_rr_(0),
          sent_rtcp_rrtr_(0),
          sent_rtcp_dlrr_(0) {}

   private:
    // Receive stream should send RR packets (and RRTR packets if enabled).
    virtual Action OnReceiveRtcp(const uint8_t* packet,
                                 size_t length) OVERRIDE {
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
    bool enable_rrtr_;
    int sent_rtcp_sr_;
    int sent_rtcp_rr_;
    int sent_rtcp_rrtr_;
    int sent_rtcp_dlrr_;
  } observer(enable_rrtr);

  CreateCalls(Call::Config(observer.SendTransport()),
              Call::Config(observer.ReceiveTransport()));
  observer.SetReceivers(receiver_call_->Receiver(), sender_call_->Receiver());

  CreateTestConfigs();
  receive_config_.rtp.rtcp_mode = newapi::kRtcpReducedSize;
  receive_config_.rtp.rtcp_xr.receiver_reference_time_report = enable_rrtr;

  CreateStreams();
  CreateFrameGenerator();
  StartSending();

  EXPECT_EQ(kEventSignaled, observer.Wait())
      << "Timed out while waiting for RTCP SR/RR packets to be sent.";

  StopSending();
  observer.StopSending();
  DestroyStreams();
}

class StatsObserver : public test::RtpRtcpObserver, public I420FrameCallback {
 public:
  StatsObserver()
      : test::RtpRtcpObserver(kLongTimeoutMs),
        receive_stream_(NULL),
        send_stream_(NULL),
        expected_receive_ssrc_(),
        expected_send_ssrcs_(),
        check_stats_event_(EventWrapper::Create()) {}

  void SetExpectedReceiveSsrc(uint32_t ssrc) { expected_receive_ssrc_ = ssrc; }

  void SetExpectedSendSsrcs(const std::vector<uint32_t>& ssrcs) {
    for (std::vector<uint32_t>::const_iterator it = ssrcs.begin();
         it != ssrcs.end();
         ++it) {
      expected_send_ssrcs_.insert(*it);
    }
  }

  void SetExpectedCName(std::string cname) { expected_cname_ = cname; }

  void SetReceiveStream(VideoReceiveStream* stream) {
    receive_stream_ = stream;
  }

  void SetSendStream(VideoSendStream* stream) { send_stream_ = stream; }

  void WaitForFilledStats() {
    Clock* clock = Clock::GetRealTimeClock();
    int64_t now = clock->TimeInMilliseconds();
    int64_t stop_time = now + kLongTimeoutMs;
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

 private:
  virtual Action OnSendRtp(const uint8_t* packet, size_t length) OVERRIDE {
    check_stats_event_->Set();
    return SEND_PACKET;
  }

  virtual Action OnSendRtcp(const uint8_t* packet, size_t length) OVERRIDE {
    check_stats_event_->Set();
    return SEND_PACKET;
  }

  virtual Action OnReceiveRtp(const uint8_t* packet, size_t length) OVERRIDE {
    check_stats_event_->Set();
    return SEND_PACKET;
  }

  virtual Action OnReceiveRtcp(const uint8_t* packet, size_t length) OVERRIDE {
    check_stats_event_->Set();
    return SEND_PACKET;
  }

  virtual void FrameCallback(I420VideoFrame* video_frame) OVERRIDE {
    // Ensure that we have at least 5ms send side delay.
    int64_t render_time = video_frame->render_time_ms();
    if (render_time > 0)
      video_frame->set_render_time_ms(render_time - 5);
  }

  bool CheckReceiveStats() {
    assert(receive_stream_ != NULL);
    VideoReceiveStream::Stats stats = receive_stream_->GetStats();
    EXPECT_EQ(expected_receive_ssrc_, stats.ssrc);

    // Make sure all fields have been populated.

    receive_stats_filled_["IncomingRate"] |=
        stats.network_frame_rate != 0 || stats.bitrate_bps != 0;

    receive_stats_filled_["FrameCallback"] |= stats.decode_frame_rate != 0;

    receive_stats_filled_["FrameRendered"] |= stats.render_frame_rate != 0;

    receive_stats_filled_["StatisticsUpdated"] |=
        stats.rtcp_stats.cumulative_lost != 0 ||
        stats.rtcp_stats.extended_max_sequence_number != 0 ||
        stats.rtcp_stats.fraction_lost != 0 || stats.rtcp_stats.jitter != 0;

    receive_stats_filled_["DataCountersUpdated"] |=
        stats.rtp_stats.bytes != 0 || stats.rtp_stats.fec_packets != 0 ||
        stats.rtp_stats.header_bytes != 0 || stats.rtp_stats.packets != 0 ||
        stats.rtp_stats.padding_bytes != 0 ||
        stats.rtp_stats.retransmitted_packets != 0;

    receive_stats_filled_["CodecStats"] |=
        stats.avg_delay_ms != 0 || stats.discarded_packets != 0 ||
        stats.key_frames != 0 || stats.delta_frames != 0;

    receive_stats_filled_["CName"] |= stats.c_name == expected_cname_;

    return AllStatsFilled(receive_stats_filled_);
  }

  bool CheckSendStats() {
    assert(send_stream_ != NULL);
    VideoSendStream::Stats stats = send_stream_->GetStats();

    send_stats_filled_["NumStreams"] |=
        stats.substreams.size() == expected_send_ssrcs_.size();

    send_stats_filled_["Delay"] |=
        stats.avg_delay_ms != 0 || stats.max_delay_ms != 0;

    receive_stats_filled_["CName"] |= stats.c_name == expected_cname_;

    for (std::map<uint32_t, StreamStats>::const_iterator it =
             stats.substreams.begin();
         it != stats.substreams.end();
         ++it) {
      EXPECT_TRUE(expected_send_ssrcs_.find(it->first) !=
                  expected_send_ssrcs_.end());

      send_stats_filled_[CompoundKey("IncomingRate", it->first)] |=
          stats.input_frame_rate != 0;

      const StreamStats& stream_stats = it->second;

      send_stats_filled_[CompoundKey("StatisticsUpdated", it->first)] |=
          stream_stats.rtcp_stats.cumulative_lost != 0 ||
          stream_stats.rtcp_stats.extended_max_sequence_number != 0 ||
          stream_stats.rtcp_stats.fraction_lost != 0;

      send_stats_filled_[CompoundKey("DataCountersUpdated", it->first)] |=
          stream_stats.rtp_stats.fec_packets != 0 ||
          stream_stats.rtp_stats.padding_bytes != 0 ||
          stream_stats.rtp_stats.retransmitted_packets != 0 ||
          stream_stats.rtp_stats.packets != 0;

      send_stats_filled_[CompoundKey("BitrateStatisticsObserver", it->first)] |=
          stream_stats.bitrate_bps != 0;

      send_stats_filled_[CompoundKey("FrameCountObserver", it->first)] |=
          stream_stats.delta_frames != 0 || stream_stats.key_frames != 0;

      send_stats_filled_[CompoundKey("OutgoingRate", it->first)] |=
          stats.encode_frame_rate != 0;
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

  VideoReceiveStream* receive_stream_;
  std::map<std::string, bool> receive_stats_filled_;

  VideoSendStream* send_stream_;
  std::map<std::string, bool> send_stats_filled_;

  uint32_t expected_receive_ssrc_;
  std::set<uint32_t> expected_send_ssrcs_;
  std::string expected_cname_;

  scoped_ptr<EventWrapper> check_stats_event_;
};

TEST_F(CallTest, GetStats) {
  StatsObserver observer;

  CreateCalls(Call::Config(observer.SendTransport()),
              Call::Config(observer.ReceiveTransport()));

  observer.SetReceivers(receiver_call_->Receiver(), sender_call_->Receiver());

  CreateTestConfigs();
  send_config_.pre_encode_callback = &observer;  // Used to inject delay.
  send_config_.rtp.c_name = "SomeCName";

  observer.SetExpectedReceiveSsrc(receive_config_.rtp.local_ssrc);
  observer.SetExpectedSendSsrcs(send_config_.rtp.ssrcs);
  observer.SetExpectedCName(send_config_.rtp.c_name);

  CreateStreams();
  observer.SetReceiveStream(receive_stream_);
  observer.SetSendStream(send_stream_);
  CreateFrameGenerator();
  StartSending();

  observer.WaitForFilledStats();

  StopSending();
  observer.StopSending();
  DestroyStreams();
}

TEST_F(CallTest, ReceiverReferenceTimeReportEnabled) {
  TestXrReceiverReferenceTimeReport(true);
}

TEST_F(CallTest, ReceiverReferenceTimeReportDisabled) {
  TestXrReceiverReferenceTimeReport(false);
}
}  // namespace webrtc
