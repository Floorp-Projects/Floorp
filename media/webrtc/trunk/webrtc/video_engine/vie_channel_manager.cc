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

#include <vector>

#include "webrtc/common.h"
#include "webrtc/engine_configurations.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "webrtc/modules/utility/interface/process_thread.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/video_engine/call_stats.h"
#include "webrtc/video_engine/encoder_state_feedback.h"
#include "webrtc/video_engine/vie_channel.h"
#include "webrtc/video_engine/vie_channel_group.h"
#include "webrtc/video_engine/vie_defines.h"
#include "webrtc/video_engine/vie_encoder.h"
#include "webrtc/video_engine/vie_remb.h"
#include "webrtc/voice_engine/include/voe_video_sync.h"

namespace webrtc {

ViEChannelManager::ViEChannelManager(int engine_id,
                                     int number_of_cores,
                                     const Config& config)
    : channel_id_critsect_(CriticalSectionWrapper::CreateCriticalSection()),
      engine_id_(engine_id),
      number_of_cores_(number_of_cores),
      free_channel_ids_(new bool[kViEMaxNumberOfChannels]),
      free_channel_ids_size_(kViEMaxNumberOfChannels),
      voice_sync_interface_(NULL),
      module_process_thread_(NULL),
      load_manager_(NULL)
{
  for (int idx = 0; idx < free_channel_ids_size_; idx++) {
    free_channel_ids_[idx] = true;
  }
}

ViEChannelManager::~ViEChannelManager() {
  while (!channel_groups_.empty()) {
    // The channel group is deleted by DeleteChannel when all its channels have
    // been deleted.
    for (int channel_id : channel_groups_.front()->GetChannelIds()) {
      DeleteChannel(channel_id);
    }
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
}

void ViEChannelManager::SetModuleProcessThread(
    ProcessThread* module_process_thread) {
  assert(!module_process_thread_);
  module_process_thread_ = module_process_thread;
}

void ViEChannelManager::SetLoadManager(
    CPULoadStateCallbackInvoker* load_manager) {
  load_manager_ = load_manager;
  for (ChannelGroups::const_iterator it = channel_groups_.begin();
       it != channel_groups_.end(); ++it) {
    (*it)->SetLoadManager(load_manager);
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
  rtc::scoped_ptr<ChannelGroup> group(
      new ChannelGroup(module_process_thread_, channel_group_config));

  if (!group->CreateSendChannel(new_channel_id, engine_id_, number_of_cores_,
                                false)) {
    ReturnChannelId(new_channel_id);
    return -1;
  }

  *channel_id = new_channel_id;
  group->AddChannel(*channel_id);
  channel_groups_.push_back(group.release());
  return 0;
}

int ViEChannelManager::CreateChannel(int* channel_id,
                                     int original_channel,
                                     bool sender,
                                     bool disable_default_encoder) {
  CriticalSectionScoped cs(channel_id_critsect_);

  ChannelGroup* channel_group = FindGroup(original_channel);
  if (!channel_group) {
    return -1;
  }
  int new_channel_id = FreeChannelId();
  if (new_channel_id == -1) {
    return -1;
  }
  if (sender) {
    if (!channel_group->CreateSendChannel(new_channel_id, engine_id_,
                                          number_of_cores_,
                                          disable_default_encoder)) {
      ReturnChannelId(new_channel_id);
      return -1;
    }
  } else {
    if (!channel_group->CreateReceiveChannel(new_channel_id, engine_id_,
                                             original_channel, number_of_cores_,
                                             disable_default_encoder)) {
      ReturnChannelId(new_channel_id);
      return -1;
    }
  }
  *channel_id = new_channel_id;
  channel_group->AddChannel(*channel_id);
  return 0;
}

int ViEChannelManager::DeleteChannel(int channel_id) {
  ChannelGroup* group = NULL;
  {
    // Write lock to make sure no one is using the channel.
    ViEManagerWriteScoped wl(this);

    // Protect the maps.
    CriticalSectionScoped cs(channel_id_critsect_);

    group = FindGroup(channel_id);
    if (group == NULL)
      return -1;
    ReturnChannelId(channel_id);
    group->DeleteChannel(channel_id);

    if (group->Empty()) {
      channel_groups_.remove(group);
    } else {
      group = NULL;  // Prevent group from being deleted.
    }
  }
  // If statment just to show that this object is not always deleted.
  if (group) {
    // Delete the group if empty last since the encoder holds a pointer to the
    // BitrateController object that the group owns.
    LOG(LS_VERBOSE) << "Channel group deleted for channel " << channel_id;
    delete group;
  }
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
      return -1;
    }
  }

  for (ChannelGroup* group : channel_groups_) {
    group->SetSyncInterface(sync_interface);
  }
  if (voice_sync_interface_) {
    voice_sync_interface_->Release();
  }
  voice_sync_interface_ = sync_interface;
  return 0;
}

int ViEChannelManager::ConnectVoiceChannel(int channel_id,
                                           int audio_channel_id) {
  CriticalSectionScoped cs(channel_id_critsect_);
  if (!voice_sync_interface_) {
    LOG_F(LS_ERROR) << "No VoE set.";
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

bool ViEChannelManager::SetRembStatus(int channel_id, bool sender,
                                      bool receiver) {
  CriticalSectionScoped cs(channel_id_critsect_);
  ChannelGroup* group = FindGroup(channel_id);
  if (!group) {
    return false;
  }
  ViEChannel* channel = ViEChannelPtr(channel_id);
  assert(channel);

  group->SetChannelRembStatus(channel_id, sender, receiver, channel);
  return true;
}

bool ViEChannelManager::SetReservedTransmitBitrate(
    int channel_id, uint32_t reserved_transmit_bitrate_bps) {
  CriticalSectionScoped cs(channel_id_critsect_);
  ChannelGroup* group = FindGroup(channel_id);
  if (!group) {
    return false;
  }

  BitrateController* bitrate_controller = group->GetBitrateController();
  bitrate_controller->SetReservedBitrate(reserved_transmit_bitrate_bps);
  return true;
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

bool ViEChannelManager::GetEstimatedSendBandwidth(
    int channel_id, uint32_t* estimated_bandwidth) const {
  CriticalSectionScoped cs(channel_id_critsect_);
  ChannelGroup* group = FindGroup(channel_id);
  if (!group) {
    return false;
  }
  group->GetBitrateController()->AvailableBandwidth(estimated_bandwidth);
  return true;
}

bool ViEChannelManager::GetEstimatedReceiveBandwidth(
    int channel_id, uint32_t* estimated_bandwidth) const {
  CriticalSectionScoped cs(channel_id_critsect_);
  ChannelGroup* group = FindGroup(channel_id);
  if (!group) {
    return false;
  }
  std::vector<unsigned int> ssrcs;
  if (!group->GetRemoteBitrateEstimator()->LatestEstimate(
      &ssrcs, estimated_bandwidth) || ssrcs.empty()) {
    *estimated_bandwidth = 0;
  }
  return true;
}

bool ViEChannelManager::GetPacerQueuingDelayMs(int channel_id,
                                               int64_t* delay_ms) const {
  CriticalSectionScoped cs(channel_id_critsect_);
  ChannelGroup* group = FindGroup(channel_id);
  if (!group)
    return false;
  *delay_ms = group->GetPacerQueuingDelayMs();
  return true;
}

bool ViEChannelManager::SetBitrateConfig(int channel_id,
                                         int min_bitrate_bps,
                                         int start_bitrate_bps,
                                         int max_bitrate_bps) {
  CriticalSectionScoped cs(channel_id_critsect_);
  ChannelGroup* group = FindGroup(channel_id);
  if (!group)
    return false;
  BitrateController* bitrate_controller = group->GetBitrateController();
  if (start_bitrate_bps > 0)
    bitrate_controller->SetStartBitrate(start_bitrate_bps);
  bitrate_controller->SetMinMaxBitrate(min_bitrate_bps, max_bitrate_bps);
  return true;
}

ViEChannel* ViEChannelManager::ViEChannelPtr(int channel_id) const {
  CriticalSectionScoped cs(channel_id_critsect_);
  ChannelGroup* group = FindGroup(channel_id);
  if (group == NULL)
    return NULL;
  return group->GetChannel(channel_id);
}

ViEEncoder* ViEChannelManager::ViEEncoderPtr(int video_channel_id) const {
  CriticalSectionScoped cs(channel_id_critsect_);
  ChannelGroup* group = FindGroup(video_channel_id);
  if (group == NULL) {
    return NULL;
  }
  return group->GetEncoder(video_channel_id);
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
  LOG(LS_ERROR) << "Max number of channels reached.";
  return -1;
}

void ViEChannelManager::ReturnChannelId(int channel_id) {
  CriticalSectionScoped cs(channel_id_critsect_);
  assert(channel_id < kViEMaxNumberOfChannels + kViEChannelIdBase &&
         channel_id >= kViEChannelIdBase);
  free_channel_ids_[channel_id - kViEChannelIdBase] = true;
}

ChannelGroup* ViEChannelManager::FindGroup(int channel_id) const {
  for (ChannelGroups::const_iterator it = channel_groups_.begin();
       it != channel_groups_.end(); ++it) {
    if ((*it)->HasChannel(channel_id)) {
      return *it;
    }
  }
  return NULL;
}

bool ViEChannelManager::ChannelUsingViEEncoder(int channel_id) const {
  CriticalSectionScoped cs(channel_id_critsect_);
  ChannelGroup* group = FindGroup(channel_id);
  if (group == NULL) {
    return false;
  }
  return group->OtherChannelsUsingEncoder(channel_id);
}

void ViEChannelManager::ChannelsUsingViEEncoder(int channel_id,
                                                ChannelList* channels) const {
  CriticalSectionScoped cs(channel_id_critsect_);
  ChannelGroup* group = FindGroup(channel_id);
  if (group == NULL)
    return;
  group->GetChannelsUsingEncoder(channel_id, channels);
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
