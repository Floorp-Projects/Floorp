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
#include <sstream>
#include <string>

#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/base/thread_annotations.h"
#include "webrtc/call.h"
#include "webrtc/modules/audio_coding/main/interface/audio_coding_module.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_header_parser.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_utility.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/rtp_to_ntp.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/test/call_test.h"
#include "webrtc/test/direct_transport.h"
#include "webrtc/test/encoder_settings.h"
#include "webrtc/test/fake_audio_device.h"
#include "webrtc/test/fake_decoder.h"
#include "webrtc/test/fake_encoder.h"
#include "webrtc/test/frame_generator.h"
#include "webrtc/test/frame_generator_capturer.h"
#include "webrtc/test/rtp_rtcp_observer.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/test/testsupport/perf_test.h"
#include "webrtc/video/transport_adapter.h"
#include "webrtc/voice_engine/include/voe_base.h"
#include "webrtc/voice_engine/include/voe_codec.h"
#include "webrtc/voice_engine/include/voe_network.h"
#include "webrtc/voice_engine/include/voe_rtp_rtcp.h"
#include "webrtc/voice_engine/include/voe_video_sync.h"

namespace webrtc {

class CallPerfTest : public test::CallTest {
 protected:
  void TestAudioVideoSync(bool fec);

  void TestMinTransmitBitrate(bool pad_to_min_bitrate);

  void TestCaptureNtpTime(const FakeNetworkPipe::Config& net_config,
                          int threshold_ms,
                          int start_time_ms,
                          int run_time_ms);
};

class SyncRtcpObserver : public test::RtpRtcpObserver {
 public:
  explicit SyncRtcpObserver(const FakeNetworkPipe::Config& config)
      : test::RtpRtcpObserver(CallPerfTest::kLongTimeoutMs, config),
        crit_(CriticalSectionWrapper::CreateCriticalSection()) {}

  virtual Action OnSendRtcp(const uint8_t* packet, size_t length) OVERRIDE {
    RTCPUtility::RTCPParserV2 parser(packet, length, true);
    EXPECT_TRUE(parser.IsValid());

    for (RTCPUtility::RTCPPacketTypes packet_type = parser.Begin();
         packet_type != RTCPUtility::kRtcpNotValidCode;
         packet_type = parser.Iterate()) {
      if (packet_type == RTCPUtility::kRtcpSrCode) {
        const RTCPUtility::RTCPPacket& packet = parser.Packet();
        RtcpMeasurement ntp_rtp_pair(
            packet.SR.NTPMostSignificant,
            packet.SR.NTPLeastSignificant,
            packet.SR.RTPTimestamp);
        StoreNtpRtpPair(ntp_rtp_pair);
      }
    }
    return SEND_PACKET;
  }

  int64_t RtpTimestampToNtp(uint32_t timestamp) const {
    CriticalSectionScoped lock(crit_.get());
    int64_t timestamp_in_ms = -1;
    if (ntp_rtp_pairs_.size() == 2) {
      // TODO(stefan): We can't EXPECT_TRUE on this call due to a bug in the
      // RTCP sender where it sends RTCP SR before any RTP packets, which leads
      // to a bogus NTP/RTP mapping.
      RtpToNtpMs(timestamp, ntp_rtp_pairs_, &timestamp_in_ms);
      return timestamp_in_ms;
    }
    return -1;
  }

 private:
  void StoreNtpRtpPair(RtcpMeasurement ntp_rtp_pair) {
    CriticalSectionScoped lock(crit_.get());
    for (RtcpList::iterator it = ntp_rtp_pairs_.begin();
         it != ntp_rtp_pairs_.end();
         ++it) {
      if (ntp_rtp_pair.ntp_secs == it->ntp_secs &&
          ntp_rtp_pair.ntp_frac == it->ntp_frac) {
        // This RTCP has already been added to the list.
        return;
      }
    }
    // We need two RTCP SR reports to map between RTP and NTP. More than two
    // will not improve the mapping.
    if (ntp_rtp_pairs_.size() == 2) {
      ntp_rtp_pairs_.pop_back();
    }
    ntp_rtp_pairs_.push_front(ntp_rtp_pair);
  }

  const scoped_ptr<CriticalSectionWrapper> crit_;
  RtcpList ntp_rtp_pairs_ GUARDED_BY(crit_);
};

class VideoRtcpAndSyncObserver : public SyncRtcpObserver, public VideoRenderer {
  static const int kInSyncThresholdMs = 50;
  static const int kStartupTimeMs = 2000;
  static const int kMinRunTimeMs = 30000;

 public:
  VideoRtcpAndSyncObserver(Clock* clock,
                           int voe_channel,
                           VoEVideoSync* voe_sync,
                           SyncRtcpObserver* audio_observer)
      : SyncRtcpObserver(FakeNetworkPipe::Config()),
        clock_(clock),
        voe_channel_(voe_channel),
        voe_sync_(voe_sync),
        audio_observer_(audio_observer),
        creation_time_ms_(clock_->TimeInMilliseconds()),
        first_time_in_sync_(-1) {}

  virtual void RenderFrame(const I420VideoFrame& video_frame,
                           int time_to_render_ms) OVERRIDE {
    int64_t now_ms = clock_->TimeInMilliseconds();
    uint32_t playout_timestamp = 0;
    if (voe_sync_->GetPlayoutTimestamp(voe_channel_, playout_timestamp) != 0)
      return;
    int64_t latest_audio_ntp =
        audio_observer_->RtpTimestampToNtp(playout_timestamp);
    int64_t latest_video_ntp = RtpTimestampToNtp(video_frame.timestamp());
    if (latest_audio_ntp < 0 || latest_video_ntp < 0)
      return;
    int time_until_render_ms =
        std::max(0, static_cast<int>(video_frame.render_time_ms() - now_ms));
    latest_video_ntp += time_until_render_ms;
    int64_t stream_offset = latest_audio_ntp - latest_video_ntp;
    std::stringstream ss;
    ss << stream_offset;
    webrtc::test::PrintResult("stream_offset",
                              "",
                              "synchronization",
                              ss.str(),
                              "ms",
                              false);
    int64_t time_since_creation = now_ms - creation_time_ms_;
    // During the first couple of seconds audio and video can falsely be
    // estimated as being synchronized. We don't want to trigger on those.
    if (time_since_creation < kStartupTimeMs)
      return;
    if (std::abs(latest_audio_ntp - latest_video_ntp) < kInSyncThresholdMs) {
      if (first_time_in_sync_ == -1) {
        first_time_in_sync_ = now_ms;
        webrtc::test::PrintResult("sync_convergence_time",
                                  "",
                                  "synchronization",
                                  time_since_creation,
                                  "ms",
                                  false);
      }
      if (time_since_creation > kMinRunTimeMs)
        observation_complete_->Set();
    }
  }

 private:
  Clock* const clock_;
  int voe_channel_;
  VoEVideoSync* voe_sync_;
  SyncRtcpObserver* audio_observer_;
  int64_t creation_time_ms_;
  int64_t first_time_in_sync_;
};

void CallPerfTest::TestAudioVideoSync(bool fec) {
  class AudioPacketReceiver : public PacketReceiver {
   public:
    AudioPacketReceiver(int channel, VoENetwork* voe_network)
        : channel_(channel),
          voe_network_(voe_network),
          parser_(RtpHeaderParser::Create()) {}
    virtual DeliveryStatus DeliverPacket(const uint8_t* packet,
                                         size_t length) OVERRIDE {
      int ret;
      if (parser_->IsRtcp(packet, static_cast<int>(length))) {
        ret = voe_network_->ReceivedRTCPPacket(
            channel_, packet, static_cast<unsigned int>(length));
      } else {
        ret = voe_network_->ReceivedRTPPacket(
            channel_, packet, static_cast<unsigned int>(length), PacketTime());
      }
      return ret == 0 ? DELIVERY_OK : DELIVERY_PACKET_ERROR;
    }

   private:
    int channel_;
    VoENetwork* voe_network_;
    scoped_ptr<RtpHeaderParser> parser_;
  };

  VoiceEngine* voice_engine = VoiceEngine::Create();
  VoEBase* voe_base = VoEBase::GetInterface(voice_engine);
  VoECodec* voe_codec = VoECodec::GetInterface(voice_engine);
  VoENetwork* voe_network = VoENetwork::GetInterface(voice_engine);
  VoEVideoSync* voe_sync = VoEVideoSync::GetInterface(voice_engine);
  const std::string audio_filename =
      test::ResourcePath("voice_engine/audio_long16", "pcm");
  ASSERT_STRNE("", audio_filename.c_str());
  test::FakeAudioDevice fake_audio_device(Clock::GetRealTimeClock(),
                                          audio_filename);
  EXPECT_EQ(0, voe_base->Init(&fake_audio_device, NULL));
  int channel = voe_base->CreateChannel();

  FakeNetworkPipe::Config net_config;
  net_config.queue_delay_ms = 500;
  net_config.loss_percent = 5;
  SyncRtcpObserver audio_observer(net_config);
  VideoRtcpAndSyncObserver observer(Clock::GetRealTimeClock(),
                                    channel,
                                    voe_sync,
                                    &audio_observer);

  Call::Config receiver_config(observer.ReceiveTransport());
  receiver_config.voice_engine = voice_engine;
  CreateCalls(Call::Config(observer.SendTransport()), receiver_config);

  CodecInst isac = {103, "ISAC", 16000, 480, 1, 32000};
  EXPECT_EQ(0, voe_codec->SetSendCodec(channel, isac));

  AudioPacketReceiver voe_packet_receiver(channel, voe_network);
  audio_observer.SetReceivers(&voe_packet_receiver, &voe_packet_receiver);

  internal::TransportAdapter transport_adapter(audio_observer.SendTransport());
  transport_adapter.Enable();
  EXPECT_EQ(0,
            voe_network->RegisterExternalTransport(channel, transport_adapter));

  observer.SetReceivers(receiver_call_->Receiver(), sender_call_->Receiver());

  test::FakeDecoder fake_decoder;

  CreateSendConfig(1);
  CreateMatchingReceiveConfigs();

  send_config_.rtp.nack.rtp_history_ms = kNackRtpHistoryMs;
  if (fec) {
    send_config_.rtp.fec.red_payload_type = kRedPayloadType;
    send_config_.rtp.fec.ulpfec_payload_type = kUlpfecPayloadType;
    receive_configs_[0].rtp.fec.red_payload_type = kRedPayloadType;
    receive_configs_[0].rtp.fec.ulpfec_payload_type = kUlpfecPayloadType;
  }
  receive_configs_[0].rtp.nack.rtp_history_ms = 1000;
  receive_configs_[0].renderer = &observer;
  receive_configs_[0].audio_channel_id = channel;

  CreateStreams();

  CreateFrameGeneratorCapturer();

  Start();

  fake_audio_device.Start();
  EXPECT_EQ(0, voe_base->StartPlayout(channel));
  EXPECT_EQ(0, voe_base->StartReceive(channel));
  EXPECT_EQ(0, voe_base->StartSend(channel));

  EXPECT_EQ(kEventSignaled, observer.Wait())
      << "Timed out while waiting for audio and video to be synchronized.";

  EXPECT_EQ(0, voe_base->StopSend(channel));
  EXPECT_EQ(0, voe_base->StopReceive(channel));
  EXPECT_EQ(0, voe_base->StopPlayout(channel));
  fake_audio_device.Stop();

  Stop();
  observer.StopSending();
  audio_observer.StopSending();

  voe_base->DeleteChannel(channel);
  voe_base->Release();
  voe_codec->Release();
  voe_network->Release();
  voe_sync->Release();

  DestroyStreams();

  VoiceEngine::Delete(voice_engine);
}

TEST_F(CallPerfTest, PlaysOutAudioAndVideoInSync) {
  TestAudioVideoSync(false);
}

TEST_F(CallPerfTest, PlaysOutAudioAndVideoInSyncWithFec) {
  TestAudioVideoSync(true);
}

void CallPerfTest::TestCaptureNtpTime(const FakeNetworkPipe::Config& net_config,
                                      int threshold_ms,
                                      int start_time_ms,
                                      int run_time_ms) {
  class CaptureNtpTimeObserver : public test::EndToEndTest,
                                 public VideoRenderer {
   public:
    CaptureNtpTimeObserver(const FakeNetworkPipe::Config& config,
                           int threshold_ms,
                           int start_time_ms,
                           int run_time_ms)
        : EndToEndTest(kLongTimeoutMs, config),
          clock_(Clock::GetRealTimeClock()),
          threshold_ms_(threshold_ms),
          start_time_ms_(start_time_ms),
          run_time_ms_(run_time_ms),
          creation_time_ms_(clock_->TimeInMilliseconds()),
          capturer_(NULL),
          rtp_start_timestamp_set_(false),
          rtp_start_timestamp_(0) {}

   private:
    virtual void RenderFrame(const I420VideoFrame& video_frame,
                             int time_to_render_ms) OVERRIDE {
      if (video_frame.ntp_time_ms() <= 0) {
        // Haven't got enough RTCP SR in order to calculate the capture ntp
        // time.
        return;
      }

      int64_t now_ms = clock_->TimeInMilliseconds();
      int64_t time_since_creation = now_ms - creation_time_ms_;
      if (time_since_creation < start_time_ms_) {
        // Wait for |start_time_ms_| before start measuring.
        return;
      }

      if (time_since_creation > run_time_ms_) {
        observation_complete_->Set();
      }

      FrameCaptureTimeList::iterator iter =
          capture_time_list_.find(video_frame.timestamp());
      EXPECT_TRUE(iter != capture_time_list_.end());

      // The real capture time has been wrapped to uint32_t before converted
      // to rtp timestamp in the sender side. So here we convert the estimated
      // capture time to a uint32_t 90k timestamp also for comparing.
      uint32_t estimated_capture_timestamp =
          90 * static_cast<uint32_t>(video_frame.ntp_time_ms());
      uint32_t real_capture_timestamp = iter->second;
      int time_offset_ms = real_capture_timestamp - estimated_capture_timestamp;
      time_offset_ms = time_offset_ms / 90;
      std::stringstream ss;
      ss << time_offset_ms;

      webrtc::test::PrintResult(
          "capture_ntp_time", "", "real - estimated", ss.str(), "ms", true);
      EXPECT_TRUE(std::abs(time_offset_ms) < threshold_ms_);
    }

    virtual Action OnSendRtp(const uint8_t* packet, size_t length) {
      RTPHeader header;
      EXPECT_TRUE(parser_->Parse(packet, length, &header));

      if (!rtp_start_timestamp_set_) {
        // Calculate the rtp timestamp offset in order to calculate the real
        // capture time.
        uint32_t first_capture_timestamp =
            90 * static_cast<uint32_t>(capturer_->first_frame_capture_time());
        rtp_start_timestamp_ = header.timestamp - first_capture_timestamp;
        rtp_start_timestamp_set_ = true;
      }

      uint32_t capture_timestamp = header.timestamp - rtp_start_timestamp_;
      capture_time_list_.insert(
          capture_time_list_.end(),
          std::make_pair(header.timestamp, capture_timestamp));
      return SEND_PACKET;
    }

    virtual void OnFrameGeneratorCapturerCreated(
        test::FrameGeneratorCapturer* frame_generator_capturer) OVERRIDE {
      capturer_ = frame_generator_capturer;
    }

    virtual void ModifyConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) OVERRIDE {
      (*receive_configs)[0].renderer = this;
      // Enable the receiver side rtt calculation.
      (*receive_configs)[0].rtp.rtcp_xr.receiver_reference_time_report = true;
    }

    virtual void PerformTest() OVERRIDE {
      EXPECT_EQ(kEventSignaled, Wait()) << "Timed out while waiting for "
                                           "estimated capture NTP time to be "
                                           "within bounds.";
    }

    Clock* clock_;
    int threshold_ms_;
    int start_time_ms_;
    int run_time_ms_;
    int64_t creation_time_ms_;
    test::FrameGeneratorCapturer* capturer_;
    bool rtp_start_timestamp_set_;
    uint32_t rtp_start_timestamp_;
    typedef std::map<uint32_t, uint32_t> FrameCaptureTimeList;
    FrameCaptureTimeList capture_time_list_;
  } test(net_config, threshold_ms, start_time_ms, run_time_ms);

  RunBaseTest(&test);
}

TEST_F(CallPerfTest, CaptureNtpTimeWithNetworkDelay) {
  FakeNetworkPipe::Config net_config;
  net_config.queue_delay_ms = 100;
  // TODO(wu): lower the threshold as the calculation/estimatation becomes more
  // accurate.
  const int kThresholdMs = 100;
  const int kStartTimeMs = 10000;
  const int kRunTimeMs = 20000;
  TestCaptureNtpTime(net_config, kThresholdMs, kStartTimeMs, kRunTimeMs);
}

TEST_F(CallPerfTest, CaptureNtpTimeWithNetworkJitter) {
  FakeNetworkPipe::Config net_config;
  net_config.queue_delay_ms = 100;
  net_config.delay_standard_deviation_ms = 10;
  // TODO(wu): lower the threshold as the calculation/estimatation becomes more
  // accurate.
  const int kThresholdMs = 100;
  const int kStartTimeMs = 10000;
  const int kRunTimeMs = 20000;
  TestCaptureNtpTime(net_config, kThresholdMs, kStartTimeMs, kRunTimeMs);
}

TEST_F(CallPerfTest, RegisterCpuOveruseObserver) {
  // Verifies that either a normal or overuse callback is triggered.
  class LoadObserver : public test::SendTest, public webrtc::LoadObserver {
   public:
    LoadObserver() : SendTest(kLongTimeoutMs) {}

    virtual void OnLoadUpdate(Load load) OVERRIDE {
      observation_complete_->Set();
    }

    virtual Call::Config GetSenderCallConfig() OVERRIDE {
      Call::Config config(SendTransport());
      config.overuse_callback = this;
      return config;
    }

    virtual void PerformTest() OVERRIDE {
      EXPECT_EQ(kEventSignaled, Wait())
          << "Timed out before receiving an overuse callback.";
    }
  } test;

  RunBaseTest(&test);
}

void CallPerfTest::TestMinTransmitBitrate(bool pad_to_min_bitrate) {
  static const int kMaxEncodeBitrateKbps = 30;
  static const int kMinTransmitBitrateBps = 150000;
  static const int kMinAcceptableTransmitBitrate = 130;
  static const int kMaxAcceptableTransmitBitrate = 170;
  static const int kNumBitrateObservationsInRange = 100;
  class BitrateObserver : public test::EndToEndTest, public PacketReceiver {
   public:
    explicit BitrateObserver(bool using_min_transmit_bitrate)
        : EndToEndTest(kLongTimeoutMs),
          send_stream_(NULL),
          send_transport_receiver_(NULL),
          pad_to_min_bitrate_(using_min_transmit_bitrate),
          num_bitrate_observations_in_range_(0) {}

   private:
    virtual void SetReceivers(PacketReceiver* send_transport_receiver,
                              PacketReceiver* receive_transport_receiver)
        OVERRIDE {
      send_transport_receiver_ = send_transport_receiver;
      test::RtpRtcpObserver::SetReceivers(this, receive_transport_receiver);
    }

    virtual DeliveryStatus DeliverPacket(const uint8_t* packet,
                                         size_t length) OVERRIDE {
      VideoSendStream::Stats stats = send_stream_->GetStats();
      if (stats.substreams.size() > 0) {
        assert(stats.substreams.size() == 1);
        int bitrate_kbps =
            stats.substreams.begin()->second.total_bitrate_bps / 1000;
        if (bitrate_kbps > 0) {
          test::PrintResult(
              "bitrate_stats_",
              (pad_to_min_bitrate_ ? "min_transmit_bitrate"
                                   : "without_min_transmit_bitrate"),
              "bitrate_kbps",
              static_cast<size_t>(bitrate_kbps),
              "kbps",
              false);
          if (pad_to_min_bitrate_) {
            if (bitrate_kbps > kMinAcceptableTransmitBitrate &&
                bitrate_kbps < kMaxAcceptableTransmitBitrate) {
              ++num_bitrate_observations_in_range_;
            }
          } else {
            // Expect bitrate stats to roughly match the max encode bitrate.
            if (bitrate_kbps > kMaxEncodeBitrateKbps - 5 &&
                bitrate_kbps < kMaxEncodeBitrateKbps + 5) {
              ++num_bitrate_observations_in_range_;
            }
          }
          if (num_bitrate_observations_in_range_ ==
              kNumBitrateObservationsInRange)
            observation_complete_->Set();
        }
      }
      return send_transport_receiver_->DeliverPacket(packet, length);
    }

    virtual void OnStreamsCreated(
        VideoSendStream* send_stream,
        const std::vector<VideoReceiveStream*>& receive_streams) OVERRIDE {
      send_stream_ = send_stream;
    }

    virtual void ModifyConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) OVERRIDE {
      if (pad_to_min_bitrate_) {
        encoder_config->min_transmit_bitrate_bps = kMinTransmitBitrateBps;
      } else {
        assert(encoder_config->min_transmit_bitrate_bps == 0);
      }
    }

    virtual void PerformTest() OVERRIDE {
      EXPECT_EQ(kEventSignaled, Wait())
          << "Timeout while waiting for send-bitrate stats.";
    }

    VideoSendStream* send_stream_;
    PacketReceiver* send_transport_receiver_;
    const bool pad_to_min_bitrate_;
    int num_bitrate_observations_in_range_;
  } test(pad_to_min_bitrate);

  fake_encoder_.SetMaxBitrate(kMaxEncodeBitrateKbps);
  RunBaseTest(&test);
}

TEST_F(CallPerfTest, PadsToMinTransmitBitrate) { TestMinTransmitBitrate(true); }

TEST_F(CallPerfTest, NoPadWithoutMinTransmitBitrate) {
  TestMinTransmitBitrate(false);
}

TEST_F(CallPerfTest, KeepsHighBitrateWhenReconfiguringSender) {
  static const uint32_t kInitialBitrateKbps = 400;
  static const uint32_t kReconfigureThresholdKbps = 600;
  static const uint32_t kPermittedReconfiguredBitrateDiffKbps = 100;

  class BitrateObserver : public test::EndToEndTest, public test::FakeEncoder {
   public:
    BitrateObserver()
        : EndToEndTest(kDefaultTimeoutMs),
          FakeEncoder(Clock::GetRealTimeClock()),
          time_to_reconfigure_(webrtc::EventWrapper::Create()),
          encoder_inits_(0) {}

    virtual int32_t InitEncode(const VideoCodec* config,
                               int32_t number_of_cores,
                               uint32_t max_payload_size) OVERRIDE {
      if (encoder_inits_ == 0) {
        EXPECT_EQ(kInitialBitrateKbps, config->startBitrate)
            << "Encoder not initialized at expected bitrate.";
      }
      ++encoder_inits_;
      if (encoder_inits_ == 2) {
        EXPECT_GE(last_set_bitrate_, kReconfigureThresholdKbps);
        EXPECT_NEAR(config->startBitrate,
                    last_set_bitrate_,
                    kPermittedReconfiguredBitrateDiffKbps)
            << "Encoder reconfigured with bitrate too far away from last set.";
        observation_complete_->Set();
      }
      return FakeEncoder::InitEncode(config, number_of_cores, max_payload_size);
    }

    virtual int32_t SetRates(uint32_t new_target_bitrate_kbps,
                             uint32_t framerate) OVERRIDE {
      last_set_bitrate_ = new_target_bitrate_kbps;
      if (encoder_inits_ == 1 &&
          new_target_bitrate_kbps > kReconfigureThresholdKbps) {
        time_to_reconfigure_->Set();
      }
      return FakeEncoder::SetRates(new_target_bitrate_kbps, framerate);
    }

    Call::Config GetSenderCallConfig() OVERRIDE {
      Call::Config config = EndToEndTest::GetSenderCallConfig();
      config.stream_start_bitrate_bps = kInitialBitrateKbps * 1000;
      return config;
    }

    virtual void ModifyConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) OVERRIDE {
      send_config->encoder_settings.encoder = this;
      encoder_config->streams[0].min_bitrate_bps = 50000;
      encoder_config->streams[0].target_bitrate_bps =
          encoder_config->streams[0].max_bitrate_bps = 2000000;

      encoder_config_ = *encoder_config;
    }

    virtual void OnStreamsCreated(
        VideoSendStream* send_stream,
        const std::vector<VideoReceiveStream*>& receive_streams) OVERRIDE {
      send_stream_ = send_stream;
    }

    virtual void PerformTest() OVERRIDE {
      ASSERT_EQ(kEventSignaled, time_to_reconfigure_->Wait(kDefaultTimeoutMs))
          << "Timed out before receiving an initial high bitrate.";
      encoder_config_.streams[0].width *= 2;
      encoder_config_.streams[0].height *= 2;
      EXPECT_TRUE(send_stream_->ReconfigureVideoEncoder(encoder_config_));
      EXPECT_EQ(kEventSignaled, Wait())
          << "Timed out while waiting for a couple of high bitrate estimates "
             "after reconfiguring the send stream.";
    }

   private:
    scoped_ptr<webrtc::EventWrapper> time_to_reconfigure_;
    int encoder_inits_;
    uint32_t last_set_bitrate_;
    VideoSendStream* send_stream_;
    VideoEncoderConfig encoder_config_;
  } test;

  RunBaseTest(&test);
}

}  // namespace webrtc
