/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_engine/vie_channel_group.h"

#include "modules/bitrate_controller/include/bitrate_controller.h"
#include "modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "video_engine/vie_channel.h"
#include "video_engine/vie_encoder.h"
#include "video_engine/vie_remb.h"

namespace webrtc {

ChannelGroup::ChannelGroup(ProcessThread* process_thread,
                           const OverUseDetectorOptions& options)
    : remb_(new VieRemb(process_thread)),
      bitrate_controller_(BitrateController::CreateBitrateController()),
      remote_bitrate_estimator_(new RemoteBitrateEstimator(remb_.get(),
                                                           options)) {
}

ChannelGroup::~ChannelGroup() {
  assert(channels_.empty());
  assert(!remb_->InUse());
}
void ChannelGroup::AddChannel(int channel_id) {
  channels_.insert(channel_id);
}

void ChannelGroup::RemoveChannel(int channel_id, unsigned int ssrc) {
  channels_.erase(channel_id);
  remote_bitrate_estimator_->RemoveStream(ssrc);
}

bool ChannelGroup::HasChannel(int channel_id) {
  return channels_.find(channel_id) != channels_.end();
}

bool ChannelGroup::Empty() {
  return channels_.empty();
}

BitrateController* ChannelGroup::GetBitrateController() {
  return bitrate_controller_.get();
}

RemoteBitrateEstimator* ChannelGroup::GetRemoteBitrateEstimator() {
  return remote_bitrate_estimator_.get();
}

bool ChannelGroup::SetChannelRembStatus(int channel_id,
                                        bool sender,
                                        bool receiver,
                                        ViEChannel* channel,
                                        ViEEncoder* encoder) {
  // Update the channel state.
  if (sender || receiver) {
    if (!channel->EnableRemb(true)) {
      return false;
    }
  } else if (channel) {
    channel->EnableRemb(false);
  }
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
  return true;
}

}  // namespace webrtc
