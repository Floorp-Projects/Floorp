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
#include "modules/utility/interface/process_thread.h"
#include "video_engine/call_stats.h"
#include "video_engine/encoder_state_feedback.h"
#include "video_engine/vie_channel.h"
#include "video_engine/vie_encoder.h"
#include "video_engine/vie_remb.h"

namespace webrtc {

ChannelGroup::ChannelGroup(ProcessThread* process_thread,
                           const OverUseDetectorOptions& options,
                           RemoteBitrateEstimator::EstimationMode mode)
    : remb_(new VieRemb()),
      bitrate_controller_(BitrateController::CreateBitrateController()),
      call_stats_(new CallStats()),
      remote_bitrate_estimator_(RemoteBitrateEstimator::Create(
          options, mode, remb_.get(), Clock::GetRealTimeClock())),
      encoder_state_feedback_(new EncoderStateFeedback()),
      process_thread_(process_thread) {
  call_stats_->RegisterStatsObserver(remote_bitrate_estimator_.get());
  process_thread->RegisterModule(call_stats_.get());
  process_thread->RegisterModule(remote_bitrate_estimator_.get());
}

ChannelGroup::~ChannelGroup() {
  call_stats_->DeregisterStatsObserver(remote_bitrate_estimator_.get());
  process_thread_->DeRegisterModule(call_stats_.get());
  process_thread_->DeRegisterModule(remote_bitrate_estimator_.get());
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

CallStats* ChannelGroup::GetCallStats() {
  return call_stats_.get();
}

EncoderStateFeedback* ChannelGroup::GetEncoderStateFeedback() {
  return encoder_state_feedback_.get();
}

bool ChannelGroup::SetChannelRembStatus(int channel_id, bool sender,
                                        bool receiver, ViEChannel* channel) {
  // Update the channel state.
  if (sender || receiver) {
    if (!channel->EnableRemb(true)) {
      return false;
    }
  } else {
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
