/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_VIE_CHANNEL_GROUP_H_
#define WEBRTC_VIDEO_ENGINE_VIE_CHANNEL_GROUP_H_

#include <list>
#include <map>
#include <set>
#include <vector>

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/bitrate_controller/include/bitrate_controller.h"

namespace webrtc {

class BitrateAllocator;
class CallStats;
class Config;
class EncoderStateFeedback;
class PacedSender;
class PacketRouter;
class ProcessThread;
class RemoteBitrateEstimator;
class ViEChannel;
class ViEEncoder;
class VieRemb;
class VoEVideoSync;
class CPULoadStateCallbackInvoker;

typedef std::list<ViEChannel*> ChannelList;

// Channel group contains data common for several channels. All channels in the
// group are assumed to send/receive data to the same end-point.
class ChannelGroup : public BitrateObserver {
 public:
  ChannelGroup(ProcessThread* process_thread, const Config* config);
  ~ChannelGroup();
  bool CreateSendChannel(int channel_id,
                         int engine_id,
                         int number_of_cores,
                         bool disable_default_encoder);
  bool CreateReceiveChannel(int channel_id,
                            int engine_id,
                            int base_channel_id,
                            int number_of_cores,
                            bool disable_default_encoder);
  void DeleteChannel(int channel_id);
  void AddChannel(int channel_id);
  void RemoveChannel(int channel_id);
  bool HasChannel(int channel_id) const;
  bool Empty() const;
  ViEChannel* GetChannel(int channel_id) const;
  ViEEncoder* GetEncoder(int channel_id) const;
  std::vector<int> GetChannelIds() const;
  bool OtherChannelsUsingEncoder(int channel_id) const;
  void SetLoadManager(CPULoadStateCallbackInvoker* load_manager);
  void GetChannelsUsingEncoder(int channel_id, ChannelList* channels) const;

  void SetSyncInterface(VoEVideoSync* sync_interface);

  void SetChannelRembStatus(int channel_id,
                            bool sender,
                            bool receiver,
                            ViEChannel* channel);

  BitrateController* GetBitrateController() const;
  CallStats* GetCallStats() const;
  RemoteBitrateEstimator* GetRemoteBitrateEstimator() const;
  EncoderStateFeedback* GetEncoderStateFeedback() const;
  int64_t GetPacerQueuingDelayMs() const;

  // Implements BitrateObserver.
  void OnNetworkChanged(uint32_t target_bitrate_bps,
                        uint8_t fraction_loss,
                        int64_t rtt) override;

 private:
  typedef std::map<int, ViEChannel*> ChannelMap;
  typedef std::set<int> ChannelSet;
  typedef std::map<int, ViEEncoder*> EncoderMap;

  bool CreateChannel(int channel_id,
                     int engine_id,
                     int number_of_cores,
                     ViEEncoder* vie_encoder,
                     bool sender,
                     bool disable_default_encoder);
  ViEChannel* PopChannel(int channel_id);
  ViEEncoder* PopEncoder(int channel_id);

  rtc::scoped_ptr<VieRemb> remb_;
  rtc::scoped_ptr<BitrateAllocator> bitrate_allocator_;
  rtc::scoped_ptr<CallStats> call_stats_;
  rtc::scoped_ptr<RemoteBitrateEstimator> remote_bitrate_estimator_;
  rtc::scoped_ptr<EncoderStateFeedback> encoder_state_feedback_;
  rtc::scoped_ptr<PacketRouter> packet_router_;
  rtc::scoped_ptr<PacedSender> pacer_;
  ChannelSet channels_;
  ChannelMap channel_map_;
  // Maps Channel id -> ViEEncoder.
  EncoderMap vie_encoder_map_;
  EncoderMap send_encoders_;
  rtc::scoped_ptr<CriticalSectionWrapper> encoder_map_cs_;

  const Config* config_;
  // Placeholder for the case where this owns the config.
  rtc::scoped_ptr<Config> own_config_;

  // Registered at construct time and assumed to outlive this class.
  ProcessThread* process_thread_;
  rtc::scoped_ptr<ProcessThread> pacer_thread_;

  rtc::scoped_ptr<BitrateController> bitrate_controller_;
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_VIE_CHANNEL_GROUP_H_
