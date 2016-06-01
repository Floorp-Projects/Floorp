/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video_engine/vie_channel_group.h"

#include "webrtc/base/checks.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/common.h"
#include "webrtc/experiments.h"
#include "webrtc/modules/pacing/include/paced_sender.h"
#include "webrtc/modules/pacing/include/packet_router.h"
#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "webrtc/modules/utility/interface/process_thread.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/video_engine/call_stats.h"
#include "webrtc/video_engine/encoder_state_feedback.h"
#include "webrtc/video_engine/payload_router.h"
#include "webrtc/video_engine/vie_channel.h"
#include "webrtc/video_engine/vie_encoder.h"
#include "webrtc/video_engine/vie_remb.h"
#include "webrtc/voice_engine/include/voe_video_sync.h"

namespace webrtc {
namespace {

static const uint32_t kTimeOffsetSwitchThreshold = 30;

class WrappingBitrateEstimator : public RemoteBitrateEstimator {
 public:
  WrappingBitrateEstimator(RemoteBitrateObserver* observer,
                           Clock* clock,
                           const Config& config)
      : observer_(observer),
        clock_(clock),
        crit_sect_(CriticalSectionWrapper::CreateCriticalSection()),
        min_bitrate_bps_(config.Get<RemoteBitrateEstimatorMinRate>().min_rate),
        rbe_(RemoteBitrateEstimatorFactory().Create(observer_,
                                                    clock_,
                                                    kAimdControl,
                                                    min_bitrate_bps_)),
        using_absolute_send_time_(false),
        packets_since_absolute_send_time_(0) {
  }

  virtual ~WrappingBitrateEstimator() {}

  void IncomingPacket(int64_t arrival_time_ms,
                      size_t payload_size,
                      const RTPHeader& header) override {
    CriticalSectionScoped cs(crit_sect_.get());
    PickEstimatorFromHeader(header);
    rbe_->IncomingPacket(arrival_time_ms, payload_size, header);
  }

  int32_t Process() override {
    CriticalSectionScoped cs(crit_sect_.get());
    return rbe_->Process();
  }

  int64_t TimeUntilNextProcess() override {
    CriticalSectionScoped cs(crit_sect_.get());
    return rbe_->TimeUntilNextProcess();
  }

  void OnRttUpdate(int64_t rtt) override {
    CriticalSectionScoped cs(crit_sect_.get());
    rbe_->OnRttUpdate(rtt);
  }

  void RemoveStream(unsigned int ssrc) override {
    CriticalSectionScoped cs(crit_sect_.get());
    rbe_->RemoveStream(ssrc);
  }

  bool LatestEstimate(std::vector<unsigned int>* ssrcs,
                      unsigned int* bitrate_bps) const override {
    CriticalSectionScoped cs(crit_sect_.get());
    return rbe_->LatestEstimate(ssrcs, bitrate_bps);
  }

  bool GetStats(ReceiveBandwidthEstimatorStats* output) const override {
    CriticalSectionScoped cs(crit_sect_.get());
    return rbe_->GetStats(output);
  }

 private:
  void PickEstimatorFromHeader(const RTPHeader& header)
      EXCLUSIVE_LOCKS_REQUIRED(crit_sect_.get()) {
    if (header.extension.hasAbsoluteSendTime) {
      // If we see AST in header, switch RBE strategy immediately.
      if (!using_absolute_send_time_) {
        LOG(LS_INFO) <<
            "WrappingBitrateEstimator: Switching to absolute send time RBE.";
        using_absolute_send_time_ = true;
        PickEstimator();
      }
      packets_since_absolute_send_time_ = 0;
    } else {
      // When we don't see AST, wait for a few packets before going back to TOF.
      if (using_absolute_send_time_) {
        ++packets_since_absolute_send_time_;
        if (packets_since_absolute_send_time_ >= kTimeOffsetSwitchThreshold) {
          LOG(LS_INFO) << "WrappingBitrateEstimator: Switching to transmission "
                       << "time offset RBE.";
          using_absolute_send_time_ = false;
          PickEstimator();
        }
      }
    }
  }

  // Instantiate RBE for Time Offset or Absolute Send Time extensions.
  void PickEstimator() EXCLUSIVE_LOCKS_REQUIRED(crit_sect_.get()) {
    if (using_absolute_send_time_) {
      rbe_.reset(AbsoluteSendTimeRemoteBitrateEstimatorFactory().Create(
          observer_, clock_, kAimdControl, min_bitrate_bps_));
    } else {
      rbe_.reset(RemoteBitrateEstimatorFactory().Create(
          observer_, clock_, kAimdControl, min_bitrate_bps_));
    }
  }

  RemoteBitrateObserver* observer_;
  Clock* clock_;
  rtc::scoped_ptr<CriticalSectionWrapper> crit_sect_;
  const uint32_t min_bitrate_bps_;
  rtc::scoped_ptr<RemoteBitrateEstimator> rbe_;
  bool using_absolute_send_time_;
  uint32_t packets_since_absolute_send_time_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(WrappingBitrateEstimator);
};
}  // namespace

ChannelGroup::ChannelGroup(ProcessThread* process_thread, const Config* config)
    : remb_(new VieRemb()),
      bitrate_allocator_(new BitrateAllocator()),
      call_stats_(new CallStats()),
      encoder_state_feedback_(new EncoderStateFeedback()),
      packet_router_(new PacketRouter()),
      pacer_(new PacedSender(Clock::GetRealTimeClock(),
                             packet_router_.get(),
                             BitrateController::kDefaultStartBitrateKbps,
                             PacedSender::kDefaultPaceMultiplier *
                                 BitrateController::kDefaultStartBitrateKbps,
                             0)),
      encoder_map_cs_(CriticalSectionWrapper::CreateCriticalSection()),
      config_(config),
      own_config_(),
      process_thread_(process_thread),
      pacer_thread_(ProcessThread::Create()),
      // Constructed last as this object calls the provided callback on
      // construction.
      bitrate_controller_(
          BitrateController::CreateBitrateController(Clock::GetRealTimeClock(),
                                                     this)) {
  if (!config) {
    own_config_.reset(new Config);
    config_ = own_config_.get();
  }
  DCHECK(config_);  // Must have a valid config pointer here.

  remote_bitrate_estimator_.reset(
      new WrappingBitrateEstimator(remb_.get(),
                                   Clock::GetRealTimeClock(),
                                   *config_));

  call_stats_->RegisterStatsObserver(remote_bitrate_estimator_.get());

  pacer_thread_->RegisterModule(pacer_.get());
  pacer_thread_->Start();

  process_thread->RegisterModule(remote_bitrate_estimator_.get());
  process_thread->RegisterModule(call_stats_.get());
  process_thread->RegisterModule(bitrate_controller_.get());
}

ChannelGroup::~ChannelGroup() {
  pacer_thread_->Stop();
  pacer_thread_->DeRegisterModule(pacer_.get());
  process_thread_->DeRegisterModule(bitrate_controller_.get());
  process_thread_->DeRegisterModule(call_stats_.get());
  process_thread_->DeRegisterModule(remote_bitrate_estimator_.get());
  call_stats_->DeregisterStatsObserver(remote_bitrate_estimator_.get());
  DCHECK(channels_.empty());
  DCHECK(channel_map_.empty());
  DCHECK(!remb_->InUse());
  DCHECK(vie_encoder_map_.empty());
  DCHECK(send_encoders_.empty());
}

bool ChannelGroup::CreateSendChannel(int channel_id,
                                     int engine_id,
                                     int number_of_cores,
                                     bool disable_default_encoder) {
  rtc::scoped_ptr<ViEEncoder> vie_encoder(new ViEEncoder(
      channel_id, number_of_cores, *config_, *process_thread_, pacer_.get(),
      bitrate_allocator_.get(), bitrate_controller_.get(), false));
  if (!vie_encoder->Init()) {
    return false;
  }
  ViEEncoder* encoder = vie_encoder.get();
  if (!CreateChannel(channel_id, engine_id, number_of_cores,
                     vie_encoder.release(), true, disable_default_encoder)) {
    return false;
  }
  ViEChannel* channel = channel_map_[channel_id];
  // Connect the encoder with the send packet router, to enable sending.
  encoder->StartThreadsAndSetSharedMembers(channel->send_payload_router(),
                                           channel->vcm_protection_callback());

  // Register the ViEEncoder to get key frame requests for this channel.
  unsigned int ssrc = 0;
  int stream_idx = 0;
  channel->GetLocalSSRC(stream_idx, &ssrc);
  encoder_state_feedback_->AddEncoder(ssrc, encoder);
  std::list<unsigned int> ssrcs;
  ssrcs.push_back(ssrc);
  encoder->SetSsrcs(ssrcs);
  return true;
}

bool ChannelGroup::CreateReceiveChannel(int channel_id,
                                        int engine_id,
                                        int base_channel_id,
                                        int number_of_cores,
                                        bool disable_default_encoder) {
  ViEEncoder* encoder = GetEncoder(base_channel_id);
  return CreateChannel(channel_id, engine_id, number_of_cores, encoder, false,
                       disable_default_encoder);
}

bool ChannelGroup::CreateChannel(int channel_id,
                                 int engine_id,
                                 int number_of_cores,
                                 ViEEncoder* vie_encoder,
                                 bool sender,
                                 bool disable_default_encoder) {
  DCHECK(vie_encoder);

  rtc::scoped_ptr<ViEChannel> channel(new ViEChannel(
      channel_id, engine_id, number_of_cores, *config_, *process_thread_,
      encoder_state_feedback_->GetRtcpIntraFrameObserver(),
      bitrate_controller_->CreateRtcpBandwidthObserver(),
      remote_bitrate_estimator_.get(), call_stats_->rtcp_rtt_stats(),
      pacer_.get(), packet_router_.get(), sender, disable_default_encoder));
  if (channel->Init() != 0) {
    return false;
  }
  if (!disable_default_encoder) {
    VideoCodec encoder;
    if (vie_encoder->GetEncoder(&encoder) != 0) {
      return false;
    }
    if (sender && channel->SetSendCodec(encoder) != 0) {
      return false;
    }
  }

  // Register the channel to receive stats updates.
  call_stats_->RegisterStatsObserver(channel->GetStatsObserver());

  // Store the channel, add it to the channel group and save the vie_encoder.
  channel_map_[channel_id] = channel.release();
  {
    CriticalSectionScoped lock(encoder_map_cs_.get());
    vie_encoder_map_[channel_id] = vie_encoder;
    if (sender)
      send_encoders_[channel_id] = vie_encoder;
  }

  return true;
}

void ChannelGroup::Stop(int channel_id) {
  ViEEncoder* vie_encoder = GetEncoder(channel_id);
  DCHECK(vie_encoder != NULL);

  // If we're owning the encoder, remove the feedback and stop all encoding
  // threads and processing. This must be done before deleting the channel.
  if (vie_encoder->channel_id() == channel_id) {
    encoder_state_feedback_->RemoveEncoder(vie_encoder);
    vie_encoder->StopThreadsAndRemoveSharedMembers();
  }
}

void ChannelGroup::DeleteChannel(int channel_id) {
  ViEChannel* vie_channel = PopChannel(channel_id);

  ViEEncoder* vie_encoder = GetEncoder(channel_id);
  DCHECK(vie_encoder != NULL);

  call_stats_->DeregisterStatsObserver(vie_channel->GetStatsObserver());
  SetChannelRembStatus(channel_id, false, false, vie_channel);

  // If we're owning the encoder, remove the feedback and stop all encoding
  // threads and processing. This must be done before deleting the channel.
  if (vie_encoder->channel_id() == channel_id) {
    encoder_state_feedback_->RemoveEncoder(vie_encoder);
    vie_encoder->StopThreadsAndRemoveSharedMembers();
  }

  unsigned int remote_ssrc = 0;
  vie_channel->GetRemoteSSRC(&remote_ssrc);
  RemoveChannel(channel_id);
  remote_bitrate_estimator_->RemoveStream(remote_ssrc);

  // Check if other channels are using the same encoder.
  if (OtherChannelsUsingEncoder(channel_id)) {
    vie_encoder = NULL;
  } else {
    // Delete later when we've released the critsect.
  }

  // We can't erase the item before we've checked for other channels using
  // same ViEEncoder.
  PopEncoder(channel_id);

  delete vie_channel;
  // Leave the write critsect before deleting the objects.
  // Deleting a channel can cause other objects, such as renderers, to be
  // deleted, which might take time.
  // If statment just to show that this object is not always deleted.
  if (vie_encoder) {
    LOG(LS_VERBOSE) << "ViEEncoder deleted for channel " << channel_id;
    delete vie_encoder;
  }

  LOG(LS_VERBOSE) << "Channel deleted " << channel_id;
}

void ChannelGroup::AddChannel(int channel_id) {
  channels_.insert(channel_id);
}

void ChannelGroup::RemoveChannel(int channel_id) {
  channels_.erase(channel_id);
}

bool ChannelGroup::HasChannel(int channel_id) const {
  return channels_.find(channel_id) != channels_.end();
}

bool ChannelGroup::Empty() const {
  return channels_.empty();
}

ViEChannel* ChannelGroup::GetChannel(int channel_id) const {
  ChannelMap::const_iterator it = channel_map_.find(channel_id);
  if (it == channel_map_.end()) {
    LOG(LS_ERROR) << "Channel doesn't exist " << channel_id;
    return NULL;
  }
  return it->second;
}

ViEEncoder* ChannelGroup::GetEncoder(int channel_id) const {
  CriticalSectionScoped lock(encoder_map_cs_.get());
  EncoderMap::const_iterator it = vie_encoder_map_.find(channel_id);
  if (it == vie_encoder_map_.end()) {
    return NULL;
  }
  return it->second;
}

ViEChannel* ChannelGroup::PopChannel(int channel_id) {
  ChannelMap::iterator c_it = channel_map_.find(channel_id);
  DCHECK(c_it != channel_map_.end());
  ViEChannel* channel = c_it->second;
  channel_map_.erase(c_it);

  return channel;
}

ViEEncoder* ChannelGroup::PopEncoder(int channel_id) {
  CriticalSectionScoped lock(encoder_map_cs_.get());
  auto it = vie_encoder_map_.find(channel_id);
  DCHECK(it != vie_encoder_map_.end());
  ViEEncoder* encoder = it->second;
  vie_encoder_map_.erase(it);

  it = send_encoders_.find(channel_id);
  if (it != send_encoders_.end())
    send_encoders_.erase(it);

  return encoder;
}

std::vector<int> ChannelGroup::GetChannelIds() const {
  std::vector<int> ids;
  for (auto channel : channel_map_)
    ids.push_back(channel.first);
  return ids;
}

bool ChannelGroup::OtherChannelsUsingEncoder(int channel_id) const {
  CriticalSectionScoped lock(encoder_map_cs_.get());
  EncoderMap::const_iterator orig_it = vie_encoder_map_.find(channel_id);
  if (orig_it == vie_encoder_map_.end()) {
    // No ViEEncoder for this channel.
    return false;
  }

  // Loop through all other channels to see if anyone points at the same
  // ViEEncoder.
  for (EncoderMap::const_iterator comp_it = vie_encoder_map_.begin();
       comp_it != vie_encoder_map_.end(); ++comp_it) {
    // Make sure we're not comparing the same channel with itself.
    if (comp_it->first != channel_id) {
      if (comp_it->second == orig_it->second) {
        return true;
      }
    }
  }
  return false;
}

void ChannelGroup::SetLoadManager(CPULoadStateCallbackInvoker* load_manager) {
  for (EncoderMap::const_iterator comp_it = vie_encoder_map_.begin();
       comp_it != vie_encoder_map_.end(); ++comp_it) {
    comp_it->second->SetLoadManager(load_manager);
  }
}

void ChannelGroup::SetSyncInterface(VoEVideoSync* sync_interface) {
  for (auto channel : channel_map_) {
    channel.second->SetVoiceChannel(-1, sync_interface);
  }
}

void ChannelGroup::GetChannelsUsingEncoder(int channel_id,
                                           ChannelList* channels) const {
  CriticalSectionScoped lock(encoder_map_cs_.get());
  EncoderMap::const_iterator orig_it = vie_encoder_map_.find(channel_id);

  for (ChannelMap::const_iterator c_it = channel_map_.begin();
       c_it != channel_map_.end(); ++c_it) {
    EncoderMap::const_iterator comp_it = vie_encoder_map_.find(c_it->first);
    DCHECK(comp_it != vie_encoder_map_.end());
    if (comp_it->second == orig_it->second) {
      channels->push_back(c_it->second);
    }
  }
}

BitrateController* ChannelGroup::GetBitrateController() const {
  return bitrate_controller_.get();
}

RemoteBitrateEstimator* ChannelGroup::GetRemoteBitrateEstimator() const {
  return remote_bitrate_estimator_.get();
}

CallStats* ChannelGroup::GetCallStats() const {
  return call_stats_.get();
}

EncoderStateFeedback* ChannelGroup::GetEncoderStateFeedback() const {
  return encoder_state_feedback_.get();
}

int64_t ChannelGroup::GetPacerQueuingDelayMs() const {
  return pacer_->QueueInMs();
}

void ChannelGroup::SetChannelRembStatus(int channel_id,
                                        bool sender,
                                        bool receiver,
                                        ViEChannel* channel) {
  // Update the channel state.
  channel->EnableRemb(sender || receiver);
  // Update the REMB instance with necessary RTP modules.
  RtpRtcp* rtp_module = channel->rtp_rtcp();
  if (sender) {
    remb_->AddRembSender(rtp_module);
  } else {
    remb_->RemoveRembSender(rtp_module);
  }
  if (receiver) {
    remb_->AddReceiveChannel(rtp_module);
  } else {
    remb_->RemoveReceiveChannel(rtp_module);
  }
}

void ChannelGroup::OnNetworkChanged(uint32_t target_bitrate_bps,
                                    uint8_t fraction_loss,
                                    int64_t rtt) {
  bitrate_allocator_->OnNetworkChanged(target_bitrate_bps, fraction_loss, rtt);
  int pad_up_to_bitrate_bps = 0;
  {
    CriticalSectionScoped lock(encoder_map_cs_.get());
    for (const auto& encoder : send_encoders_) {
      pad_up_to_bitrate_bps +=
          encoder.second->GetPaddingNeededBps(target_bitrate_bps);
    }
  }
  pacer_->UpdateBitrate(
      target_bitrate_bps / 1000,
      PacedSender::kDefaultPaceMultiplier * target_bitrate_bps / 1000,
      pad_up_to_bitrate_bps / 1000);
}
}  // namespace webrtc
