/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string.h>

#include <map>
#include <vector>

#include "webrtc/audio/audio_receive_stream.h"
#include "webrtc/audio/audio_send_stream.h"
#include "webrtc/audio/audio_state.h"
#include "webrtc/audio/scoped_voe_interface.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/base/thread_checker.h"
#include "webrtc/base/trace_event.h"
#include "webrtc/call.h"
#include "webrtc/call/bitrate_allocator.h"
#include "webrtc/call/congestion_controller.h"
#include "webrtc/call/rtc_event_log.h"
#include "webrtc/common.h"
#include "webrtc/config.h"
#include "webrtc/modules/bitrate_controller/include/bitrate_controller.h"
#include "webrtc/modules/pacing/paced_sender.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_header_parser.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"
#include "webrtc/modules/utility/include/process_thread.h"
#include "webrtc/system_wrappers/include/cpu_info.h"
#include "webrtc/system_wrappers/include/critical_section_wrapper.h"
#include "webrtc/system_wrappers/include/metrics.h"
#include "webrtc/system_wrappers/include/rw_lock_wrapper.h"
#include "webrtc/system_wrappers/include/trace.h"
#include "webrtc/video/call_stats.h"
#include "webrtc/video/video_receive_stream.h"
#include "webrtc/video/video_send_stream.h"
#include "webrtc/voice_engine/include/voe_codec.h"

namespace webrtc {

const int Call::Config::kDefaultStartBitrateBps = 300000;

namespace internal {

class Call : public webrtc::Call, public PacketReceiver,
             public BitrateObserver {
 public:
  explicit Call(const Call::Config& config);
  virtual ~Call();

  PacketReceiver* Receiver() override;

  webrtc::AudioSendStream* CreateAudioSendStream(
      const webrtc::AudioSendStream::Config& config) override;
  void DestroyAudioSendStream(webrtc::AudioSendStream* send_stream) override;

  webrtc::AudioReceiveStream* CreateAudioReceiveStream(
      const webrtc::AudioReceiveStream::Config& config) override;
  void DestroyAudioReceiveStream(
      webrtc::AudioReceiveStream* receive_stream) override;

  webrtc::VideoSendStream* CreateVideoSendStream(
      const webrtc::VideoSendStream::Config& config,
      const VideoEncoderConfig& encoder_config) override;
  void DestroyVideoSendStream(webrtc::VideoSendStream* send_stream) override;

  webrtc::VideoReceiveStream* CreateVideoReceiveStream(
      const webrtc::VideoReceiveStream::Config& config) override;
  void DestroyVideoReceiveStream(
      webrtc::VideoReceiveStream* receive_stream) override;

  Stats GetStats() const override;

  DeliveryStatus DeliverPacket(MediaType media_type,
                               const uint8_t* packet,
                               size_t length,
                               const PacketTime& packet_time) override;

  void SetBitrateConfig(
      const webrtc::Call::Config::BitrateConfig& bitrate_config) override;
  void SignalNetworkState(NetworkState state) override;

  void OnSentPacket(const rtc::SentPacket& sent_packet) override;

  // Implements BitrateObserver.
  void OnNetworkChanged(uint32_t bitrate_bps, uint8_t fraction_loss,
                        int64_t rtt_ms) override;

 private:
  DeliveryStatus DeliverRtcp(MediaType media_type, const uint8_t* packet,
                             size_t length);
  DeliveryStatus DeliverRtp(MediaType media_type,
                            const uint8_t* packet,
                            size_t length,
                            const PacketTime& packet_time);

  void ConfigureSync(const std::string& sync_group)
      EXCLUSIVE_LOCKS_REQUIRED(receive_crit_);

  VoiceEngine* voice_engine() {
    internal::AudioState* audio_state =
        static_cast<internal::AudioState*>(config_.audio_state.get());
    if (audio_state)
      return audio_state->voice_engine();
    else
      return nullptr;
  }

  void UpdateSendHistograms() EXCLUSIVE_LOCKS_REQUIRED(&bitrate_crit_);
  void UpdateReceiveHistograms();

  Clock* const clock_;

  const int num_cpu_cores_;
  const rtc::scoped_ptr<ProcessThread> module_process_thread_;
  const rtc::scoped_ptr<CallStats> call_stats_;
  const rtc::scoped_ptr<BitrateAllocator> bitrate_allocator_;
  Call::Config config_;
  rtc::ThreadChecker configuration_thread_checker_;

  bool network_enabled_;

  rtc::scoped_ptr<RWLockWrapper> receive_crit_;
  // Audio and Video receive streams are owned by the client that creates them.
  std::map<uint32_t, AudioReceiveStream*> audio_receive_ssrcs_
      GUARDED_BY(receive_crit_);
  std::map<uint32_t, VideoReceiveStream*> video_receive_ssrcs_
      GUARDED_BY(receive_crit_);
  std::set<VideoReceiveStream*> video_receive_streams_
      GUARDED_BY(receive_crit_);
  std::map<std::string, AudioReceiveStream*> sync_stream_mapping_
      GUARDED_BY(receive_crit_);

  rtc::scoped_ptr<RWLockWrapper> send_crit_;
  // Audio and Video send streams are owned by the client that creates them.
  std::map<uint32_t, AudioSendStream*> audio_send_ssrcs_ GUARDED_BY(send_crit_);
  std::map<uint32_t, VideoSendStream*> video_send_ssrcs_ GUARDED_BY(send_crit_);
  std::set<VideoSendStream*> video_send_streams_ GUARDED_BY(send_crit_);

  VideoSendStream::RtpStateMap suspended_video_send_ssrcs_;

  RtcEventLog* event_log_ = nullptr;

  // The following members are only accessed (exclusively) from one thread and
  // from the destructor, and therefore doesn't need any explicit
  // synchronization.
  int64_t received_video_bytes_;
  int64_t received_audio_bytes_;
  int64_t received_rtcp_bytes_;
  int64_t first_rtp_packet_received_ms_;
  int64_t last_rtp_packet_received_ms_;
  int64_t first_packet_sent_ms_;

  // TODO(holmer): Remove this lock once BitrateController no longer calls
  // OnNetworkChanged from multiple threads.
  rtc::CriticalSection bitrate_crit_;
  int64_t estimated_send_bitrate_sum_kbits_ GUARDED_BY(&bitrate_crit_);
  int64_t pacer_bitrate_sum_kbits_ GUARDED_BY(&bitrate_crit_);
  int64_t num_bitrate_updates_ GUARDED_BY(&bitrate_crit_);

  const rtc::scoped_ptr<CongestionController> congestion_controller_;

  RTC_DISALLOW_COPY_AND_ASSIGN(Call);
};
}  // namespace internal

Call* Call::Create(const Call::Config& config) {
  return new internal::Call(config);
}

namespace internal {

Call::Call(const Call::Config& config)
    : clock_(Clock::GetRealTimeClock()),
      num_cpu_cores_(CpuInfo::DetectNumberOfCores()),
      module_process_thread_(ProcessThread::Create("ModuleProcessThread")),
      call_stats_(new CallStats(clock_)),
      bitrate_allocator_(new BitrateAllocator()),
      config_(config),
      network_enabled_(true),
      receive_crit_(RWLockWrapper::CreateRWLock()),
      send_crit_(RWLockWrapper::CreateRWLock()),
      received_video_bytes_(0),
      received_audio_bytes_(0),
      received_rtcp_bytes_(0),
      first_rtp_packet_received_ms_(-1),
      last_rtp_packet_received_ms_(-1),
      first_packet_sent_ms_(-1),
      estimated_send_bitrate_sum_kbits_(0),
      pacer_bitrate_sum_kbits_(0),
      num_bitrate_updates_(0),
      congestion_controller_(
          new CongestionController(module_process_thread_.get(),
                                   call_stats_.get(),
                                   this)) {
  RTC_DCHECK(configuration_thread_checker_.CalledOnValidThread());
  RTC_DCHECK_GE(config.bitrate_config.min_bitrate_bps, 0);
  RTC_DCHECK_GE(config.bitrate_config.start_bitrate_bps,
                config.bitrate_config.min_bitrate_bps);
  if (config.bitrate_config.max_bitrate_bps != -1) {
    RTC_DCHECK_GE(config.bitrate_config.max_bitrate_bps,
                  config.bitrate_config.start_bitrate_bps);
  }
  if (config.audio_state.get()) {
    ScopedVoEInterface<VoECodec> voe_codec(voice_engine());
    event_log_ = voe_codec->GetEventLog();
  }

  Trace::CreateTrace();
  module_process_thread_->Start();
  module_process_thread_->RegisterModule(call_stats_.get());

  congestion_controller_->SetBweBitrates(
      config_.bitrate_config.min_bitrate_bps,
      config_.bitrate_config.start_bitrate_bps,
      config_.bitrate_config.max_bitrate_bps);

  congestion_controller_->GetBitrateController()->SetEventLog(event_log_);
}

Call::~Call() {
  RTC_DCHECK(configuration_thread_checker_.CalledOnValidThread());
  UpdateSendHistograms();
  UpdateReceiveHistograms();
  RTC_CHECK(audio_send_ssrcs_.empty());
  RTC_CHECK(video_send_ssrcs_.empty());
  RTC_CHECK(video_send_streams_.empty());
  RTC_CHECK(audio_receive_ssrcs_.empty());
  RTC_CHECK(video_receive_ssrcs_.empty());
  RTC_CHECK(video_receive_streams_.empty());

  module_process_thread_->DeRegisterModule(call_stats_.get());
  module_process_thread_->Stop();
  Trace::ReturnTrace();
}

void Call::UpdateSendHistograms() {
  if (num_bitrate_updates_ == 0 || first_packet_sent_ms_ == -1)
    return;
  int64_t elapsed_sec =
      (clock_->TimeInMilliseconds() - first_packet_sent_ms_) / 1000;
  if (elapsed_sec < metrics::kMinRunTimeInSeconds)
    return;
  int send_bitrate_kbps =
      estimated_send_bitrate_sum_kbits_ / num_bitrate_updates_;
  int pacer_bitrate_kbps = pacer_bitrate_sum_kbits_ / num_bitrate_updates_;
  if (send_bitrate_kbps > 0) {
    RTC_HISTOGRAM_COUNTS_SPARSE_100000("WebRTC.Call.EstimatedSendBitrateInKbps",
                                       send_bitrate_kbps);
  }
  if (pacer_bitrate_kbps > 0) {
    RTC_HISTOGRAM_COUNTS_SPARSE_100000("WebRTC.Call.PacerBitrateInKbps",
                                       pacer_bitrate_kbps);
  }
}

void Call::UpdateReceiveHistograms() {
  if (first_rtp_packet_received_ms_ == -1)
    return;
  int64_t elapsed_sec =
      (last_rtp_packet_received_ms_ - first_rtp_packet_received_ms_) / 1000;
  if (elapsed_sec < metrics::kMinRunTimeInSeconds)
    return;
  int audio_bitrate_kbps = received_audio_bytes_ * 8 / elapsed_sec / 1000;
  int video_bitrate_kbps = received_video_bytes_ * 8 / elapsed_sec / 1000;
  int rtcp_bitrate_bps = received_rtcp_bytes_ * 8 / elapsed_sec;
  if (video_bitrate_kbps > 0) {
    RTC_HISTOGRAM_COUNTS_SPARSE_100000("WebRTC.Call.VideoBitrateReceivedInKbps",
                                       video_bitrate_kbps);
  }
  if (audio_bitrate_kbps > 0) {
    RTC_HISTOGRAM_COUNTS_SPARSE_100000("WebRTC.Call.AudioBitrateReceivedInKbps",
                                       audio_bitrate_kbps);
  }
  if (rtcp_bitrate_bps > 0) {
    RTC_HISTOGRAM_COUNTS_SPARSE_100000("WebRTC.Call.RtcpBitrateReceivedInBps",
                                       rtcp_bitrate_bps);
  }
  RTC_HISTOGRAM_COUNTS_SPARSE_100000(
      "WebRTC.Call.BitrateReceivedInKbps",
      audio_bitrate_kbps + video_bitrate_kbps + rtcp_bitrate_bps / 1000);
}

PacketReceiver* Call::Receiver() {
  // TODO(solenberg): Some test cases in EndToEndTest use this from a different
  // thread. Re-enable once that is fixed.
  // RTC_DCHECK(configuration_thread_checker_.CalledOnValidThread());
  return this;
}

webrtc::AudioSendStream* Call::CreateAudioSendStream(
    const webrtc::AudioSendStream::Config& config) {
  TRACE_EVENT0("webrtc", "Call::CreateAudioSendStream");
  RTC_DCHECK(configuration_thread_checker_.CalledOnValidThread());
  AudioSendStream* send_stream = new AudioSendStream(
      config, config_.audio_state, congestion_controller_.get());
  if (!network_enabled_)
    send_stream->SignalNetworkState(kNetworkDown);
  {
    WriteLockScoped write_lock(*send_crit_);
    RTC_DCHECK(audio_send_ssrcs_.find(config.rtp.ssrc) ==
               audio_send_ssrcs_.end());
    audio_send_ssrcs_[config.rtp.ssrc] = send_stream;
  }
  return send_stream;
}

void Call::DestroyAudioSendStream(webrtc::AudioSendStream* send_stream) {
  TRACE_EVENT0("webrtc", "Call::DestroyAudioSendStream");
  RTC_DCHECK(configuration_thread_checker_.CalledOnValidThread());
  RTC_DCHECK(send_stream != nullptr);

  send_stream->Stop();

  webrtc::internal::AudioSendStream* audio_send_stream =
      static_cast<webrtc::internal::AudioSendStream*>(send_stream);
  {
    WriteLockScoped write_lock(*send_crit_);
    size_t num_deleted = audio_send_ssrcs_.erase(
        audio_send_stream->config().rtp.ssrc);
    RTC_DCHECK(num_deleted == 1);
  }
  delete audio_send_stream;
}

webrtc::AudioReceiveStream* Call::CreateAudioReceiveStream(
    const webrtc::AudioReceiveStream::Config& config) {
  TRACE_EVENT0("webrtc", "Call::CreateAudioReceiveStream");
  RTC_DCHECK(configuration_thread_checker_.CalledOnValidThread());
  AudioReceiveStream* receive_stream = new AudioReceiveStream(
      congestion_controller_.get(), config, config_.audio_state);
  {
    WriteLockScoped write_lock(*receive_crit_);
    RTC_DCHECK(audio_receive_ssrcs_.find(config.rtp.remote_ssrc) ==
               audio_receive_ssrcs_.end());
    audio_receive_ssrcs_[config.rtp.remote_ssrc] = receive_stream;
    ConfigureSync(config.sync_group);
  }
  return receive_stream;
}

void Call::DestroyAudioReceiveStream(
    webrtc::AudioReceiveStream* receive_stream) {
  TRACE_EVENT0("webrtc", "Call::DestroyAudioReceiveStream");
  RTC_DCHECK(configuration_thread_checker_.CalledOnValidThread());
  RTC_DCHECK(receive_stream != nullptr);
  webrtc::internal::AudioReceiveStream* audio_receive_stream =
      static_cast<webrtc::internal::AudioReceiveStream*>(receive_stream);
  {
    WriteLockScoped write_lock(*receive_crit_);
    size_t num_deleted = audio_receive_ssrcs_.erase(
        audio_receive_stream->config().rtp.remote_ssrc);
    RTC_DCHECK(num_deleted == 1);
    const std::string& sync_group = audio_receive_stream->config().sync_group;
    const auto it = sync_stream_mapping_.find(sync_group);
    if (it != sync_stream_mapping_.end() &&
        it->second == audio_receive_stream) {
      sync_stream_mapping_.erase(it);
      ConfigureSync(sync_group);
    }
  }
  delete audio_receive_stream;
}

webrtc::VideoSendStream* Call::CreateVideoSendStream(
    const webrtc::VideoSendStream::Config& config,
    const VideoEncoderConfig& encoder_config) {
  TRACE_EVENT0("webrtc", "Call::CreateVideoSendStream");
  RTC_DCHECK(configuration_thread_checker_.CalledOnValidThread());

  // TODO(mflodman): Base the start bitrate on a current bandwidth estimate, if
  // the call has already started.
  VideoSendStream* send_stream = new VideoSendStream(
      num_cpu_cores_, module_process_thread_.get(), call_stats_.get(),
      congestion_controller_.get(), bitrate_allocator_.get(), config,
      encoder_config, suspended_video_send_ssrcs_);

  if (!network_enabled_)
    send_stream->SignalNetworkState(kNetworkDown);

  WriteLockScoped write_lock(*send_crit_);
  for (uint32_t ssrc : config.rtp.ssrcs) {
    RTC_DCHECK(video_send_ssrcs_.find(ssrc) == video_send_ssrcs_.end());
    video_send_ssrcs_[ssrc] = send_stream;
  }
  video_send_streams_.insert(send_stream);

  if (event_log_)
    event_log_->LogVideoSendStreamConfig(config);

  return send_stream;
}

void Call::DestroyVideoSendStream(webrtc::VideoSendStream* send_stream) {
  TRACE_EVENT0("webrtc", "Call::DestroyVideoSendStream");
  RTC_DCHECK(send_stream != nullptr);
  RTC_DCHECK(configuration_thread_checker_.CalledOnValidThread());

  send_stream->Stop();

  VideoSendStream* send_stream_impl = nullptr;
  {
    WriteLockScoped write_lock(*send_crit_);
    auto it = video_send_ssrcs_.begin();
    while (it != video_send_ssrcs_.end()) {
      if (it->second == static_cast<VideoSendStream*>(send_stream)) {
        send_stream_impl = it->second;
        video_send_ssrcs_.erase(it++);
      } else {
        ++it;
      }
    }
    video_send_streams_.erase(send_stream_impl);
  }
  RTC_CHECK(send_stream_impl != nullptr);

  VideoSendStream::RtpStateMap rtp_state = send_stream_impl->GetRtpStates();

  for (VideoSendStream::RtpStateMap::iterator it = rtp_state.begin();
       it != rtp_state.end();
       ++it) {
    suspended_video_send_ssrcs_[it->first] = it->second;
  }

  delete send_stream_impl;
}

webrtc::VideoReceiveStream* Call::CreateVideoReceiveStream(
    const webrtc::VideoReceiveStream::Config& config) {
  TRACE_EVENT0("webrtc", "Call::CreateVideoReceiveStream");
  RTC_DCHECK(configuration_thread_checker_.CalledOnValidThread());
  VideoReceiveStream* receive_stream = new VideoReceiveStream(
      num_cpu_cores_, congestion_controller_.get(), config,
      voice_engine(), module_process_thread_.get(), call_stats_.get());

  WriteLockScoped write_lock(*receive_crit_);
  RTC_DCHECK(video_receive_ssrcs_.find(config.rtp.remote_ssrc) ==
             video_receive_ssrcs_.end());
  video_receive_ssrcs_[config.rtp.remote_ssrc] = receive_stream;
  // TODO(pbos): Configure different RTX payloads per receive payload.
  VideoReceiveStream::Config::Rtp::RtxMap::const_iterator it =
      config.rtp.rtx.begin();
  if (it != config.rtp.rtx.end())
    video_receive_ssrcs_[it->second.ssrc] = receive_stream;
  video_receive_streams_.insert(receive_stream);

  ConfigureSync(config.sync_group);

  if (!network_enabled_)
    receive_stream->SignalNetworkState(kNetworkDown);

  if (event_log_)
    event_log_->LogVideoReceiveStreamConfig(config);

  return receive_stream;
}

void Call::DestroyVideoReceiveStream(
    webrtc::VideoReceiveStream* receive_stream) {
  TRACE_EVENT0("webrtc", "Call::DestroyVideoReceiveStream");
  RTC_DCHECK(configuration_thread_checker_.CalledOnValidThread());
  RTC_DCHECK(receive_stream != nullptr);
  VideoReceiveStream* receive_stream_impl = nullptr;
  {
    WriteLockScoped write_lock(*receive_crit_);
    // Remove all ssrcs pointing to a receive stream. As RTX retransmits on a
    // separate SSRC there can be either one or two.
    auto it = video_receive_ssrcs_.begin();
    while (it != video_receive_ssrcs_.end()) {
      if (it->second == static_cast<VideoReceiveStream*>(receive_stream)) {
        if (receive_stream_impl != nullptr)
          RTC_DCHECK(receive_stream_impl == it->second);
        receive_stream_impl = it->second;
        video_receive_ssrcs_.erase(it++);
      } else {
        ++it;
      }
    }
    video_receive_streams_.erase(receive_stream_impl);
    RTC_CHECK(receive_stream_impl != nullptr);
    ConfigureSync(receive_stream_impl->config().sync_group);
  }
  delete receive_stream_impl;
}

Call::Stats Call::GetStats() const {
  // TODO(solenberg): Some test cases in EndToEndTest use this from a different
  // thread. Re-enable once that is fixed.
  // RTC_DCHECK(configuration_thread_checker_.CalledOnValidThread());
  Stats stats;
  // Fetch available send/receive bitrates.
  uint32_t send_bandwidth = 0;
  congestion_controller_->GetBitrateController()->AvailableBandwidth(
      &send_bandwidth);
  std::vector<unsigned int> ssrcs;
  uint32_t recv_bandwidth = 0;
  congestion_controller_->GetRemoteBitrateEstimator(false)->LatestEstimate(
      &ssrcs, &recv_bandwidth);
  stats.send_bandwidth_bps = send_bandwidth;
  stats.recv_bandwidth_bps = recv_bandwidth;
  stats.pacer_delay_ms = congestion_controller_->GetPacerQueuingDelayMs();
  {
    ReadLockScoped read_lock(*send_crit_);
    // TODO(solenberg): Add audio send streams.
    for (const auto& kv : video_send_ssrcs_) {
      int rtt_ms = kv.second->GetRtt();
      if (rtt_ms > 0)
        stats.rtt_ms = rtt_ms;
    }
  }
  return stats;
}

void Call::SetBitrateConfig(
    const webrtc::Call::Config::BitrateConfig& bitrate_config) {
  TRACE_EVENT0("webrtc", "Call::SetBitrateConfig");
  RTC_DCHECK(configuration_thread_checker_.CalledOnValidThread());
  RTC_DCHECK_GE(bitrate_config.min_bitrate_bps, 0);
  if (bitrate_config.max_bitrate_bps != -1)
    RTC_DCHECK_GT(bitrate_config.max_bitrate_bps, 0);
  if (config_.bitrate_config.min_bitrate_bps ==
          bitrate_config.min_bitrate_bps &&
      (bitrate_config.start_bitrate_bps <= 0 ||
       config_.bitrate_config.start_bitrate_bps ==
           bitrate_config.start_bitrate_bps) &&
      config_.bitrate_config.max_bitrate_bps ==
          bitrate_config.max_bitrate_bps) {
    // Nothing new to set, early abort to avoid encoder reconfigurations.
    return;
  }
  config_.bitrate_config = bitrate_config;
  congestion_controller_->SetBweBitrates(bitrate_config.min_bitrate_bps,
                                         bitrate_config.start_bitrate_bps,
                                         bitrate_config.max_bitrate_bps);
}

void Call::SignalNetworkState(NetworkState state) {
  RTC_DCHECK(configuration_thread_checker_.CalledOnValidThread());
  network_enabled_ = state == kNetworkUp;
  congestion_controller_->SignalNetworkState(state);
  {
    ReadLockScoped write_lock(*send_crit_);
    for (auto& kv : audio_send_ssrcs_) {
      kv.second->SignalNetworkState(state);
    }
    for (auto& kv : video_send_ssrcs_) {
      kv.second->SignalNetworkState(state);
    }
  }
  {
    ReadLockScoped write_lock(*receive_crit_);
    for (auto& kv : video_receive_ssrcs_) {
      kv.second->SignalNetworkState(state);
    }
  }
}

void Call::OnSentPacket(const rtc::SentPacket& sent_packet) {
  if (first_packet_sent_ms_ == -1)
    first_packet_sent_ms_ = clock_->TimeInMilliseconds();
  congestion_controller_->OnSentPacket(sent_packet);
}

void Call::OnNetworkChanged(uint32_t target_bitrate_bps, uint8_t fraction_loss,
                            int64_t rtt_ms) {
  uint32_t allocated_bitrate_bps = bitrate_allocator_->OnNetworkChanged(
      target_bitrate_bps, fraction_loss, rtt_ms);

  int pad_up_to_bitrate_bps = 0;
  {
    ReadLockScoped read_lock(*send_crit_);
    // No need to update as long as we're not sending.
    if (video_send_streams_.empty())
      return;

    for (VideoSendStream* stream : video_send_streams_)
      pad_up_to_bitrate_bps += stream->GetPaddingNeededBps();
  }
  // Allocated bitrate might be higher than bitrate estimate if enforcing min
  // bitrate, or lower if estimate is higher than the sum of max bitrates, so
  // set the pacer bitrate to the maximum of the two.
  uint32_t pacer_bitrate_bps =
      std::max(target_bitrate_bps, allocated_bitrate_bps);
  {
    rtc::CritScope lock(&bitrate_crit_);
    // We only update these stats if we have send streams, and assume that
    // OnNetworkChanged is called roughly with a fixed frequency.
    estimated_send_bitrate_sum_kbits_ += target_bitrate_bps / 1000;
    pacer_bitrate_sum_kbits_ += pacer_bitrate_bps / 1000;
    ++num_bitrate_updates_;
  }
  congestion_controller_->UpdatePacerBitrate(
      target_bitrate_bps / 1000,
      PacedSender::kDefaultPaceMultiplier * pacer_bitrate_bps / 1000,
      pad_up_to_bitrate_bps / 1000);
}

void Call::ConfigureSync(const std::string& sync_group) {
  // Set sync only if there was no previous one.
  if (voice_engine() == nullptr || sync_group.empty())
    return;

  AudioReceiveStream* sync_audio_stream = nullptr;
  // Find existing audio stream.
  const auto it = sync_stream_mapping_.find(sync_group);
  if (it != sync_stream_mapping_.end()) {
    sync_audio_stream = it->second;
  } else {
    // No configured audio stream, see if we can find one.
    for (const auto& kv : audio_receive_ssrcs_) {
      if (kv.second->config().sync_group == sync_group) {
        if (sync_audio_stream != nullptr) {
          LOG(LS_WARNING) << "Attempting to sync more than one audio stream "
                             "within the same sync group. This is not "
                             "supported in the current implementation.";
          break;
        }
        sync_audio_stream = kv.second;
      }
    }
  }
  if (sync_audio_stream)
    sync_stream_mapping_[sync_group] = sync_audio_stream;
  size_t num_synced_streams = 0;
  for (VideoReceiveStream* video_stream : video_receive_streams_) {
    if (video_stream->config().sync_group != sync_group)
      continue;
    ++num_synced_streams;
    if (num_synced_streams > 1) {
      // TODO(pbos): Support synchronizing more than one A/V pair.
      // https://code.google.com/p/webrtc/issues/detail?id=4762
      LOG(LS_WARNING) << "Attempting to sync more than one audio/video pair "
                         "within the same sync group. This is not supported in "
                         "the current implementation.";
    }
    // Only sync the first A/V pair within this sync group.
    if (sync_audio_stream != nullptr && num_synced_streams == 1) {
      video_stream->SetSyncChannel(voice_engine(),
                                   sync_audio_stream->config().voe_channel_id);
    } else {
      video_stream->SetSyncChannel(voice_engine(), -1);
    }
  }
}

PacketReceiver::DeliveryStatus Call::DeliverRtcp(MediaType media_type,
                                                 const uint8_t* packet,
                                                 size_t length) {
  TRACE_EVENT0("webrtc", "Call::DeliverRtcp");
  // TODO(pbos): Figure out what channel needs it actually.
  //             Do NOT broadcast! Also make sure it's a valid packet.
  //             Return DELIVERY_UNKNOWN_SSRC if it can be determined that
  //             there's no receiver of the packet.
  received_rtcp_bytes_ += length;
  bool rtcp_delivered = false;
  if (media_type == MediaType::ANY || media_type == MediaType::VIDEO) {
    ReadLockScoped read_lock(*receive_crit_);
    for (VideoReceiveStream* stream : video_receive_streams_) {
      if (stream->DeliverRtcp(packet, length)) {
        rtcp_delivered = true;
        if (event_log_)
          event_log_->LogRtcpPacket(true, media_type, packet, length);
      }
    }
  }
  if (media_type == MediaType::ANY || media_type == MediaType::VIDEO) {
    ReadLockScoped read_lock(*send_crit_);
    for (VideoSendStream* stream : video_send_streams_) {
      if (stream->DeliverRtcp(packet, length)) {
        rtcp_delivered = true;
        if (event_log_)
          event_log_->LogRtcpPacket(false, media_type, packet, length);
      }
    }
  }
  return rtcp_delivered ? DELIVERY_OK : DELIVERY_PACKET_ERROR;
}

PacketReceiver::DeliveryStatus Call::DeliverRtp(MediaType media_type,
                                                const uint8_t* packet,
                                                size_t length,
                                                const PacketTime& packet_time) {
  TRACE_EVENT0("webrtc", "Call::DeliverRtp");
  // Minimum RTP header size.
  if (length < 12)
    return DELIVERY_PACKET_ERROR;

  last_rtp_packet_received_ms_ = clock_->TimeInMilliseconds();
  if (first_rtp_packet_received_ms_ == -1)
    first_rtp_packet_received_ms_ = last_rtp_packet_received_ms_;

  uint32_t ssrc = ByteReader<uint32_t>::ReadBigEndian(&packet[8]);
  ReadLockScoped read_lock(*receive_crit_);
  if (media_type == MediaType::ANY || media_type == MediaType::AUDIO) {
    auto it = audio_receive_ssrcs_.find(ssrc);
    if (it != audio_receive_ssrcs_.end()) {
      received_audio_bytes_ += length;
      auto status = it->second->DeliverRtp(packet, length, packet_time)
                        ? DELIVERY_OK
                        : DELIVERY_PACKET_ERROR;
      if (status == DELIVERY_OK && event_log_)
        event_log_->LogRtpHeader(true, media_type, packet, length);
      return status;
    }
  }
  if (media_type == MediaType::ANY || media_type == MediaType::VIDEO) {
    auto it = video_receive_ssrcs_.find(ssrc);
    if (it != video_receive_ssrcs_.end()) {
      received_video_bytes_ += length;
      auto status = it->second->DeliverRtp(packet, length, packet_time)
                        ? DELIVERY_OK
                        : DELIVERY_PACKET_ERROR;
      if (status == DELIVERY_OK && event_log_)
        event_log_->LogRtpHeader(true, media_type, packet, length);
      return status;
    }
  }
  return DELIVERY_UNKNOWN_SSRC;
}

PacketReceiver::DeliveryStatus Call::DeliverPacket(
    MediaType media_type,
    const uint8_t* packet,
    size_t length,
    const PacketTime& packet_time) {
  // TODO(solenberg): Tests call this function on a network thread, libjingle
  // calls on the worker thread. We should move towards always using a network
  // thread. Then this check can be enabled.
  // RTC_DCHECK(!configuration_thread_checker_.CalledOnValidThread());
  if (RtpHeaderParser::IsRtcp(packet, length))
    return DeliverRtcp(media_type, packet, length);

  return DeliverRtp(media_type, packet, length, packet_time);
}

}  // namespace internal
}  // namespace webrtc
