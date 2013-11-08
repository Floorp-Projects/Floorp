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

#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/modules/rtp_rtcp/interface/receive_statistics.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_header_parser.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp.h"
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

namespace webrtc {

namespace {
  static const int kTOffsetExtensionId = 7;
}

class StreamObserver : public newapi::Transport, public RemoteBitrateObserver {
 public:
  typedef std::map<uint32_t, int> BytesSentMap;
  StreamObserver(int num_expected_ssrcs,
                 newapi::Transport* feedback_transport,
                 Clock* clock)
      : critical_section_(CriticalSectionWrapper::CreateCriticalSection()),
        all_ssrcs_sent_(EventWrapper::Create()),
        rtp_parser_(RtpHeaderParser::Create()),
        feedback_transport_(new TransportWrapper(feedback_transport)),
        receive_stats_(ReceiveStatistics::Create(clock)),
        clock_(clock),
        num_expected_ssrcs_(num_expected_ssrcs) {
    // Ideally we would only have to instantiate an RtcpSender, an
    // RtpHeaderParser and a RemoteBitrateEstimator here, but due to the current
    // state of the RTP module we need a full module and receive statistics to
    // be able to produce an RTCP with REMB.
    RtpRtcp::Configuration config;
    config.receive_statistics = receive_stats_.get();
    config.outgoing_transport = feedback_transport_.get();
    rtp_rtcp_.reset(RtpRtcp::CreateRtpRtcp(config));
    rtp_rtcp_->SetREMBStatus(true);
    rtp_rtcp_->SetRTCPStatus(kRtcpNonCompound);
    rtp_parser_->RegisterRtpHeaderExtension(kRtpExtensionTransmissionTimeOffset,
                                            kTOffsetExtensionId);
    AbsoluteSendTimeRemoteBitrateEstimatorFactory rbe_factory;
    remote_bitrate_estimator_.reset(rbe_factory.Create(this, clock));
  }

  virtual void OnReceiveBitrateChanged(const std::vector<unsigned int>& ssrcs,
                                       unsigned int bitrate) {
    CriticalSectionScoped lock(critical_section_.get());
    if (ssrcs.size() == num_expected_ssrcs_ && bitrate >= kExpectedBitrateBps)
      all_ssrcs_sent_->Set();
    rtp_rtcp_->SetREMBData(
        bitrate, static_cast<uint8_t>(ssrcs.size()), &ssrcs[0]);
    rtp_rtcp_->Process();
  }

  virtual bool SendRTP(const uint8_t* packet, size_t length) OVERRIDE {
    CriticalSectionScoped lock(critical_section_.get());
    RTPHeader header;
    EXPECT_TRUE(rtp_parser_->Parse(packet, static_cast<int>(length), &header));
    receive_stats_->IncomingPacket(header, length, false);
    rtp_rtcp_->SetRemoteSSRC(header.ssrc);
    remote_bitrate_estimator_->IncomingPacket(
        clock_->TimeInMilliseconds(), static_cast<int>(length - 12), header);
    if (remote_bitrate_estimator_->TimeUntilNextProcess() <= 0) {
      remote_bitrate_estimator_->Process();
    }
    return true;
  }

  virtual bool SendRTCP(const uint8_t* packet, size_t length) OVERRIDE {
    return true;
  }

  EventTypeWrapper Wait() { return all_ssrcs_sent_->Wait(120 * 1000); }

 private:
  class TransportWrapper : public webrtc::Transport {
   public:
    explicit TransportWrapper(newapi::Transport* new_transport)
        : new_transport_(new_transport) {}

    virtual int SendPacket(int channel, const void* data, int len) OVERRIDE {
      return new_transport_->SendRTP(static_cast<const uint8_t*>(data), len)
                 ? len
                 : -1;
    }

    virtual int SendRTCPPacket(int channel,
                               const void* data,
                               int len) OVERRIDE {
      return new_transport_->SendRTCP(static_cast<const uint8_t*>(data), len)
                 ? len
                 : -1;
    }

   private:
    newapi::Transport* new_transport_;
  };

  static const unsigned int kExpectedBitrateBps = 1200000;

  scoped_ptr<CriticalSectionWrapper> critical_section_;
  scoped_ptr<EventWrapper> all_ssrcs_sent_;
  scoped_ptr<RtpHeaderParser> rtp_parser_;
  scoped_ptr<RtpRtcp> rtp_rtcp_;
  scoped_ptr<TransportWrapper> feedback_transport_;
  scoped_ptr<ReceiveStatistics> receive_stats_;
  scoped_ptr<RemoteBitrateEstimator> remote_bitrate_estimator_;
  Clock* clock_;
  const size_t num_expected_ssrcs_;
};

class RampUpTest : public ::testing::TestWithParam<bool> {
 public:
  virtual void SetUp() { reserved_ssrcs_.clear(); }

 protected:
  std::map<uint32_t, bool> reserved_ssrcs_;
};

TEST_P(RampUpTest, RampUpWithPadding) {
  test::DirectTransport receiver_transport;
  StreamObserver stream_observer(
      3, &receiver_transport, Clock::GetRealTimeClock());
  Call::Config call_config(&stream_observer);
  scoped_ptr<Call> call(Call::Create(call_config));
  VideoSendStream::Config send_config = call->GetDefaultSendConfig();

  receiver_transport.SetReceiver(call->Receiver());

  test::FakeEncoder encoder(Clock::GetRealTimeClock());
  send_config.encoder = &encoder;
  send_config.internal_source = false;
  test::FakeEncoder::SetCodecSettings(&send_config.codec, 3);
  send_config.pacing = GetParam();
  send_config.rtp.extensions.push_back(
      RtpExtension("toffset", kTOffsetExtensionId));

  test::GenerateRandomSsrcs(&send_config, &reserved_ssrcs_);

  VideoSendStream* send_stream = call->CreateSendStream(send_config);

  VideoReceiveStream::Config receive_config;
  receive_config.rtp.ssrc = send_config.rtp.ssrcs[0];
  receive_config.rtp.nack.rtp_history_ms = send_config.rtp.nack.rtp_history_ms;
  VideoReceiveStream* receive_stream =
      call->CreateReceiveStream(receive_config);

  scoped_ptr<test::FrameGeneratorCapturer> frame_generator_capturer(
      test::FrameGeneratorCapturer::Create(send_stream->Input(),
                                           send_config.codec.width,
                                           send_config.codec.height,
                                           30,
                                           Clock::GetRealTimeClock()));

  receive_stream->StartReceive();
  send_stream->StartSend();
  frame_generator_capturer->Start();

  EXPECT_EQ(kEventSignaled, stream_observer.Wait());

  frame_generator_capturer->Stop();
  send_stream->StopSend();
  receive_stream->StopReceive();

  call->DestroyReceiveStream(receive_stream);
  call->DestroySendStream(send_stream);
}

INSTANTIATE_TEST_CASE_P(RampUpTest, RampUpTest, ::testing::Bool());

}  // namespace webrtc
