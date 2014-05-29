/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video_engine/vie_channel_manager.h"

#include "webrtc/common.h"
#include "webrtc/engine_configurations.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "webrtc/modules/utility/interface/process_thread.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/trace.h"
#include "webrtc/video_engine/call_stats.h"
#include "webrtc/video_engine/encoder_state_feedback.h"
#include "webrtc/video_engine/vie_channel.h"
#include "webrtc/video_engine/vie_defines.h"
#include "webrtc/video_engine/vie_encoder.h"
#include "webrtc/video_engine/vie_remb.h"
#include "webrtc/voice_engine/include/voe_video_sync.h"

namespace webrtc {

ViEChannelManager::ViEChannelManager(
    int engine_id,
    int number_of_cores,
    const Config& config)
    : channel_id_critsect_(CriticalSectionWrapper::CreateCriticalSection()),
      engine_id_(engine_id),
      number_of_cores_(number_of_cores),
      free_channel_ids_(new bool[kViEMaxNumberOfChannels]),
      free_channel_ids_size_(kViEMaxNumberOfChannels),
      voice_sync_interface_(NULL),
      voice_engine_(NULL),
      module_process_thread_(NULL),
      engine_config_(config),
      load_manager_(NULL)
{
  WEBRTC_TRACE(kTraceMemory, kTraceVideo, ViEId(engine_id),
               "ViEChannelManager::ViEChannelManager(engine_id: %d)",
               engine_id);
  for (int idx = 0; idx < free_channel_ids_size_; idx++) {
    free_channel_ids_[idx] = true;
  }
}

ViEChannelManager::~ViEChannelManager() {
  WEBRTC_TRACE(kTraceMemory, kTraceVideo, ViEId(engine_id_),
               "ViEChannelManager Destructor, engine_id: %d", engine_id_);

  while (channel_map_.size() > 0) {
    ChannelMap::iterator it = channel_map_.begin();
    // DeleteChannel will erase this channel from the map and invalidate |it|.
    DeleteChannel(it->first);
  }

  if (voice_sync_interface_) {
    voice_sync_interface_->Release();
  }
  if (channel_id_critsect_) {
    delete channel_id_critsect_;
    channel_id_critsect_ = NULL;
  }
  if (free_channel_ids_) {
    delete[] free_channel_ids_;
    free_channel_ids_ = NULL;
    free_channel_ids_size_ = 0;
  }
  assert(channel_groups_.empty());
  assert(channel_map_.empty());
  assert(vie_encoder_map_.empty());
}

void ViEChannelManager::SetModuleProcessThread(
    ProcessThread* module_process_thread) {
  assert(!module_process_thread_);
  module_process_thread_ = module_process_thread;
}

void ViEChannelManager::SetLoadManager(
    CPULoadStateCallbackInvoker* load_manager) {
  load_manager_ = load_manager;
  for (EncoderMap::const_iterator comp_it = vie_encoder_map_.begin();
       comp_it != vie_encoder_map_.end(); ++comp_it) {
    comp_it->second->SetLoadManager(load_manager);
  }
}

int ViEChannelManager::CreateChannel(int* channel_id,
                                     const Config* channel_group_config) {
  CriticalSectionScoped cs(channel_id_critsect_);

  // Get a new channel id.
  int new_channel_id = FreeChannelId();
  if (new_channel_id == -1) {
    return -1;
  }

  // Create a new channel group and add this channel.
  ChannelGroup* group = new ChannelGroup(engine_id_, module_process_thread_,
                                         channel_group_config);
  BitrateController* bitrate_controller = group->GetBitrateController();
  ViEEncoder* vie_encoder = new ViEEncoder(engine_id_, new_channel_id,
                                           number_of_cores_,
                                           engine_config_,
                                           *module_process_thread_,
                                           bitrate_controller);

  RtcpBandwidthObserver* bandwidth_observer =
      bitrate_controller->CreateRtcpBandwidthObserver();
  RemoteBitrateEstimator* remote_bitrate_estimator =
      group->GetRemoteBitrateEstimator();
  EncoderStateFeedback* encoder_state_feedback =
      group->GetEncoderStateFeedback();
  RtcpRttStats* rtcp_rtt_stats =
      group->GetCallStats()->rtcp_rtt_stats();

  if (!(vie_encoder->Init() &&
        CreateChannelObject(new_channel_id, vie_encoder, bandwidth_observer,
                            remote_bitrate_estimator, rtcp_rtt_stats,
                            encoder_state_feedback->GetRtcpIntraFrameObserver(),
                            true))) {
    delete vie_encoder;
    vie_encoder = NULL;
    ReturnChannelId(new_channel_id);
    delete group;
    return -1;
  }

  // Add ViEEncoder to EncoderFeedBackObserver.
  unsigned int ssrc = 0;
  int idx = 0;
  channel_map_[new_channel_id]->GetLocalSSRC(idx, &ssrc);
  encoder_state_feedback->AddEncoder(ssrc, vie_encoder);
  std::list<unsigned int> ssrcs;
  ssrcs.push_back(ssrc);
  vie_encoder->SetSsrcs(ssrcs);
  *channel_id = new_channel_id;
  group->AddChannel(*channel_id);
  channel_groups_.push_back(group);
  // Register the channel to receive stats updates.
  group->GetCallStats()->RegisterStatsObserver(
      channel_map_[new_channel_id]->GetStatsObserver());
  return 0;
}

int ViEChannelManager::CreateChannel(int* channel_id,
                                     int original_channel,
                                     bool sender) {
  CriticalSectionScoped cs(channel_id_critsect_);

  ChannelGroup* channel_group = FindGroup(original_channel);
  if (!channel_group) {
    return -1;
  }
  int new_channel_id = FreeChannelId();
  if (new_channel_id == -1) {
    return -1;
  }
  BitrateController* bitrate_controller = channel_group->GetBitrateController();
  RtcpBandwidthObserver* bandwidth_observer =
      bitrate_controller->CreateRtcpBandwidthObserver();
  RemoteBitrateEstimator* remote_bitrate_estimator =
      channel_group->GetRemoteBitrateEstimator();
  EncoderStateFeedback* encoder_state_feedback =
      channel_group->GetEncoderStateFeedback();
    RtcpRttStats* rtcp_rtt_stats =
        channel_group->GetCallStats()->rtcp_rtt_stats();

  ViEEncoder* vie_encoder = NULL;
  if (sender) {
    // We need to create a new ViEEncoder.
    vie_encoder = new ViEEncoder(engine_id_, new_channel_id, number_of_cores_,
                                 engine_config_,
                                 *module_process_thread_,
                                 bitrate_controller);
    if (!(vie_encoder->Init() &&
        CreateChannelObject(
            new_channel_id,
            vie_encoder,
            bandwidth_observer,
            remote_bitrate_estimator,
            rtcp_rtt_stats,
            encoder_state_feedback->GetRtcpIntraFrameObserver(),
            sender))) {
      delete vie_encoder;
      vie_encoder = NULL;
    }
    // Register the ViEEncoder to get key frame requests for this channel.
    unsigned int ssrc = 0;
    int stream_idx = 0;
    channel_map_[new_channel_id]->GetLocalSSRC(stream_idx, &ssrc);
    encoder_state_feedback->AddEncoder(ssrc, vie_encoder);
  } else {
    vie_encoder = ViEEncoderPtr(original_channel);
    assert(vie_encoder);
    if (!CreateChannelObject(
        new_channel_id,
        vie_encoder,
        bandwidth_observer,
        remote_bitrate_estimator,
        rtcp_rtt_stats,
        encoder_state_feedback->GetRtcpIntraFrameObserver(),
        sender)) {
      vie_encoder = NULL;
    }
  }
  if (!vie_encoder) {
    ReturnChannelId(new_channel_id);
    return -1;
  }
  *channel_id = new_channel_id;
  channel_group->AddChannel(*channel_id);
  // Register the channel to receive stats updates.
  channel_group->GetCallStats()->RegisterStatsObserver(
      channel_map_[new_channel_id]->GetStatsObserver());
  return 0;
}

int ViEChannelManager::DeleteChannel(int channel_id) {
  ViEChannel* vie_channel = NULL;
  ViEEncoder* vie_encoder = NULL;
  ChannelGroup* group = NULL;
  {
    // Write lock to make sure no one is using the channel.
    ViEManagerWriteScoped wl(this);

    // Protect the maps.
    CriticalSectionScoped cs(channel_id_critsect_);

    ChannelMap::iterator c_it = channel_map_.find(channel_id);
    if (c_it == channel_map_.end()) {
      // No such channel.
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_),
                   "%s Channel doesn't exist: %d", __FUNCTION__, channel_id);
      return -1;
    }
    vie_channel = c_it->second;
    channel_map_.erase(c_it);

    ReturnChannelId(channel_id);

    // Find the encoder object.
    EncoderMap::iterator e_it = vie_encoder_map_.find(channel_id);
    assert(e_it != vie_encoder_map_.end());
    vie_encoder = e_it->second;

    group = FindGroup(channel_id);
    group->GetCallStats()->DeregisterStatsObserver(
        vie_channel->GetStatsObserver());
    group->SetChannelRembStatus(channel_id, false, false, vie_channel);

    // Remove the feedback if we're owning the encoder.
    if (vie_encoder->channel_id() == channel_id) {
      group->GetEncoderStateFeedback()->RemoveEncoder(vie_encoder);
    }

    unsigned int remote_ssrc = 0;
    vie_channel->GetRemoteSSRC(&remote_ssrc);
    group->RemoveChannel(channel_id, remote_ssrc);

    // Check if other channels are using the same encoder.
    if (ChannelUsingViEEncoder(channel_id)) {
      vie_encoder = NULL;
    } else {
      // Delete later when we've released the critsect.
    }

    // We can't erase the item before we've checked for other channels using
    // same ViEEncoder.
    vie_encoder_map_.erase(e_it);

    if (group->Empty()) {
      channel_groups_.remove(group);
    } else {
      group = NULL;  // Prevent group from being deleted.
    }
  }
  delete vie_channel;
  // Leave the write critsect before deleting the objects.
  // Deleting a channel can cause other objects, such as renderers, to be
  // deleted, which might take time.
  // If statment just to show that this object is not always deleted.
  if (vie_encoder) {
    WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_),
                 "%s ViEEncoder deleted for channel %d", __FUNCTION__,
                 channel_id);
    delete vie_encoder;
  }
  // If statment just to show that this object is not always deleted.
  if (group) {
    // Delete the group if empty last since the encoder holds a pointer to the
    // BitrateController object that the group owns.
    WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_),
                 "%s ChannelGroup deleted for channel %d", __FUNCTION__,
                 channel_id);
    delete group;
  }
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_),
               "%s Channel %d deleted", __FUNCTION__, channel_id);
  return 0;
}

int ViEChannelManager::SetVoiceEngine(VoiceEngine* voice_engine) {
  // Write lock to make sure no one is using the channel.
  ViEManagerWriteScoped wl(this);

  CriticalSectionScoped cs(channel_id_critsect_);

  VoEVideoSync* sync_interface = NULL;
  if (voice_engine) {
    // Get new sync interface.
    sync_interface = VoEVideoSync::GetInterface(voice_engine);
    if (!sync_interface) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_),
                   "%s Can't get audio sync interface from VoiceEngine.",
                   __FUNCTION__);
      return -1;
    }
  }

  for (ChannelMap::iterator it = channel_map_.begin(); it != channel_map_.end();
       ++it) {
    it->second->SetVoiceChannel(-1, sync_interface);
  }
  if (voice_sync_interface_) {
    voice_sync_interface_->Release();
  }
  voice_engine_ = voice_engine;
  voice_sync_interface_ = sync_interface;
  return 0;
}

int ViEChannelManager::ConnectVoiceChannel(int channel_id,
                                           int audio_channel_id) {
  CriticalSectionScoped cs(channel_id_critsect_);
  if (!voice_sync_interface_) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id),
                 "No VoE set");
    return -1;
  }
  ViEChannel* channel = ViEChannelPtr(channel_id);
  if (!channel) {
    return -1;
  }
  return channel->SetVoiceChannel(audio_channel_id, voice_sync_interface_);
}

int ViEChannelManager::DisconnectVoiceChannel(int channel_id) {
  CriticalSectionScoped cs(channel_id_critsect_);
  ViEChannel* channel = ViEChannelPtr(channel_id);
  if (channel) {
    channel->SetVoiceChannel(-1, NULL);
    return 0;
  }
  return -1;
}

VoiceEngine* ViEChannelManager::GetVoiceEngine() {
  CriticalSectionScoped cs(channel_id_critsect_);
  return voice_engine_;
}

bool ViEChannelManager::SetRembStatus(int channel_id, bool sender,
                                      bool receiver) {
  CriticalSectionScoped cs(channel_id_critsect_);
  ChannelGroup* group = FindGroup(channel_id);
  if (!group) {
    return false;
  }
  ViEChannel* channel = ViEChannelPtr(channel_id);
  assert(channel);

  return group->SetChannelRembStatus(channel_id, sender, receiver, channel);
}

void ViEChannelManager::UpdateSsrcs(int channel_id,
                                    const std::list<unsigned int>& ssrcs) {
  CriticalSectionScoped cs(channel_id_critsect_);
  ChannelGroup* channel_group = FindGroup(channel_id);
  if (channel_group == NULL) {
    return;
  }
  ViEEncoder* encoder = ViEEncoderPtr(channel_id);
  assert(encoder);

  EncoderStateFeedback* encoder_state_feedback =
      channel_group->GetEncoderStateFeedback();
  // Remove a possible previous setting for this encoder before adding the new
  // setting.
  encoder_state_feedback->RemoveEncoder(encoder);
  for (std::list<unsigned int>::const_iterator it = ssrcs.begin();
       it != ssrcs.end(); ++it) {
    encoder_state_feedback->AddEncoder(*it, encoder);
  }
}

bool ViEChannelManager::CreateChannelObject(
    int channel_id,
    ViEEncoder* vie_encoder,
    RtcpBandwidthObserver* bandwidth_observer,
    RemoteBitrateEstimator* remote_bitrate_estimator,
    RtcpRttStats* rtcp_rtt_stats,
    RtcpIntraFrameObserver* intra_frame_observer,
    bool sender) {
  PacedSender* paced_sender = vie_encoder->GetPacedSender();

  // Register the channel at the encoder.
  RtpRtcp* send_rtp_rtcp_module = vie_encoder->SendRtpRtcpModule();

  ViEChannel* vie_channel = new ViEChannel(channel_id, engine_id_,
                                           number_of_cores_,
                                           engine_config_,
                                           *module_process_thread_,
                                           intra_frame_observer,
                                           bandwidth_observer,
                                           remote_bitrate_estimator,
                                           rtcp_rtt_stats,
                                           paced_sender,
                                           send_rtp_rtcp_module,
                                           sender);
  if (vie_channel->Init() != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_),
                 "%s could not init channel", __FUNCTION__, channel_id);
    delete vie_channel;
    return false;
  }
  VideoCodec encoder;
  if (vie_encoder->GetEncoder(&encoder) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id),
                 "%s: Could not GetEncoder.", __FUNCTION__);
    delete vie_channel;
    return false;
  }
  if (sender && vie_channel->SetSendCodec(encoder) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id),
                 "%s: Could not SetSendCodec.", __FUNCTION__);
    delete vie_channel;
    return false;
  }
  // Store the channel, add it to the channel group and save the vie_encoder.
  channel_map_[channel_id] = vie_channel;
  vie_encoder_map_[channel_id] = vie_encoder;
  return true;
}

ViEChannel* ViEChannelManager::ViEChannelPtr(int channel_id) const {
  CriticalSectionScoped cs(channel_id_critsect_);
  ChannelMap::const_iterator it = channel_map_.find(channel_id);
  if (it == channel_map_.end()) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_),
                 "%s Channel doesn't exist: %d", __FUNCTION__, channel_id);
    return NULL;
  }
  return it->second;
}

ViEEncoder* ViEChannelManager::ViEEncoderPtr(int video_channel_id) const {
  CriticalSectionScoped cs(channel_id_critsect_);
  EncoderMap::const_iterator it = vie_encoder_map_.find(video_channel_id);
  if (it == vie_encoder_map_.end()) {
    return NULL;
  }
  return it->second;
}

int ViEChannelManager::FreeChannelId() {
  int idx = 0;
  while (idx < free_channel_ids_size_) {
    if (free_channel_ids_[idx] == true) {
      // We've found a free id, allocate it and return.
      free_channel_ids_[idx] = false;
      return idx + kViEChannelIdBase;
    }
    idx++;
  }
  WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_),
               "Max number of channels reached: %d", channel_map_.size());
  return -1;
}

void ViEChannelManager::ReturnChannelId(int channel_id) {
  CriticalSectionScoped cs(channel_id_critsect_);
  assert(channel_id < kViEMaxNumberOfChannels + kViEChannelIdBase &&
         channel_id >= kViEChannelIdBase);
  free_channel_ids_[channel_id - kViEChannelIdBase] = true;
}

ChannelGroup* ViEChannelManager::FindGroup(int channel_id) {
  for (ChannelGroups::iterator it = channel_groups_.begin();
       it != channel_groups_.end(); ++it) {
    if ((*it)->HasChannel(channel_id)) {
      return *it;
    }
  }
  return NULL;
}

bool ViEChannelManager::ChannelUsingViEEncoder(int channel_id) const {
  CriticalSectionScoped cs(channel_id_critsect_);
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

void ViEChannelManager::ChannelsUsingViEEncoder(int channel_id,
                                                ChannelList* channels) const {
  CriticalSectionScoped cs(channel_id_critsect_);
  EncoderMap::const_iterator orig_it = vie_encoder_map_.find(channel_id);

  for (ChannelMap::const_iterator c_it = channel_map_.begin();
       c_it != channel_map_.end(); ++c_it) {
    EncoderMap::const_iterator comp_it = vie_encoder_map_.find(c_it->first);
    assert(comp_it != vie_encoder_map_.end());
    if (comp_it->second == orig_it->second) {
      channels->push_back(c_it->second);
    }
  }
}

ViEChannelManagerScoped::ViEChannelManagerScoped(
    const ViEChannelManager& vie_channel_manager)
    : ViEManagerScopedBase(vie_channel_manager) {
}

ViEChannel* ViEChannelManagerScoped::Channel(int vie_channel_id) const {
  return static_cast<const ViEChannelManager*>(vie_manager_)->ViEChannelPtr(
      vie_channel_id);
}
ViEEncoder* ViEChannelManagerScoped::Encoder(int vie_channel_id) const {
  return static_cast<const ViEChannelManager*>(vie_manager_)->ViEEncoderPtr(
      vie_channel_id);
}

bool ViEChannelManagerScoped::ChannelUsingViEEncoder(int channel_id) const {
  return (static_cast<const ViEChannelManager*>(vie_manager_))->
      ChannelUsingViEEncoder(channel_id);
}

void ViEChannelManagerScoped::ChannelsUsingViEEncoder(
    int channel_id, ChannelList* channels) const {
  (static_cast<const ViEChannelManager*>(vie_manager_))->
      ChannelsUsingViEEncoder(channel_id, channels);
}

}  // namespace webrtc
