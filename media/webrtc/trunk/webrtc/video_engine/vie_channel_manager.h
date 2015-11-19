/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_VIE_CHANNEL_MANAGER_H_
#define WEBRTC_VIDEO_ENGINE_VIE_CHANNEL_MANAGER_H_

#include <list>
#include <map>

#include "webrtc/engine_configurations.h"
#include "webrtc/typedefs.h"
#include "webrtc/video_engine/include/vie_rtp_rtcp.h"
#include "webrtc/video_engine/vie_defines.h"
#include "webrtc/video_engine/vie_manager_base.h"
#include "webrtc/video_engine/vie_remb.h"

namespace webrtc {

class ChannelGroup;
class Config;
class CriticalSectionWrapper;
class ProcessThread;
class RtcpRttStats;
class ViEChannel;
class ViEEncoder;
class VoEVideoSync;
class VoiceEngine;

typedef std::list<ChannelGroup*> ChannelGroups;
typedef std::list<ViEChannel*> ChannelList;

class ViEChannelManager: private ViEManagerBase {
  friend class ViEChannelManagerScoped;
 public:
  ViEChannelManager(int engine_id,
                    int number_of_cores,
                    const Config& config);
  ~ViEChannelManager();

  void SetModuleProcessThread(ProcessThread* module_process_thread);

  void SetLoadManager(CPULoadStateCallbackInvoker* load_manager);

  // Creates a new channel. 'channel_id' will be the id of the created channel.
  int CreateChannel(int* channel_id,
                    const Config* config);

  // Creates a new channel grouped with |original_channel|. The new channel
  // will get its own |ViEEncoder| if |sender| is set to true. It will be a
  // receive only channel, without an own |ViEEncoder| if |sender| is false.
  // Doesn't internally allocate an encoder if |disable_default_encoder|.
  int CreateChannel(int* channel_id,
                    int original_channel,
                    bool sender,
                    bool disable_default_encoder);

  // Deletes a channel.
  int DeleteChannel(int channel_id);

  // Set the voice engine instance to be used by all video channels.
  int SetVoiceEngine(VoiceEngine* voice_engine);

  // Enables lip sync of the channel.
  int ConnectVoiceChannel(int channel_id, int audio_channel_id);

  // Disables lip sync of the channel.
  int DisconnectVoiceChannel(int channel_id);

  // Adds a channel to include when sending REMB.
  bool SetRembStatus(int channel_id, bool sender, bool receiver);

  bool SetReservedTransmitBitrate(int channel_id,
                                  uint32_t reserved_transmit_bitrate_bps);

  // Updates the SSRCs for a channel. If one of the SSRCs already is registered,
  // it will simply be ignored and no error is returned.
  void UpdateSsrcs(int channel_id, const std::list<unsigned int>& ssrcs);

  bool GetEstimatedSendBandwidth(int channel_id,
                                 uint32_t* estimated_bandwidth) const;
  bool GetEstimatedReceiveBandwidth(int channel_id,
                                    uint32_t* estimated_bandwidth) const;

  bool GetPacerQueuingDelayMs(int channel_id, int64_t* delay_ms) const;

  bool SetBitrateConfig(int channel_id,
                        int min_bitrate_bps,
                        int start_bitrate_bps,
                        int max_bitrate_bps);

  bool ReAllocateBitrates(int channel_id);

 private:
  // Used by ViEChannelScoped, forcing a manager user to use scoped.
  // Returns a pointer to the channel with id 'channel_id'.
  ViEChannel* ViEChannelPtr(int channel_id) const;

  // Methods used by ViECaptureScoped and ViEEncoderScoped.
  // Gets the ViEEncoder used as input for video_channel_id
  ViEEncoder* ViEEncoderPtr(int video_channel_id) const;

  // Returns a free channel id, -1 if failing.
  int FreeChannelId();

  // Returns a previously allocated channel id.
  void ReturnChannelId(int channel_id);

  // Returns the iterator to the ChannelGroup containing |channel_id|.
  ChannelGroup* FindGroup(int channel_id) const;

  // Returns true if at least one other channels uses the same ViEEncoder as
  // channel_id.
  bool ChannelUsingViEEncoder(int channel_id) const;
  void ChannelsUsingViEEncoder(int channel_id, ChannelList* channels) const;

  // Protects channel_map_ and free_channel_ids_.
  CriticalSectionWrapper* channel_id_critsect_;
  int engine_id_;
  int number_of_cores_;

  bool* free_channel_ids_;
  int free_channel_ids_size_;

  // List with all channel groups.
  std::list<ChannelGroup*> channel_groups_;

  // TODO(mflodman) Make part of channel group.
  VoEVideoSync* voice_sync_interface_;

  ProcessThread* module_process_thread_;
  CPULoadStateCallbackInvoker* load_manager_;
};

class ViEChannelManagerScoped: private ViEManagerScopedBase {
 public:
  explicit ViEChannelManagerScoped(
      const ViEChannelManager& vie_channel_manager);
  ViEChannel* Channel(int vie_channel_id) const;
  ViEEncoder* Encoder(int vie_channel_id) const;

  // Returns true if at least one other channels uses the same ViEEncoder as
  // channel_id.
  bool ChannelUsingViEEncoder(int channel_id) const;

  // Returns a list with pointers to all channels using the same encoder as the
  // channel with |channel_id|, including the one with the specified id.
  void ChannelsUsingViEEncoder(int channel_id, ChannelList* channels) const;
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_VIE_CHANNEL_MANAGER_H_
