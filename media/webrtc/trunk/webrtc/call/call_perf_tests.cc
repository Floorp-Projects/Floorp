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
#include <sstream>
#include <string>

#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/base/checks.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/call.h"
#include "webrtc/call/transport_adapter.h"
#include "webrtc/common.h"
#include "webrtc/config.h"
#include "webrtc/modules/audio_coding/include/audio_coding_module.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_header_parser.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_utility.h"
#include "webrtc/system_wrappers/include/critical_section_wrapper.h"
#include "webrtc/system_wrappers/include/rtp_to_ntp.h"
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
#include "webrtc/voice_engine/include/voe_base.h"
#include "webrtc/voice_engine/include/voe_codec.h"
#include "webrtc/voice_engine/include/voe_network.h"
#include "webrtc/voice_engine/include/voe_rtp_rtcp.h"
#include "webrtc/voice_engine/include/voe_video_sync.h"

namespace webrtc {

class CallPerfTest : public test::CallTest {
 protected:
  void TestAudioVideoSync(bool fec, bool create_audio_first);

  void TestCpuOveruse(LoadObserver::Load tested_load, int encode_delay_ms);

  void TestMinTransmitBitrate(bool pad_to_min_bitrate);

  void TestCaptureNtpTime(const FakeNetworkPipe::Config& net_config,
                          int threshold_ms,
                          int start_time_ms,
                          int run_time_ms);
};

class SyncRtcpObserver : public test::RtpRtcpObserver {
 public:
  SyncRtcpObserver() : test::RtpRtcpObserver(CallPerfTest::kLongTimeoutMs) {}

  Action OnSendRtcp(const uint8_t* packet, size_t length) override {
    RTCPUtility::RTCPParserV2 parser(packet, length, true);
    EXPECT_TRUE(parser.IsValid());

    for (RTCPUtility::RTCPPacketTypes packet_type = parser.Begin();
         packet_type != RTCPUtility::RTCPPacketTypes::kInvalid;
         packet_type = parser.Iterate()) {
      if (packet_type == RTCPUtility::RTCPPacketTypes::kSr) {
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
    rtc::CritScope lock(&crit_);
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
    rtc::CritScope lock(&crit_);
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

  mutable rtc::CriticalSection crit_;
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
      : clock_(clock),
        voe_channel_(voe_channel),
        voe_sync_(voe_sync),
        audio_observer_(audio_observer),
        creation_time_ms_(clock_->TimeInMilliseconds()),
        first_time_in_sync_(-1) {}

  void RenderFrame(const VideoFrame& video_frame,
                   int time_to_render_ms) override {
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
        observation_complete_.Set();
    }
  }

  bool IsTextureSupported() const override { return false; }

 private:
  Clock* const clock_;
  const int voe_channel_;
  VoEVideoSync* const voe_sync_;
  SyncRtcpObserver* const audio_observer_;
  const int64_t creation_time_ms_;
  int64_t first_time_in_sync_;
};

void CallPerfTest::TestAudioVideoSync(bool fec, bool create_audio_first) {
  const char* kSyncGroup = "av_sync";
  const uint32_t kAudioSendSsrc = 1234;
  const uint32_t kAudioRecvSsrc = 5678;
  class AudioPacketReceiver : public PacketReceiver {
   public:
    AudioPacketReceiver(int channel, VoENetwork* voe_network)
        : channel_(channel),
          voe_network_(voe_network),
          parser_(RtpHeaderParser::Create()) {}
    DeliveryStatus DeliverPacket(MediaType media_type,
                                 const uint8_t* packet,
                                 size_t length,
                                 const PacketTime& packet_time) override {
      EXPECT_TRUE(media_type == MediaType::ANY ||
                  media_type == MediaType::AUDIO);
      int ret;
      if (parser_->IsRtcp(packet, length)) {
        ret = voe_network_->ReceivedRTCPPacket(channel_, packet, length);
      } else {
        ret = voe_network_->ReceivedRTPPacket(channel_, packet, length,
                                              PacketTime());
      }
      return ret == 0 ? DELIVERY_OK : DELIVERY_PACKET_ERROR;
    }

   private:
    int channel_;
    VoENetwork* voe_network_;
    rtc::scoped_ptr<RtpHeaderParser> parser_;
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
  EXPECT_EQ(0, voe_base->Init(&fake_audio_device, nullptr));
  Config voe_config;
  voe_config.Set<VoicePacing>(new VoicePacing(true));
  int send_channel_id = voe_base->CreateChannel(voe_config);
  int recv_channel_id = voe_base->CreateChannel();

  SyncRtcpObserver audio_observer;

  AudioState::Config send_audio_state_config;
  send_audio_state_config.voice_engine = voice_engine;
  Call::Config sender_config;
  sender_config.audio_state = AudioState::Create(send_audio_state_config);
  Call::Config receiver_config;
  receiver_config.audio_state = sender_config.audio_state;
  CreateCalls(sender_config, receiver_config);

  AudioPacketReceiver voe_send_packet_receiver(send_channel_id, voe_network);
  AudioPacketReceiver voe_recv_packet_receiver(recv_channel_id, voe_network);

  FakeNetworkPipe::Config net_config;
  net_config.queue_delay_ms = 500;
  net_config.loss_percent = 5;
  test::PacketTransport audio_send_transport(
      nullptr, &audio_observer, test::PacketTransport::kSender, net_config);
  audio_send_transport.SetReceiver(&voe_recv_packet_receiver);
  test::PacketTransport audio_receive_transport(
      nullptr, &audio_observer, test::PacketTransport::kReceiver, net_config);
  audio_receive_transport.SetReceiver(&voe_send_packet_receiver);

  internal::TransportAdapter send_transport_adapter(&audio_send_transport);
  send_transport_adapter.Enable();
  EXPECT_EQ(0, voe_network->RegisterExternalTransport(send_channel_id,
                                                      send_transport_adapter));

  internal::TransportAdapter recv_transport_adapter(&audio_receive_transport);
  recv_transport_adapter.Enable();
  EXPECT_EQ(0, voe_network->RegisterExternalTransport(recv_channel_id,
                                                      recv_transport_adapter));

  VideoRtcpAndSyncObserver observer(Clock::GetRealTimeClock(), recv_channel_id,
                                    voe_sync, &audio_observer);

  test::PacketTransport sync_send_transport(sender_call_.get(), &observer,
                                            test::PacketTransport::kSender,
                                            FakeNetworkPipe::Config());
  sync_send_transport.SetReceiver(receiver_call_->Receiver());
  test::PacketTransport sync_receive_transport(receiver_call_.get(), &observer,
                                               test::PacketTransport::kReceiver,
                                               FakeNetworkPipe::Config());
  sync_receive_transport.SetReceiver(sender_call_->Receiver());

  test::FakeDecoder fake_decoder;

  CreateSendConfig(1, 0, &sync_send_transport);
  CreateMatchingReceiveConfigs(&sync_receive_transport);

  AudioSendStream::Config audio_send_config(&audio_send_transport);
  audio_send_config.voe_channel_id = send_channel_id;
  audio_send_config.rtp.ssrc = kAudioSendSsrc;
  AudioSendStream* audio_send_stream =
      sender_call_->CreateAudioSendStream(audio_send_config);

  CodecInst isac = {103, "ISAC", 16000, 480, 1, 32000};
  EXPECT_EQ(0, voe_codec->SetSendCodec(send_channel_id, isac));

  video_send_config_.rtp.nack.rtp_history_ms = kNackRtpHistoryMs;
  if (fec) {
    video_send_config_.rtp.fec.red_payload_type = kRedPayloadType;
    video_send_config_.rtp.fec.ulpfec_payload_type = kUlpfecPayloadType;
    video_receive_configs_[0].rtp.fec.red_payload_type = kRedPayloadType;
    video_receive_configs_[0].rtp.fec.ulpfec_payload_type = kUlpfecPayloadType;
  }
  video_receive_configs_[0].rtp.nack.rtp_history_ms = 1000;
  video_receive_configs_[0].renderer = &observer;
  video_receive_configs_[0].sync_group = kSyncGroup;

  AudioReceiveStream::Config audio_recv_config;
  audio_recv_config.rtp.remote_ssrc = kAudioSendSsrc;
  audio_recv_config.rtp.local_ssrc = kAudioRecvSsrc;
  audio_recv_config.voe_channel_id = recv_channel_id;
  audio_recv_config.sync_group = kSyncGroup;

  AudioReceiveStream* audio_receive_stream;

  if (create_audio_first) {
    audio_receive_stream =
        receiver_call_->CreateAudioReceiveStream(audio_recv_config);
    CreateVideoStreams();
  } else {
    CreateVideoStreams();
    audio_receive_stream =
        receiver_call_->CreateAudioReceiveStream(audio_recv_config);
  }

  CreateFrameGeneratorCapturer();

  Start();

  fake_audio_device.Start();
  EXPECT_EQ(0, voe_base->StartPlayout(recv_channel_id));
  EXPECT_EQ(0, voe_base->StartReceive(recv_channel_id));
  EXPECT_EQ(0, voe_base->StartSend(send_channel_id));

  EXPECT_TRUE(observer.Wait())
      << "Timed out while waiting for audio and video to be synchronized.";

  EXPECT_EQ(0, voe_base->StopSend(send_channel_id));
  EXPECT_EQ(0, voe_base->StopReceive(recv_channel_id));
  EXPECT_EQ(0, voe_base->StopPlayout(recv_channel_id));
  fake_audio_device.Stop();

  Stop();
  sync_send_transport.StopSending();
  sync_receive_transport.StopSending();
  audio_send_transport.StopSending();
  audio_receive_transport.StopSending();

  DestroyStreams();

  sender_call_->DestroyAudioSendStream(audio_send_stream);
  receiver_call_->DestroyAudioReceiveStream(audio_receive_stream);

  voe_base->DeleteChannel(send_channel_id);
  voe_base->DeleteChannel(recv_channel_id);
  voe_base->Release();
  voe_codec->Release();
  voe_network->Release();
  voe_sync->Release();

  DestroyCalls();

  VoiceEngine::Delete(voice_engine);
}

TEST_F(CallPerfTest, PlaysOutAudioAndVideoInSyncWithAudioCreatedFirst) {
  TestAudioVideoSync(false, true);
}

TEST_F(CallPerfTest, PlaysOutAudioAndVideoInSyncWithVideoCreatedFirst) {
  TestAudioVideoSync(false, false);
}

TEST_F(CallPerfTest, PlaysOutAudioAndVideoInSyncWithFec) {
  TestAudioVideoSync(true, false);
}

void CallPerfTest::TestCaptureNtpTime(const FakeNetworkPipe::Config& net_config,
                                      int threshold_ms,
                                      int start_time_ms,
                                      int run_time_ms) {
  class CaptureNtpTimeObserver : public test::EndToEndTest,
                                 public VideoRenderer {
   public:
    CaptureNtpTimeObserver(const FakeNetworkPipe::Config& net_config,
                           int threshold_ms,
                           int start_time_ms,
                           int run_time_ms)
        : EndToEndTest(kLongTimeoutMs),
          net_config_(net_config),
          clock_(Clock::GetRealTimeClock()),
          threshold_ms_(threshold_ms),
          start_time_ms_(start_time_ms),
          run_time_ms_(run_time_ms),
          creation_time_ms_(clock_->TimeInMilliseconds()),
          capturer_(nullptr),
          rtp_start_timestamp_set_(false),
          rtp_start_timestamp_(0) {}

   private:
    test::PacketTransport* CreateSendTransport(Call* sender_call) override {
      return new test::PacketTransport(
          sender_call, this, test::PacketTransport::kSender, net_config_);
    }

    test::PacketTransport* CreateReceiveTransport() override {
      return new test::PacketTransport(
          nullptr, this, test::PacketTransport::kReceiver, net_config_);
    }

    void RenderFrame(const VideoFrame& video_frame,
                     int time_to_render_ms) override {
      rtc::CritScope lock(&crit_);
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
        observation_complete_.Set();
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

    bool IsTextureSupported() const override { return false; }

    virtual Action OnSendRtp(const uint8_t* packet, size_t length) {
      rtc::CritScope lock(&crit_);
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

    void OnFrameGeneratorCapturerCreated(
        test::FrameGeneratorCapturer* frame_generator_capturer) override {
      capturer_ = frame_generator_capturer;
    }

    void ModifyVideoConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) override {
      (*receive_configs)[0].renderer = this;
      // Enable the receiver side rtt calculation.
      (*receive_configs)[0].rtp.rtcp_xr.receiver_reference_time_report = true;
    }

    void PerformTest() override {
      EXPECT_TRUE(Wait()) << "Timed out while waiting for "
                             "estimated capture NTP time to be "
                             "within bounds.";
    }

    rtc::CriticalSection crit_;
    const FakeNetworkPipe::Config net_config_;
    Clock* const clock_;
    int threshold_ms_;
    int start_time_ms_;
    int run_time_ms_;
    int64_t creation_time_ms_;
    test::FrameGeneratorCapturer* capturer_;
    bool rtp_start_timestamp_set_;
    uint32_t rtp_start_timestamp_;
    typedef std::map<uint32_t, uint32_t> FrameCaptureTimeList;
    FrameCaptureTimeList capture_time_list_ GUARDED_BY(&crit_);
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

void CallPerfTest::TestCpuOveruse(LoadObserver::Load tested_load,
                                  int encode_delay_ms) {
  class LoadObserver : public test::SendTest, public webrtc::LoadObserver {
   public:
    LoadObserver(LoadObserver::Load tested_load, int encode_delay_ms)
        : SendTest(kLongTimeoutMs),
          tested_load_(tested_load),
          encoder_(Clock::GetRealTimeClock(), encode_delay_ms) {}

    void OnLoadUpdate(Load load) override {
      if (load == tested_load_)
        observation_complete_.Set();
    }

    void ModifyVideoConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) override {
      send_config->overuse_callback = this;
      send_config->encoder_settings.encoder = &encoder_;
    }

    void PerformTest() override {
      EXPECT_TRUE(Wait()) << "Timed out before receiving an overuse callback.";
    }

    LoadObserver::Load tested_load_;
    test::DelayedEncoder encoder_;
  } test(tested_load, encode_delay_ms);

  RunBaseTest(&test);
}

TEST_F(CallPerfTest, ReceivesCpuUnderuse) {
  const int kEncodeDelayMs = 2;
  TestCpuOveruse(LoadObserver::kUnderuse, kEncodeDelayMs);
}

TEST_F(CallPerfTest, ReceivesCpuOveruse) {
  const int kEncodeDelayMs = 35;
  TestCpuOveruse(LoadObserver::kOveruse, kEncodeDelayMs);
}

void CallPerfTest::TestMinTransmitBitrate(bool pad_to_min_bitrate) {
  static const int kMaxEncodeBitrateKbps = 30;
  static const int kMinTransmitBitrateBps = 150000;
  static const int kMinAcceptableTransmitBitrate = 130;
  static const int kMaxAcceptableTransmitBitrate = 170;
  static const int kNumBitrateObservationsInRange = 100;
  static const int kAcceptableBitrateErrorMargin = 15;  // +- 7
  class BitrateObserver : public test::EndToEndTest {
   public:
    explicit BitrateObserver(bool using_min_transmit_bitrate)
        : EndToEndTest(kLongTimeoutMs),
          send_stream_(nullptr),
          pad_to_min_bitrate_(using_min_transmit_bitrate),
          num_bitrate_observations_in_range_(0) {}

   private:
    // TODO(holmer): Run this with a timer instead of once per packet.
    Action OnSendRtp(const uint8_t* packet, size_t length) override {
      VideoSendStream::Stats stats = send_stream_->GetStats();
      if (stats.substreams.size() > 0) {
        RTC_DCHECK_EQ(1u, stats.substreams.size());
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
            if (bitrate_kbps > (kMaxEncodeBitrateKbps -
                                kAcceptableBitrateErrorMargin / 2) &&
                bitrate_kbps < (kMaxEncodeBitrateKbps +
                                kAcceptableBitrateErrorMargin / 2)) {
              ++num_bitrate_observations_in_range_;
            }
          }
          if (num_bitrate_observations_in_range_ ==
              kNumBitrateObservationsInRange)
            observation_complete_.Set();
        }
      }
      return SEND_PACKET;
    }

    void OnVideoStreamsCreated(
        VideoSendStream* send_stream,
        const std::vector<VideoReceiveStream*>& receive_streams) override {
      send_stream_ = send_stream;
    }

    void ModifyVideoConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) override {
      if (pad_to_min_bitrate_) {
        encoder_config->min_transmit_bitrate_bps = kMinTransmitBitrateBps;
      } else {
        RTC_DCHECK_EQ(0, encoder_config->min_transmit_bitrate_bps);
      }
    }

    void PerformTest() override {
      EXPECT_TRUE(Wait()) << "Timeout while waiting for send-bitrate stats.";
    }

    VideoSendStream* send_stream_;
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
          time_to_reconfigure_(false, false),
          encoder_inits_(0),
          last_set_bitrate_(0),
          send_stream_(nullptr) {}

    int32_t InitEncode(const VideoCodec* config,
                       int32_t number_of_cores,
                       size_t max_payload_size) override {
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
        observation_complete_.Set();
      }
      return FakeEncoder::InitEncode(config, number_of_cores, max_payload_size);
    }

    int32_t SetRates(uint32_t new_target_bitrate_kbps,
                     uint32_t framerate) override {
      last_set_bitrate_ = new_target_bitrate_kbps;
      if (encoder_inits_ == 1 &&
          new_target_bitrate_kbps > kReconfigureThresholdKbps) {
        time_to_reconfigure_.Set();
      }
      return FakeEncoder::SetRates(new_target_bitrate_kbps, framerate);
    }

    Call::Config GetSenderCallConfig() override {
      Call::Config config = EndToEndTest::GetSenderCallConfig();
      config.bitrate_config.start_bitrate_bps = kInitialBitrateKbps * 1000;
      return config;
    }

    void ModifyVideoConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) override {
      send_config->encoder_settings.encoder = this;
      encoder_config->streams[0].min_bitrate_bps = 50000;
      encoder_config->streams[0].target_bitrate_bps =
          encoder_config->streams[0].max_bitrate_bps = 2000000;

      encoder_config_ = *encoder_config;
    }

    void OnVideoStreamsCreated(
        VideoSendStream* send_stream,
        const std::vector<VideoReceiveStream*>& receive_streams) override {
      send_stream_ = send_stream;
    }

    void PerformTest() override {
      ASSERT_TRUE(time_to_reconfigure_.Wait(kDefaultTimeoutMs))
          << "Timed out before receiving an initial high bitrate.";
      encoder_config_.streams[0].width *= 2;
      encoder_config_.streams[0].height *= 2;
      EXPECT_TRUE(send_stream_->ReconfigureVideoEncoder(encoder_config_));
      EXPECT_TRUE(Wait())
          << "Timed out while waiting for a couple of high bitrate estimates "
             "after reconfiguring the send stream.";
    }

   private:
    rtc::Event time_to_reconfigure_;
    int encoder_inits_;
    uint32_t last_set_bitrate_;
    VideoSendStream* send_stream_;
    VideoEncoderConfig encoder_config_;
  } test;

  RunBaseTest(&test);
}

}  // namespace webrtc
