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

#include "webrtc/base/thread_annotations.h"
#include "webrtc/call.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/event_wrapper.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/system_wrappers/interface/trace.h"
#include "webrtc/test/call_test.h"
#include "webrtc/test/direct_transport.h"
#include "webrtc/test/encoder_settings.h"
#include "webrtc/test/fake_decoder.h"
#include "webrtc/test/fake_encoder.h"
#include "webrtc/test/frame_generator_capturer.h"

namespace webrtc {
namespace {
// Note: consider to write tests that don't depend on the trace system instead
// of re-using this class.
class TraceObserver {
 public:
  TraceObserver() {
    Trace::set_level_filter(kTraceTerseInfo);

    Trace::CreateTrace();
    Trace::SetTraceCallback(&callback_);

    // Call webrtc trace to initialize the tracer that would otherwise trigger a
    // data-race if left to be initialized by multiple threads (i.e. threads
    // spawned by test::DirectTransport members in BitrateEstimatorTest).
    WEBRTC_TRACE(kTraceStateInfo,
                 kTraceUtility,
                 -1,
                 "Instantiate without data races.");
  }

  ~TraceObserver() {
    Trace::SetTraceCallback(NULL);
    Trace::ReturnTrace();
  }

  void PushExpectedLogLine(const std::string& expected_log_line) {
    callback_.PushExpectedLogLine(expected_log_line);
  }

  EventTypeWrapper Wait() {
    return callback_.Wait();
  }

 private:
  class Callback : public TraceCallback {
   public:
    Callback()
        : crit_sect_(CriticalSectionWrapper::CreateCriticalSection()),
          done_(EventWrapper::Create()) {}

    virtual void Print(TraceLevel level,
                       const char* message,
                       int length) OVERRIDE {
      CriticalSectionScoped lock(crit_sect_.get());
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

    EventTypeWrapper Wait() {
      return done_->Wait(test::CallTest::kDefaultTimeoutMs);
    }

    void PushExpectedLogLine(const std::string& expected_log_line) {
      CriticalSectionScoped lock(crit_sect_.get());
      expected_log_lines_.push_back(expected_log_line);
    }

   private:
    typedef std::list<std::string> Strings;
    const scoped_ptr<CriticalSectionWrapper> crit_sect_;
    Strings received_log_lines_ GUARDED_BY(crit_sect_);
    Strings expected_log_lines_ GUARDED_BY(crit_sect_);
    scoped_ptr<EventWrapper> done_;
  };

  Callback callback_;
};
}  // namespace

static const int kTOFExtensionId = 4;
static const int kASTExtensionId = 5;

class BitrateEstimatorTest : public test::CallTest {
 public:
  BitrateEstimatorTest()
      : receiver_trace_(),
        send_transport_(),
        receive_transport_(),
        sender_call_(),
        receiver_call_(),
        receive_config_(),
        streams_() {
  }

  virtual ~BitrateEstimatorTest() {
    EXPECT_TRUE(streams_.empty());
  }

  virtual void SetUp() {
    Call::Config receiver_call_config(&receive_transport_);
    receiver_call_.reset(Call::Create(receiver_call_config));

    Call::Config sender_call_config(&send_transport_);
    sender_call_.reset(Call::Create(sender_call_config));

    send_transport_.SetReceiver(receiver_call_->Receiver());
    receive_transport_.SetReceiver(sender_call_->Receiver());

    send_config_ = VideoSendStream::Config();
    send_config_.rtp.ssrcs.push_back(kSendSsrcs[0]);
    // Encoders will be set separately per stream.
    send_config_.encoder_settings.encoder = NULL;
    send_config_.encoder_settings.payload_name = "FAKE";
    send_config_.encoder_settings.payload_type = kFakeSendPayloadType;
    encoder_config_.streams = test::CreateVideoStreams(1);

    receive_config_ = VideoReceiveStream::Config();
    // receive_config_.decoders will be set by every stream separately.
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

    receiver_call_.reset();
  }

 protected:
  friend class Stream;

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
      test_->send_config_.encoder_settings.encoder = &fake_encoder_;
      send_stream_ = test_->sender_call_->CreateVideoSendStream(
          test_->send_config_, test_->encoder_config_);
      assert(test_->encoder_config_.streams.size() == 1);
      frame_generator_capturer_.reset(test::FrameGeneratorCapturer::Create(
          send_stream_->Input(),
          test_->encoder_config_.streams[0].width,
          test_->encoder_config_.streams[0].height,
          30,
          Clock::GetRealTimeClock()));
      send_stream_->Start();
      frame_generator_capturer_->Start();

      VideoReceiveStream::Decoder decoder;
      decoder.decoder = &fake_decoder_;
      decoder.payload_type = test_->send_config_.encoder_settings.payload_type;
      decoder.payload_name = test_->send_config_.encoder_settings.payload_name;
      test_->receive_config_.decoders.push_back(decoder);
      test_->receive_config_.rtp.remote_ssrc = test_->send_config_.rtp.ssrcs[0];
      test_->receive_config_.rtp.local_ssrc++;
      receive_stream_ = test_->receiver_call_->CreateVideoReceiveStream(
          test_->receive_config_);
      receive_stream_->Start();

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
        send_stream_->Stop();
        receive_stream_->Stop();
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
