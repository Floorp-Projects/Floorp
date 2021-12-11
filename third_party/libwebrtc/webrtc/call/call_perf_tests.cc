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
#include <limits>
#include <memory>
#include <string>

#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "call/call.h"
#include "call/video_config.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "modules/audio_coding/include/audio_coding_module.h"
#include "modules/audio_mixer/audio_mixer_impl.h"
#include "modules/rtp_rtcp/include/rtp_header_parser.h"
#include "rtc_base/bitrateallocationstrategy.h"
#include "rtc_base/checks.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/thread_annotations.h"
#include "system_wrappers/include/metrics_default.h"
#include "test/call_test.h"
#include "test/direct_transport.h"
#include "test/drifting_clock.h"
#include "test/encoder_settings.h"
#include "test/fake_audio_device.h"
#include "test/fake_encoder.h"
#include "test/field_trial.h"
#include "test/frame_generator.h"
#include "test/frame_generator_capturer.h"
#include "test/gtest.h"
#include "test/rtp_rtcp_observer.h"
#include "test/single_threaded_task_queue.h"
#include "test/testsupport/fileutils.h"
#include "test/testsupport/perf_test.h"
#include "video/transport_adapter.h"
#include "voice_engine/include/voe_base.h"

using webrtc::test::DriftingClock;
using webrtc::test::FakeAudioDevice;

namespace webrtc {

class CallPerfTest : public test::CallTest {
 protected:
  enum class FecMode {
    kOn, kOff
  };
  enum class CreateOrder {
    kAudioFirst, kVideoFirst
  };
  void TestAudioVideoSync(FecMode fec,
                          CreateOrder create_first,
                          float video_ntp_speed,
                          float video_rtp_speed,
                          float audio_rtp_speed);

  void TestMinTransmitBitrate(bool pad_to_min_bitrate);

  void TestCaptureNtpTime(const FakeNetworkPipe::Config& net_config,
                          int threshold_ms,
                          int start_time_ms,
                          int run_time_ms);
  void TestMinAudioVideoBitrate(bool use_bitrate_allocation_strategy,
                                int test_bitrate_from,
                                int test_bitrate_to,
                                int test_bitrate_step,
                                int min_bwe,
                                int start_bwe,
                                int max_bwe);
};

class VideoRtcpAndSyncObserver : public test::RtpRtcpObserver,
                                 public rtc::VideoSinkInterface<VideoFrame> {
  static const int kInSyncThresholdMs = 50;
  static const int kStartupTimeMs = 2000;
  static const int kMinRunTimeMs = 30000;

 public:
  explicit VideoRtcpAndSyncObserver(Clock* clock)
      : test::RtpRtcpObserver(CallPerfTest::kLongTimeoutMs),
        clock_(clock),
        creation_time_ms_(clock_->TimeInMilliseconds()),
        first_time_in_sync_(-1),
        receive_stream_(nullptr) {}

  void OnFrame(const VideoFrame& video_frame) override {
    VideoReceiveStream::Stats stats;
    {
      rtc::CritScope lock(&crit_);
      if (receive_stream_)
        stats = receive_stream_->GetStats();
    }
    if (stats.sync_offset_ms == std::numeric_limits<int>::max())
      return;

    int64_t now_ms = clock_->TimeInMilliseconds();
    int64_t time_since_creation = now_ms - creation_time_ms_;
    // During the first couple of seconds audio and video can falsely be
    // estimated as being synchronized. We don't want to trigger on those.
    if (time_since_creation < kStartupTimeMs)
      return;
    if (std::abs(stats.sync_offset_ms) < kInSyncThresholdMs) {
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
    if (first_time_in_sync_ != -1)
      sync_offset_ms_list_.push_back(stats.sync_offset_ms);
  }

  void set_receive_stream(VideoReceiveStream* receive_stream) {
    rtc::CritScope lock(&crit_);
    receive_stream_ = receive_stream;
  }

  void PrintResults() {
    test::PrintResultList("stream_offset", "", "synchronization",
                          sync_offset_ms_list_, "ms", false);
  }

 private:
  Clock* const clock_;
  const int64_t creation_time_ms_;
  int64_t first_time_in_sync_;
  rtc::CriticalSection crit_;
  VideoReceiveStream* receive_stream_ RTC_GUARDED_BY(crit_);
  std::vector<double> sync_offset_ms_list_;
};

void CallPerfTest::TestAudioVideoSync(FecMode fec,
                                      CreateOrder create_first,
                                      float video_ntp_speed,
                                      float video_rtp_speed,
                                      float audio_rtp_speed) {
  const char* kSyncGroup = "av_sync";
  const uint32_t kAudioSendSsrc = 1234;
  const uint32_t kAudioRecvSsrc = 5678;

  int send_channel_id;
  int recv_channel_id;

  FakeNetworkPipe::Config audio_net_config;
  audio_net_config.queue_delay_ms = 500;
  audio_net_config.loss_percent = 5;

  rtc::scoped_refptr<AudioProcessing> audio_processing;
  VoiceEngine* voice_engine;
  VoEBase* voe_base;
  std::unique_ptr<FakeAudioDevice> fake_audio_device;
  VideoRtcpAndSyncObserver observer(Clock::GetRealTimeClock());

  std::map<uint8_t, MediaType> audio_pt_map;
  std::map<uint8_t, MediaType> video_pt_map;

  std::unique_ptr<test::PacketTransport> audio_send_transport;
  std::unique_ptr<test::PacketTransport> video_send_transport;
  std::unique_ptr<test::PacketTransport> receive_transport;

  AudioSendStream* audio_send_stream;
  AudioReceiveStream* audio_receive_stream;
  std::unique_ptr<DriftingClock> drifting_clock;

  task_queue_.SendTask([&]() {
    metrics::Reset();
    audio_processing = AudioProcessing::Create();
    voice_engine = VoiceEngine::Create();
    voe_base = VoEBase::GetInterface(voice_engine);
    fake_audio_device = rtc::MakeUnique<FakeAudioDevice>(
        FakeAudioDevice::CreatePulsedNoiseCapturer(256, 48000),
        FakeAudioDevice::CreateDiscardRenderer(48000), audio_rtp_speed);
    EXPECT_EQ(0, fake_audio_device->Init());
    EXPECT_EQ(0, voe_base->Init(fake_audio_device.get(), audio_processing.get(),
                                decoder_factory_));
    VoEBase::ChannelConfig config;
    config.enable_voice_pacing = true;
    send_channel_id = voe_base->CreateChannel(config);
    recv_channel_id = voe_base->CreateChannel();

    AudioState::Config send_audio_state_config;
    send_audio_state_config.voice_engine = voice_engine;
    send_audio_state_config.audio_mixer = AudioMixerImpl::Create();
    send_audio_state_config.audio_processing = audio_processing;
    Call::Config sender_config(event_log_.get());

    auto audio_state = AudioState::Create(send_audio_state_config);
    fake_audio_device->RegisterAudioCallback(audio_state->audio_transport());
    sender_config.audio_state = audio_state;
    Call::Config receiver_config(event_log_.get());
    receiver_config.audio_state = audio_state;
    CreateCalls(sender_config, receiver_config);

    std::copy_if(std::begin(payload_type_map_), std::end(payload_type_map_),
                 std::inserter(audio_pt_map, audio_pt_map.end()),
                 [](const std::pair<const uint8_t, MediaType>& pair) {
                   return pair.second == MediaType::AUDIO;
                 });
    std::copy_if(std::begin(payload_type_map_), std::end(payload_type_map_),
                 std::inserter(video_pt_map, video_pt_map.end()),
                 [](const std::pair<const uint8_t, MediaType>& pair) {
                   return pair.second == MediaType::VIDEO;
                 });

    audio_send_transport = rtc::MakeUnique<test::PacketTransport>(
        &task_queue_, sender_call_.get(), &observer,
        test::PacketTransport::kSender, audio_pt_map, audio_net_config);
    audio_send_transport->SetReceiver(receiver_call_->Receiver());

    video_send_transport = rtc::MakeUnique<test::PacketTransport>(
        &task_queue_, sender_call_.get(), &observer,
        test::PacketTransport::kSender, video_pt_map,
        FakeNetworkPipe::Config());
    video_send_transport->SetReceiver(receiver_call_->Receiver());

    receive_transport = rtc::MakeUnique<test::PacketTransport>(
        &task_queue_, receiver_call_.get(), &observer,
        test::PacketTransport::kReceiver, payload_type_map_,
        FakeNetworkPipe::Config());
    receive_transport->SetReceiver(sender_call_->Receiver());

    CreateSendConfig(1, 0, 0, video_send_transport.get());
    CreateMatchingReceiveConfigs(receive_transport.get());

    AudioSendStream::Config audio_send_config(audio_send_transport.get());
    audio_send_config.voe_channel_id = send_channel_id;
    audio_send_config.rtp.ssrc = kAudioSendSsrc;
    audio_send_config.send_codec_spec =
        rtc::Optional<AudioSendStream::Config::SendCodecSpec>(
            {kAudioSendPayloadType, {"ISAC", 16000, 1}});
    audio_send_config.encoder_factory = CreateBuiltinAudioEncoderFactory();
    audio_send_stream = sender_call_->CreateAudioSendStream(audio_send_config);

    video_send_config_.rtp.nack.rtp_history_ms = kNackRtpHistoryMs;
    if (fec == FecMode::kOn) {
      video_send_config_.rtp.ulpfec.red_payload_type = kRedPayloadType;
      video_send_config_.rtp.ulpfec.ulpfec_payload_type = kUlpfecPayloadType;
      video_receive_configs_[0].rtp.red_payload_type = kRedPayloadType;
      video_receive_configs_[0].rtp.ulpfec_payload_type = kUlpfecPayloadType;
    }
    video_receive_configs_[0].rtp.nack.rtp_history_ms = 1000;
    video_receive_configs_[0].renderer = &observer;
    video_receive_configs_[0].sync_group = kSyncGroup;

    AudioReceiveStream::Config audio_recv_config;
    audio_recv_config.rtp.remote_ssrc = kAudioSendSsrc;
    audio_recv_config.rtp.local_ssrc = kAudioRecvSsrc;
    audio_recv_config.voe_channel_id = recv_channel_id;
    audio_recv_config.sync_group = kSyncGroup;
    audio_recv_config.decoder_factory = decoder_factory_;
    audio_recv_config.decoder_map = {
        {kAudioSendPayloadType, {"ISAC", 16000, 1}}};

    if (create_first == CreateOrder::kAudioFirst) {
      audio_receive_stream =
          receiver_call_->CreateAudioReceiveStream(audio_recv_config);
      CreateVideoStreams();
    } else {
      CreateVideoStreams();
      audio_receive_stream =
          receiver_call_->CreateAudioReceiveStream(audio_recv_config);
    }
    EXPECT_EQ(1u, video_receive_streams_.size());
    observer.set_receive_stream(video_receive_streams_[0]);
    drifting_clock = rtc::MakeUnique<DriftingClock>(clock_, video_ntp_speed);
    CreateFrameGeneratorCapturerWithDrift(drifting_clock.get(), video_rtp_speed,
                                          kDefaultFramerate, kDefaultWidth,
                                          kDefaultHeight);

    Start();

    audio_send_stream->Start();
    audio_receive_stream->Start();
  });

  EXPECT_TRUE(observer.Wait())
      << "Timed out while waiting for audio and video to be synchronized.";

  task_queue_.SendTask([&]() {
    audio_send_stream->Stop();
    audio_receive_stream->Stop();

    Stop();

    DestroyStreams();

    video_send_transport.reset();
    audio_send_transport.reset();
    receive_transport.reset();

    sender_call_->DestroyAudioSendStream(audio_send_stream);
    receiver_call_->DestroyAudioReceiveStream(audio_receive_stream);

    voe_base->DeleteChannel(send_channel_id);
    voe_base->DeleteChannel(recv_channel_id);
    voe_base->Release();

    DestroyCalls();

    VoiceEngine::Delete(voice_engine);

    fake_audio_device.reset();
  });

  observer.PrintResults();

  // In quick test synchronization may not be achieved in time.
  if (!field_trial::IsEnabled("WebRTC-QuickPerfTest")) {
    EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.AVSyncOffsetInMs"));
  }
}

TEST_F(CallPerfTest, PlaysOutAudioAndVideoInSyncWithVideoNtpDrift) {
  TestAudioVideoSync(FecMode::kOff, CreateOrder::kAudioFirst,
                     DriftingClock::PercentsFaster(10.0f),
                     DriftingClock::kNoDrift, DriftingClock::kNoDrift);
}

TEST_F(CallPerfTest, PlaysOutAudioAndVideoInSyncWithAudioFasterThanVideoDrift) {
  TestAudioVideoSync(FecMode::kOff, CreateOrder::kAudioFirst,
                     DriftingClock::kNoDrift,
                     DriftingClock::PercentsSlower(30.0f),
                     DriftingClock::PercentsFaster(30.0f));
}

TEST_F(CallPerfTest, PlaysOutAudioAndVideoInSyncWithVideoFasterThanAudioDrift) {
  TestAudioVideoSync(FecMode::kOn, CreateOrder::kVideoFirst,
                     DriftingClock::kNoDrift,
                     DriftingClock::PercentsFaster(30.0f),
                     DriftingClock::PercentsSlower(30.0f));
}

void CallPerfTest::TestCaptureNtpTime(const FakeNetworkPipe::Config& net_config,
                                      int threshold_ms,
                                      int start_time_ms,
                                      int run_time_ms) {
  class CaptureNtpTimeObserver : public test::EndToEndTest,
                                 public rtc::VideoSinkInterface<VideoFrame> {
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
    test::PacketTransport* CreateSendTransport(
        test::SingleThreadedTaskQueueForTesting* task_queue,
        Call* sender_call) override {
      return new test::PacketTransport(task_queue, sender_call, this,
                                       test::PacketTransport::kSender,
                                       payload_type_map_, net_config_);
    }

    test::PacketTransport* CreateReceiveTransport(
        test::SingleThreadedTaskQueueForTesting* task_queue) override {
      return new test::PacketTransport(task_queue, nullptr, this,
                                       test::PacketTransport::kReceiver,
                                       payload_type_map_, net_config_);
    }

    void OnFrame(const VideoFrame& video_frame) override {
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
      time_offset_ms_list_.push_back(time_offset_ms);

      EXPECT_TRUE(std::abs(time_offset_ms) < threshold_ms_);
    }

    Action OnSendRtp(const uint8_t* packet, size_t length) override {
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
      test::PrintResultList("capture_ntp_time", "", "real - estimated",
                            time_offset_ms_list_, "ms", true);
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
    FrameCaptureTimeList capture_time_list_ RTC_GUARDED_BY(&crit_);
    std::vector<double> time_offset_ms_list_;
  } test(net_config, threshold_ms, start_time_ms, run_time_ms);

  RunBaseTest(&test);
}

// Flaky tests, disabled on Mac due to webrtc:8291.
#if !(defined(WEBRTC_MAC))
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
#endif

TEST_F(CallPerfTest, ReceivesCpuOveruseAndUnderuse) {
  // Minimal normal usage at the start, then 30s overuse to allow filter to
  // settle, and then 80s underuse to allow plenty of time for rampup again.
  test::ScopedFieldTrials fake_overuse_settings(
      "WebRTC-ForceSimulatedOveruseIntervalMs/1-30000-80000/");

  class LoadObserver : public test::SendTest,
                       public test::FrameGeneratorCapturer::SinkWantsObserver {
   public:
    LoadObserver() : SendTest(kLongTimeoutMs), test_phase_(TestPhase::kStart) {}

    void OnFrameGeneratorCapturerCreated(
        test::FrameGeneratorCapturer* frame_generator_capturer) override {
      frame_generator_capturer->SetSinkWantsObserver(this);
      // Set a high initial resolution to be sure that we can scale down.
      frame_generator_capturer->ChangeResolution(1920, 1080);
    }

    // OnSinkWantsChanged is called when FrameGeneratorCapturer::AddOrUpdateSink
    // is called.
    // TODO(sprang): Add integration test for maintain-framerate mode?
    void OnSinkWantsChanged(rtc::VideoSinkInterface<VideoFrame>* sink,
                            const rtc::VideoSinkWants& wants) override {
      // First expect CPU overuse. Then expect CPU underuse when the encoder
      // delay has been decreased.
      switch (test_phase_) {
        case TestPhase::kStart:
          if (wants.max_pixel_count < std::numeric_limits<int>::max()) {
            // On adapting down, VideoStreamEncoder::VideoSourceProxy will set
            // only the max pixel count, leaving the target unset.
            test_phase_ = TestPhase::kAdaptedDown;
          } else {
            ADD_FAILURE() << "Got unexpected adaptation request, max res = "
                          << wants.max_pixel_count << ", target res = "
                          << wants.target_pixel_count.value_or(-1)
                          << ", max fps = " << wants.max_framerate_fps;
          }
          break;
        case TestPhase::kAdaptedDown:
          // On adapting up, the adaptation counter will again be at zero, and
          // so all constraints will be reset.
          if (wants.max_pixel_count == std::numeric_limits<int>::max() &&
              !wants.target_pixel_count) {
            test_phase_ = TestPhase::kAdaptedUp;
            observation_complete_.Set();
          } else {
            ADD_FAILURE() << "Got unexpected adaptation request, max res = "
                          << wants.max_pixel_count << ", target res = "
                          << wants.target_pixel_count.value_or(-1)
                          << ", max fps = " << wants.max_framerate_fps;
          }
          break;
        case TestPhase::kAdaptedUp:
          ADD_FAILURE() << "Got unexpected adaptation request, max res = "
                        << wants.max_pixel_count << ", target res = "
                        << wants.target_pixel_count.value_or(-1)
                        << ", max fps = " << wants.max_framerate_fps;
      }
    }

    void ModifyVideoConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) override {
    }

    void PerformTest() override {
      EXPECT_TRUE(Wait()) << "Timed out before receiving an overuse callback.";
    }

    enum class TestPhase { kStart, kAdaptedDown, kAdaptedUp } test_phase_;
  } test;

  RunBaseTest(&test);
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
          converged_(false),
          pad_to_min_bitrate_(using_min_transmit_bitrate),
          min_acceptable_bitrate_(using_min_transmit_bitrate
                                      ? kMinAcceptableTransmitBitrate
                                      : (kMaxEncodeBitrateKbps -
                                         kAcceptableBitrateErrorMargin / 2)),
          max_acceptable_bitrate_(using_min_transmit_bitrate
                                      ? kMaxAcceptableTransmitBitrate
                                      : (kMaxEncodeBitrateKbps +
                                         kAcceptableBitrateErrorMargin / 2)),
          num_bitrate_observations_in_range_(0) {}

   private:
    // TODO(holmer): Run this with a timer instead of once per packet.
    Action OnSendRtp(const uint8_t* packet, size_t length) override {
      VideoSendStream::Stats stats = send_stream_->GetStats();
      if (stats.substreams.size() > 0) {
        RTC_DCHECK_EQ(1, stats.substreams.size());
        int bitrate_kbps =
            stats.substreams.begin()->second.total_bitrate_bps / 1000;
        if (bitrate_kbps > min_acceptable_bitrate_ &&
            bitrate_kbps < max_acceptable_bitrate_) {
          converged_ = true;
          ++num_bitrate_observations_in_range_;
          if (num_bitrate_observations_in_range_ ==
              kNumBitrateObservationsInRange)
            observation_complete_.Set();
        }
        if (converged_)
          bitrate_kbps_list_.push_back(bitrate_kbps);
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
      test::PrintResultList(
          "bitrate_stats_",
          (pad_to_min_bitrate_ ? "min_transmit_bitrate"
                               : "without_min_transmit_bitrate"),
          "bitrate_kbps", bitrate_kbps_list_, "kbps", false);
    }

    VideoSendStream* send_stream_;
    bool converged_;
    const bool pad_to_min_bitrate_;
    const int min_acceptable_bitrate_;
    const int max_acceptable_bitrate_;
    int num_bitrate_observations_in_range_;
    std::vector<double> bitrate_kbps_list_;
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

  class VideoStreamFactory
      : public VideoEncoderConfig::VideoStreamFactoryInterface {
   public:
    VideoStreamFactory() {}

   private:
    std::vector<VideoStream> CreateEncoderStreams(
        int width,
        int height,
        const VideoEncoderConfig& encoder_config) override {
      std::vector<VideoStream> streams =
          test::CreateVideoStreams(width, height, encoder_config);
      streams[0].min_bitrate_bps = 50000;
      streams[0].target_bitrate_bps = streams[0].max_bitrate_bps = 2000000;
      return streams;
    }
  };

  class BitrateObserver : public test::EndToEndTest, public test::FakeEncoder {
   public:
    BitrateObserver()
        : EndToEndTest(kDefaultTimeoutMs),
          FakeEncoder(Clock::GetRealTimeClock()),
          time_to_reconfigure_(false, false),
          encoder_inits_(0),
          last_set_bitrate_kbps_(0),
          send_stream_(nullptr),
          frame_generator_(nullptr) {}

    int32_t InitEncode(const VideoCodec* config,
                       int32_t number_of_cores,
                       size_t max_payload_size) override {
      ++encoder_inits_;
      if (encoder_inits_ == 1) {
        // First time initialization. Frame size is known.
        // |expected_bitrate| is affected by bandwidth estimation before the
        // first frame arrives to the encoder.
        uint32_t expected_bitrate = last_set_bitrate_kbps_ > 0
                                        ? last_set_bitrate_kbps_
                                        : kInitialBitrateKbps;
        EXPECT_EQ(expected_bitrate, config->startBitrate)
            << "Encoder not initialized at expected bitrate.";
        EXPECT_EQ(kDefaultWidth, config->width);
        EXPECT_EQ(kDefaultHeight, config->height);
      } else if (encoder_inits_ == 2) {
        EXPECT_EQ(2 * kDefaultWidth, config->width);
        EXPECT_EQ(2 * kDefaultHeight, config->height);
        EXPECT_GE(last_set_bitrate_kbps_, kReconfigureThresholdKbps);
        EXPECT_GT(
            config->startBitrate,
            last_set_bitrate_kbps_ - kPermittedReconfiguredBitrateDiffKbps)
            << "Encoder reconfigured with bitrate too far away from last set.";
        observation_complete_.Set();
      }
      return FakeEncoder::InitEncode(config, number_of_cores, max_payload_size);
    }

    int32_t SetRateAllocation(const BitrateAllocation& rate_allocation,
                              uint32_t framerate) override {
      last_set_bitrate_kbps_ = rate_allocation.get_sum_kbps();
      if (encoder_inits_ == 1 &&
          rate_allocation.get_sum_kbps() > kReconfigureThresholdKbps) {
        time_to_reconfigure_.Set();
      }
      return FakeEncoder::SetRateAllocation(rate_allocation, framerate);
    }

    Call::Config GetSenderCallConfig() override {
      Call::Config config = EndToEndTest::GetSenderCallConfig();
      config.event_log = event_log_.get();
      config.bitrate_config.start_bitrate_bps = kInitialBitrateKbps * 1000;
      return config;
    }

    void ModifyVideoConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) override {
      send_config->encoder_settings.encoder = this;
      encoder_config->max_bitrate_bps = 2 * kReconfigureThresholdKbps * 1000;
      encoder_config->video_stream_factory =
          new rtc::RefCountedObject<VideoStreamFactory>();

      encoder_config_ = encoder_config->Copy();
    }

    void OnVideoStreamsCreated(
        VideoSendStream* send_stream,
        const std::vector<VideoReceiveStream*>& receive_streams) override {
      send_stream_ = send_stream;
    }

    void OnFrameGeneratorCapturerCreated(
        test::FrameGeneratorCapturer* frame_generator_capturer) override {
      frame_generator_ = frame_generator_capturer;
    }

    void PerformTest() override {
      ASSERT_TRUE(time_to_reconfigure_.Wait(kDefaultTimeoutMs))
          << "Timed out before receiving an initial high bitrate.";
      frame_generator_->ChangeResolution(kDefaultWidth * 2, kDefaultHeight * 2);
      send_stream_->ReconfigureVideoEncoder(encoder_config_.Copy());
      EXPECT_TRUE(Wait())
          << "Timed out while waiting for a couple of high bitrate estimates "
             "after reconfiguring the send stream.";
    }

   private:
    rtc::Event time_to_reconfigure_;
    int encoder_inits_;
    uint32_t last_set_bitrate_kbps_;
    VideoSendStream* send_stream_;
    test::FrameGeneratorCapturer* frame_generator_;
    VideoEncoderConfig encoder_config_;
  } test;

  RunBaseTest(&test);
}

// Discovers the minimal supported audio+video bitrate. The test bitrate is
// considered supported if Rtt does not go above 400ms with the network
// contrained to the test bitrate.
//
// |use_bitrate_allocation_strategy| use AudioPriorityBitrateAllocationStrategy
// |test_bitrate_from test_bitrate_to| bitrate constraint range
// |test_bitrate_step| bitrate constraint update step during the test
// |min_bwe max_bwe| BWE range
// |start_bwe| initial BWE
void CallPerfTest::TestMinAudioVideoBitrate(
    bool use_bitrate_allocation_strategy,
    int test_bitrate_from,
    int test_bitrate_to,
    int test_bitrate_step,
    int min_bwe,
    int start_bwe,
    int max_bwe) {
  static const std::string kAudioTrackId = "audio_track_0";
  static constexpr uint32_t kSufficientAudioBitrateBps = 16000;
  static constexpr int kOpusMinBitrateBps = 6000;
  static constexpr int kOpusBitrateFbBps = 32000;
  static constexpr int kBitrateStabilizationMs = 10000;
  static constexpr int kBitrateMeasurements = 10;
  static constexpr int kBitrateMeasurementMs = 1000;
  static constexpr int kMinGoodRttMs = 400;

  class MinVideoAndAudioBitrateTester : public test::EndToEndTest {
   public:
    MinVideoAndAudioBitrateTester(bool use_bitrate_allocation_strategy,
                                  int test_bitrate_from,
                                  int test_bitrate_to,
                                  int test_bitrate_step,
                                  int min_bwe,
                                  int start_bwe,
                                  int max_bwe)
        : EndToEndTest(),
          allocation_strategy_(new rtc::AudioPriorityBitrateAllocationStrategy(
              kAudioTrackId,
              kSufficientAudioBitrateBps)),
          use_bitrate_allocation_strategy_(use_bitrate_allocation_strategy),
          test_bitrate_from_(test_bitrate_from),
          test_bitrate_to_(test_bitrate_to),
          test_bitrate_step_(test_bitrate_step),
          min_bwe_(min_bwe),
          start_bwe_(start_bwe),
          max_bwe_(max_bwe) {}

   protected:
    FakeNetworkPipe::Config GetFakeNetworkPipeConfig() {
      FakeNetworkPipe::Config pipe_config;
      pipe_config.link_capacity_kbps = test_bitrate_from_;
      return pipe_config;
    }

    test::PacketTransport* CreateSendTransport(
        test::SingleThreadedTaskQueueForTesting* task_queue,
        Call* sender_call) override {
      return send_transport_ = new test::PacketTransport(
                 task_queue, sender_call, this, test::PacketTransport::kSender,
                 test::CallTest::payload_type_map_, GetFakeNetworkPipeConfig());
    }

    test::PacketTransport* CreateReceiveTransport(
        test::SingleThreadedTaskQueueForTesting* task_queue) override {
      return receive_transport_ = new test::PacketTransport(
                 task_queue, nullptr, this, test::PacketTransport::kReceiver,
                 test::CallTest::payload_type_map_, GetFakeNetworkPipeConfig());
    }

    void PerformTest() override {
      int last_passed_test_bitrate = -1;
      for (int test_bitrate = test_bitrate_from_;
           test_bitrate_from_ < test_bitrate_to_
               ? test_bitrate <= test_bitrate_to_
               : test_bitrate >= test_bitrate_to_;
           test_bitrate += test_bitrate_step_) {
        FakeNetworkPipe::Config pipe_config;
        pipe_config.link_capacity_kbps = test_bitrate;
        send_transport_->SetConfig(pipe_config);
        receive_transport_->SetConfig(pipe_config);

        rtc::ThreadManager::Instance()->CurrentThread()->SleepMs(
            kBitrateStabilizationMs);

        int64_t avg_rtt = 0;
        for (int i = 0; i < kBitrateMeasurements; i++) {
          Call::Stats call_stats = sender_call_->GetStats();
          avg_rtt += call_stats.rtt_ms;
          rtc::ThreadManager::Instance()->CurrentThread()->SleepMs(
              kBitrateMeasurementMs);
        }
        avg_rtt = avg_rtt / kBitrateMeasurements;
        if (avg_rtt > kMinGoodRttMs) {
          break;
        } else {
          last_passed_test_bitrate = test_bitrate;
        }
      }
      EXPECT_GT(last_passed_test_bitrate, -1)
          << "Minimum supported bitrate out of the test scope";
      webrtc::test::PrintResult("min_test_bitrate_",
                                use_bitrate_allocation_strategy_
                                    ? "with_allocation_strategy"
                                    : "no_allocation_strategy",
                                "", last_passed_test_bitrate, "kbps", false);
    }

    void OnCallsCreated(Call* sender_call, Call* receiver_call) override {
      sender_call_ = sender_call;
      Call::Config::BitrateConfig bitrate_config;
      bitrate_config.min_bitrate_bps = min_bwe_;
      bitrate_config.start_bitrate_bps = start_bwe_;
      bitrate_config.max_bitrate_bps = max_bwe_;
      sender_call->SetBitrateConfig(bitrate_config);
      if (use_bitrate_allocation_strategy_) {
        sender_call->SetBitrateAllocationStrategy(
            std::move(allocation_strategy_));
      }
    }

    size_t GetNumVideoStreams() const override { return 1; }

    size_t GetNumAudioStreams() const override { return 1; }

    void ModifyAudioConfigs(
        AudioSendStream::Config* send_config,
        std::vector<AudioReceiveStream::Config>* receive_configs) override {
      if (use_bitrate_allocation_strategy_) {
        send_config->track_id = kAudioTrackId;
        send_config->min_bitrate_bps = kOpusMinBitrateBps;
        send_config->max_bitrate_bps = kOpusBitrateFbBps;
      } else {
        send_config->send_codec_spec->target_bitrate_bps =
            rtc::Optional<int>(kOpusBitrateFbBps);
      }
    }

   private:
    std::unique_ptr<rtc::BitrateAllocationStrategy> allocation_strategy_;
    const bool use_bitrate_allocation_strategy_;
    const int test_bitrate_from_;
    const int test_bitrate_to_;
    const int test_bitrate_step_;
    const int min_bwe_;
    const int start_bwe_;
    const int max_bwe_;
    test::PacketTransport* send_transport_;
    test::PacketTransport* receive_transport_;
    Call* sender_call_;
  } test(use_bitrate_allocation_strategy, test_bitrate_from, test_bitrate_to,
         test_bitrate_step, min_bwe, start_bwe, max_bwe);

  RunBaseTest(&test);
}

TEST_F(CallPerfTest, MinVideoAndAudioBitrate) {
  TestMinAudioVideoBitrate(false, 110, 40, -10, 10000, 70000, 200000);
}
TEST_F(CallPerfTest, MinVideoAndAudioBitrateWStrategy) {
  TestMinAudioVideoBitrate(true, 110, 40, -10, 10000, 70000, 200000);
}

}  // namespace webrtc
