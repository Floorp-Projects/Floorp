/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_AUDIO_NETWORK_ADAPTOR_IMPL_H_
#define MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_AUDIO_NETWORK_ADAPTOR_IMPL_H_

#include <memory>

#include "modules/audio_coding/audio_network_adaptor/controller.h"
#include "modules/audio_coding/audio_network_adaptor/controller_manager.h"
#include "modules/audio_coding/audio_network_adaptor/debug_dump_writer.h"
#include "modules/audio_coding/audio_network_adaptor/event_log_writer.h"
#include "modules/audio_coding/audio_network_adaptor/include/audio_network_adaptor.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {

class RtcEventLog;

class AudioNetworkAdaptorImpl final : public AudioNetworkAdaptor {
 public:
  struct Config {
    Config();
    ~Config();
    RtcEventLog* event_log;
  };

  AudioNetworkAdaptorImpl(
      const Config& config,
      std::unique_ptr<ControllerManager> controller_manager,
      std::unique_ptr<DebugDumpWriter> debug_dump_writer = nullptr);

  ~AudioNetworkAdaptorImpl() override;

  void SetUplinkBandwidth(int uplink_bandwidth_bps) override;

  void SetUplinkPacketLossFraction(float uplink_packet_loss_fraction) override;

  void SetUplinkRecoverablePacketLossFraction(
      float uplink_recoverable_packet_loss_fraction) override;

  void SetRtt(int rtt_ms) override;

  void SetTargetAudioBitrate(int target_audio_bitrate_bps) override;

  void SetOverhead(size_t overhead_bytes_per_packet) override;

  AudioEncoderRuntimeConfig GetEncoderRuntimeConfig() override;

  void StartDebugDump(FILE* file_handle) override;

  void StopDebugDump() override;

  ANAStats GetStats() const override;

 private:
  void DumpNetworkMetrics();

  void UpdateNetworkMetrics(const Controller::NetworkMetrics& network_metrics);

  const Config config_;

  std::unique_ptr<ControllerManager> controller_manager_;

  std::unique_ptr<DebugDumpWriter> debug_dump_writer_;

  const std::unique_ptr<EventLogWriter> event_log_writer_;

  Controller::NetworkMetrics last_metrics_;

  rtc::Optional<AudioEncoderRuntimeConfig> prev_config_;

  ANAStats stats_;

  const bool enable_bitrate_adaptation_;
  const bool enable_dtx_adaptation_;
  const bool enable_fec_adaptation_;
  const bool enable_channel_adaptation_;
  const bool enable_frame_length_adaptation_;

  RTC_DISALLOW_COPY_AND_ASSIGN(AudioNetworkAdaptorImpl);
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_AUDIO_NETWORK_ADAPTOR_IMPL_H_
