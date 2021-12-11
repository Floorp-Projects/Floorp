/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_RAMPUP_TESTS_H_
#define CALL_RAMPUP_TESTS_H_

#include <map>
#include <string>
#include <vector>

#include "call/call.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "rtc_base/event.h"
#include "test/call_test.h"

namespace webrtc {

static const int kTransmissionTimeOffsetExtensionId = 6;
static const int kAbsSendTimeExtensionId = 7;
static const int kTransportSequenceNumberExtensionId = 8;
static const unsigned int kSingleStreamTargetBps = 1000000;

class Clock;

class RampUpTester : public test::EndToEndTest {
 public:
  RampUpTester(size_t num_video_streams,
               size_t num_audio_streams,
               size_t num_flexfec_streams,
               unsigned int start_bitrate_bps,
               int64_t min_run_time_ms,
               const std::string& extension_type,
               bool rtx,
               bool red,
               bool report_perf_stats);
  ~RampUpTester() override;

  size_t GetNumVideoStreams() const override;
  size_t GetNumAudioStreams() const override;
  size_t GetNumFlexfecStreams() const override;

  void PerformTest() override;

 protected:
  virtual void PollStats();

  void AccumulateStats(const VideoSendStream::StreamStats& stream,
                       size_t* total_packets_sent,
                       size_t* total_sent,
                       size_t* padding_sent,
                       size_t* media_sent) const;

  void ReportResult(const std::string& measurement,
                    size_t value,
                    const std::string& units) const;
  void TriggerTestDone();

  webrtc::RtcEventLogNullImpl event_log_;
  rtc::Event stop_event_;
  Clock* const clock_;
  FakeNetworkPipe::Config forward_transport_config_;
  const size_t num_video_streams_;
  const size_t num_audio_streams_;
  const size_t num_flexfec_streams_;
  const bool rtx_;
  const bool red_;
  const bool report_perf_stats_;
  Call* sender_call_;
  VideoSendStream* send_stream_;
  test::PacketTransport* send_transport_;

 private:
  typedef std::map<uint32_t, uint32_t> SsrcMap;
  class VideoStreamFactory;

  Call::Config GetSenderCallConfig() override;
  void OnVideoStreamsCreated(
      VideoSendStream* send_stream,
      const std::vector<VideoReceiveStream*>& receive_streams) override;
  test::PacketTransport* CreateSendTransport(
      test::SingleThreadedTaskQueueForTesting* task_queue,
      Call* sender_call) override;
  void ModifyVideoConfigs(
      VideoSendStream::Config* send_config,
      std::vector<VideoReceiveStream::Config>* receive_configs,
      VideoEncoderConfig* encoder_config) override;
  void ModifyAudioConfigs(
      AudioSendStream::Config* send_config,
      std::vector<AudioReceiveStream::Config>* receive_configs) override;
  void ModifyFlexfecConfigs(
      std::vector<FlexfecReceiveStream::Config>* receive_configs) override;
  void OnCallsCreated(Call* sender_call, Call* receiver_call) override;

  static void BitrateStatsPollingThread(void* obj);

  const int start_bitrate_bps_;
  const int64_t min_run_time_ms_;
  int expected_bitrate_bps_;
  int64_t test_start_ms_;
  int64_t ramp_up_finished_ms_;

  const std::string extension_type_;
  std::vector<uint32_t> video_ssrcs_;
  std::vector<uint32_t> video_rtx_ssrcs_;
  std::vector<uint32_t> audio_ssrcs_;

  rtc::PlatformThread poller_thread_;
};

class RampUpDownUpTester : public RampUpTester {
 public:
  RampUpDownUpTester(size_t num_video_streams,
                     size_t num_audio_streams,
                     size_t num_flexfec_streams,
                     unsigned int start_bitrate_bps,
                     const std::string& extension_type,
                     bool rtx,
                     bool red,
                     const std::vector<int>& loss_rates,
                     bool report_perf_stats);
  ~RampUpDownUpTester() override;

 protected:
  void PollStats() override;

 private:
  enum TestStates {
    kFirstRampup = 0,
    kLowRate,
    kSecondRampup,
    kTestEnd,
    kTransitionToNextState,
  };

  Call::Config GetReceiverCallConfig() override;

  std::string GetModifierString() const;
  int GetExpectedHighBitrate() const;
  int GetHighLinkCapacity() const;
  size_t GetFecBytes() const;
  bool ExpectingFec() const;
  void EvolveTestState(int bitrate_bps, bool suspended);

  const std::vector<int> link_rates_;
  TestStates test_state_;
  TestStates next_state_;
  int64_t state_start_ms_;
  int64_t interval_start_ms_;
  int sent_bytes_;
  std::vector<int> loss_rates_;
};
}  // namespace webrtc
#endif  // CALL_RAMPUP_TESTS_H_
