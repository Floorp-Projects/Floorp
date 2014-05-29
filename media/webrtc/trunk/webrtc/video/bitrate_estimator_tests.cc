/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <functional>
#include <list>
#include <string>

#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/call.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/event_wrapper.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/test/direct_transport.h"
#include "webrtc/test/fake_decoder.h"
#include "webrtc/test/fake_encoder.h"
#include "webrtc/test/frame_generator_capturer.h"

namespace webrtc {

static const int kTOFExtensionId = 4;
static const int kASTExtensionId = 5;

static unsigned int kDefaultTimeoutMs = 30 * 1000;
static const uint32_t kSendSsrc = 0x654321;
static const uint32_t kReceiverLocalSsrc = 0x123456;
static const uint8_t kSendPayloadType = 125;

class BitrateEstimatorTest : public ::testing::Test {
 public:
  BitrateEstimatorTest()
      : receiver_trace_(),
        send_transport_(),
        receive_transport_(),
        sender_call_(),
        receiver_call_(),
        send_config_(),
        receive_config_(),
        streams_() {
  }

  virtual ~BitrateEstimatorTest() {
    EXPECT_TRUE(streams_.empty());
  }

  virtual void SetUp() {
    // Create receiver call first so that we are guaranteed to have a trace
    // callback when sender call is created.
    Call::Config receiver_call_config(&receive_transport_);
    receiver_call_config.trace_callback = &receiver_trace_;
    receiver_call_.reset(Call::Create(receiver_call_config));

    Call::Config sender_call_config(&send_transport_);
    sender_call_.reset(Call::Create(sender_call_config));

    send_transport_.SetReceiver(receiver_call_->Receiver());
    receive_transport_.SetReceiver(sender_call_->Receiver());

    send_config_ = sender_call_->GetDefaultSendConfig();
    send_config_.rtp.ssrcs.push_back(kSendSsrc);
    // send_config_.encoder will be set by every stream separately.
    send_config_.internal_source = false;
    test::FakeEncoder::SetCodecSettings(&send_config_.codec, 1);
    send_config_.codec.plType = kSendPayloadType;

    receive_config_ = receiver_call_->GetDefaultReceiveConfig();
    receive_config_.codecs.clear();
    receive_config_.codecs.push_back(send_config_.codec);
    // receive_config_.external_decoders will be set by every stream separately.
    receive_config_.rtp.remote_ssrc = send_config_.rtp.ssrcs[0];
    receive_config_.rtp.local_ssrc = kReceiverLocalSsrc;
    receive_config_.rtp.extensions.push_back(
        RtpExtension(RtpExtension::kTOffset, kTOFExtensionId));
    receive_config_.rtp.extensions.push_back(
        RtpExtension(RtpExtension::kAbsSendTime, kASTExtensionId));
  }

  virtual void TearDown() {
    std::for_each(streams_.begin(), streams_.end(),
        std::mem_fun(&Stream::StopSending));

    send_transport_.StopSending();
    receive_transport_.StopSending();

    while (!streams_.empty()) {
      delete streams_.back();
      streams_.pop_back();
    }

    // The TraceCallback instance MUST outlive Calls, destroy Calls explicitly.
    receiver_call_.reset();
  }

 protected:
  friend class Stream;

  class TraceObserver : public TraceCallback {
   public:
    TraceObserver()
        : crit_sect_(CriticalSectionWrapper::CreateCriticalSection()),
          received_log_lines_(),
          expected_log_lines_(),
          done_(EventWrapper::Create()) {
    }

    void PushExpectedLogLine(const std::string& expected_log_line) {
      CriticalSectionScoped cs(crit_sect_.get());
      expected_log_lines_.push_back(expected_log_line);
    }

    virtual void Print(TraceLevel level,
                       const char* message,
                       int length) OVERRIDE {
      CriticalSectionScoped cs(crit_sect_.get());
      if (!(level & kTraceStateInfo)) {
        return;
      }
      std::string msg(message);
      if (msg.find("BitrateEstimator") != std::string::npos) {
        received_log_lines_.push_back(msg);
      }
      int num_popped = 0;
      while (!received_log_lines_.empty() && !expected_log_lines_.empty()) {
        std::string a = received_log_lines_.front();
        std::string b = expected_log_lines_.front();
        received_log_lines_.pop_front();
        expected_log_lines_.pop_front();
        num_popped++;
        EXPECT_TRUE(a.find(b) != std::string::npos);
      }
      if (expected_log_lines_.size() <= 0) {
        if (num_popped > 0) {
          done_->Set();
        }
        return;
      }
    }

    EventTypeWrapper Wait() { return done_->Wait(kDefaultTimeoutMs); }

   private:
    typedef std::list<std::string> Strings;
    scoped_ptr<CriticalSectionWrapper> crit_sect_;
    Strings received_log_lines_;
    Strings expected_log_lines_;
    scoped_ptr<EventWrapper> done_;
  };

  class Stream {
   public:
    explicit Stream(BitrateEstimatorTest* test)
        : test_(test),
          is_sending_receiving_(false),
          send_stream_(NULL),
          receive_stream_(NULL),
          frame_generator_capturer_(),
          fake_encoder_(Clock::GetRealTimeClock()),
          fake_decoder_() {
      test_->send_config_.rtp.ssrcs[0]++;
      test_->send_config_.encoder = &fake_encoder_;
      send_stream_ =
          test_->sender_call_->CreateVideoSendStream(test_->send_config_);
      frame_generator_capturer_.reset(
          test::FrameGeneratorCapturer::Create(send_stream_->Input(),
                                               test_->send_config_.codec.width,
                                               test_->send_config_.codec.height,
                                               30,
                                               Clock::GetRealTimeClock()));
      send_stream_->StartSending();
      frame_generator_capturer_->Start();

      ExternalVideoDecoder decoder;
      decoder.decoder = &fake_decoder_;
      decoder.payload_type = test_->send_config_.codec.plType;
      test_->receive_config_.rtp.remote_ssrc = test_->send_config_.rtp.ssrcs[0];
      test_->receive_config_.rtp.local_ssrc++;
      test_->receive_config_.external_decoders.push_back(decoder);
      receive_stream_ = test_->receiver_call_->CreateVideoReceiveStream(
          test_->receive_config_);
      receive_stream_->StartReceiving();

      is_sending_receiving_ = true;
    }

    ~Stream() {
      frame_generator_capturer_.reset(NULL);
      test_->sender_call_->DestroyVideoSendStream(send_stream_);
      send_stream_ = NULL;
      test_->receiver_call_->DestroyVideoReceiveStream(receive_stream_);
      receive_stream_ = NULL;
    }

    void StopSending() {
      if (is_sending_receiving_) {
        frame_generator_capturer_->Stop();
        send_stream_->StopSending();
        receive_stream_->StopReceiving();
        is_sending_receiving_ = false;
      }
    }

   private:
    BitrateEstimatorTest* test_;
    bool is_sending_receiving_;
    VideoSendStream* send_stream_;
    VideoReceiveStream* receive_stream_;
    scoped_ptr<test::FrameGeneratorCapturer> frame_generator_capturer_;
    test::FakeEncoder fake_encoder_;
    test::FakeDecoder fake_decoder_;
  };

  TraceObserver receiver_trace_;
  test::DirectTransport send_transport_;
  test::DirectTransport receive_transport_;
  scoped_ptr<Call> sender_call_;
  scoped_ptr<Call> receiver_call_;
  VideoSendStream::Config send_config_;
  VideoReceiveStream::Config receive_config_;
  std::vector<Stream*> streams_;
};

TEST_F(BitrateEstimatorTest, InstantiatesTOFPerDefault) {
  send_config_.rtp.extensions.push_back(
      RtpExtension(RtpExtension::kTOffset, kTOFExtensionId));
  receiver_trace_.PushExpectedLogLine(
      "RemoteBitrateEstimatorFactory: Instantiating.");
  receiver_trace_.PushExpectedLogLine(
      "RemoteBitrateEstimatorFactory: Instantiating.");
  streams_.push_back(new Stream(this));
  EXPECT_EQ(kEventSignaled, receiver_trace_.Wait());
}

TEST_F(BitrateEstimatorTest, ImmediatelySwitchToAST) {
  send_config_.rtp.extensions.push_back(
      RtpExtension(RtpExtension::kAbsSendTime, kASTExtensionId));
  receiver_trace_.PushExpectedLogLine(
      "RemoteBitrateEstimatorFactory: Instantiating.");
  receiver_trace_.PushExpectedLogLine(
      "RemoteBitrateEstimatorFactory: Instantiating.");
  receiver_trace_.PushExpectedLogLine("Switching to absolute send time RBE.");
  receiver_trace_.PushExpectedLogLine(
      "AbsoluteSendTimeRemoteBitrateEstimatorFactory: Instantiating.");
  streams_.push_back(new Stream(this));
  EXPECT_EQ(kEventSignaled, receiver_trace_.Wait());
}

TEST_F(BitrateEstimatorTest, SwitchesToAST) {
  send_config_.rtp.extensions.push_back(
      RtpExtension(RtpExtension::kTOffset, kTOFExtensionId));
  receiver_trace_.PushExpectedLogLine(
      "RemoteBitrateEstimatorFactory: Instantiating.");
  receiver_trace_.PushExpectedLogLine(
      "RemoteBitrateEstimatorFactory: Instantiating.");
  streams_.push_back(new Stream(this));
  EXPECT_EQ(kEventSignaled, receiver_trace_.Wait());

  send_config_.rtp.extensions[0] =
      RtpExtension(RtpExtension::kAbsSendTime, kASTExtensionId);
  receiver_trace_.PushExpectedLogLine("Switching to absolute send time RBE.");
  receiver_trace_.PushExpectedLogLine(
      "AbsoluteSendTimeRemoteBitrateEstimatorFactory: Instantiating.");
  streams_.push_back(new Stream(this));
  EXPECT_EQ(kEventSignaled, receiver_trace_.Wait());
}

TEST_F(BitrateEstimatorTest, SwitchesToASTThenBackToTOF) {
  send_config_.rtp.extensions.push_back(
      RtpExtension(RtpExtension::kTOffset, kTOFExtensionId));
  receiver_trace_.PushExpectedLogLine(
      "RemoteBitrateEstimatorFactory: Instantiating.");
  receiver_trace_.PushExpectedLogLine(
      "RemoteBitrateEstimatorFactory: Instantiating.");
  streams_.push_back(new Stream(this));
  EXPECT_EQ(kEventSignaled, receiver_trace_.Wait());

  send_config_.rtp.extensions[0] =
      RtpExtension(RtpExtension::kAbsSendTime, kASTExtensionId);
  receiver_trace_.PushExpectedLogLine("Switching to absolute send time RBE.");
  receiver_trace_.PushExpectedLogLine(
      "AbsoluteSendTimeRemoteBitrateEstimatorFactory: Instantiating.");
  streams_.push_back(new Stream(this));
  EXPECT_EQ(kEventSignaled, receiver_trace_.Wait());

  send_config_.rtp.extensions[0] =
      RtpExtension(RtpExtension::kTOffset, kTOFExtensionId);
  receiver_trace_.PushExpectedLogLine(
      "WrappingBitrateEstimator: Switching to transmission time offset RBE.");
  receiver_trace_.PushExpectedLogLine(
      "RemoteBitrateEstimatorFactory: Instantiating.");
  streams_.push_back(new Stream(this));
  streams_[0]->StopSending();
  streams_[1]->StopSending();
  EXPECT_EQ(kEventSignaled, receiver_trace_.Wait());
}
}  // namespace webrtc
