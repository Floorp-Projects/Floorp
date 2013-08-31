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

#include "webrtc/common.h"
#include "webrtc/modules/bitrate_controller/include/bitrate_controller.h"
#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "webrtc/modules/utility/interface/process_thread.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/video_engine/call_stats.h"
#include "webrtc/video_engine/encoder_state_feedback.h"
#include "webrtc/video_engine/vie_channel.h"
#include "webrtc/video_engine/vie_encoder.h"
#include "webrtc/video_engine/vie_remb.h"

namespace webrtc {
namespace {

class WrappingBitrateEstimator : public RemoteBitrateEstimator {
 public:
  WrappingBitrateEstimator(RemoteBitrateObserver* observer, Clock* clock,
                           ProcessThread* process_thread)
      : observer_(observer),
        clock_(clock),
        process_thread_(process_thread),
        crit_sect_(CriticalSectionWrapper::CreateCriticalSection()),
        rbe_(RemoteBitrateEstimatorFactory().Create(observer_, clock_)),
        receive_absolute_send_time_(false) {
    assert(process_thread_ != NULL);
    process_thread_->RegisterModule(rbe_.get());
  }
  virtual ~WrappingBitrateEstimator() {
    process_thread_->DeRegisterModule(rbe_.get());
  }

  void SetReceiveAbsoluteSendTimeStatus(bool enable) {
    CriticalSectionScoped cs(crit_sect_.get());
    if (enable == receive_absolute_send_time_) {
      return;
    }

    process_thread_->DeRegisterModule(rbe_.get());
    if (enable) {
      rbe_.reset(AbsoluteSendTimeRemoteBitrateEstimatorFactory().Create(
          observer_, clock_));
    } else {
      rbe_.reset(RemoteBitrateEstimatorFactory().Create(observer_, clock_));
    }
    process_thread_->RegisterModule(rbe_.get());

    receive_absolute_send_time_ = enable;
  }

  virtual void IncomingRtcp(unsigned int ssrc, uint32_t ntp_secs,
                            uint32_t ntp_frac, uint32_t rtp_timestamp) {
    CriticalSectionScoped cs(crit_sect_.get());
    rbe_->IncomingRtcp(ssrc, ntp_secs, ntp_frac, rtp_timestamp);
  }

  virtual void IncomingPacket(int64_t arrival_time_ms,
                              int payload_size,
                              const RTPHeader& header) {
    CriticalSectionScoped cs(crit_sect_.get());
    rbe_->IncomingPacket(arrival_time_ms, payload_size, header);
  }

  virtual int32_t Process() {
    assert(false && "Not supposed to register the WrappingBitrateEstimator!");
    return 0;
  }

  virtual int32_t TimeUntilNextProcess() {
    assert(false && "Not supposed to register the WrappingBitrateEstimator!");
    return 0;
  }

  virtual void OnRttUpdate(uint32_t rtt) {
    CriticalSectionScoped cs(crit_sect_.get());
    rbe_->OnRttUpdate(rtt);
  }

  virtual void RemoveStream(unsigned int ssrc) {
    CriticalSectionScoped cs(crit_sect_.get());
    rbe_->RemoveStream(ssrc);
  }

  virtual bool LatestEstimate(std::vector<unsigned int>* ssrcs,
                              unsigned int* bitrate_bps) const {
    CriticalSectionScoped cs(crit_sect_.get());
    return rbe_->LatestEstimate(ssrcs, bitrate_bps);
  }

 private:
  RemoteBitrateObserver* observer_;
  Clock* clock_;
  ProcessThread* process_thread_;
  scoped_ptr<CriticalSectionWrapper> crit_sect_;
  scoped_ptr<RemoteBitrateEstimator> rbe_;
  bool receive_absolute_send_time_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(WrappingBitrateEstimator);
};
}  // namespace

ChannelGroup::ChannelGroup(ProcessThread* process_thread,
                           const Config& config)
    : remb_(new VieRemb()),
      bitrate_controller_(BitrateController::CreateBitrateController()),
      call_stats_(new CallStats()),
      remote_bitrate_estimator_(new WrappingBitrateEstimator(remb_.get(),
                                Clock::GetRealTimeClock(), process_thread)),
      encoder_state_feedback_(new EncoderStateFeedback()),
      process_thread_(process_thread) {
  call_stats_->RegisterStatsObserver(remote_bitrate_estimator_.get());
  process_thread->RegisterModule(call_stats_.get());
}

ChannelGroup::~ChannelGroup() {
  call_stats_->DeregisterStatsObserver(remote_bitrate_estimator_.get());
  process_thread_->DeRegisterModule(call_stats_.get());
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

void ChannelGroup::SetReceiveAbsoluteSendTimeStatus(bool enable) {
  static_cast<WrappingBitrateEstimator*>(remote_bitrate_estimator_.get())->
      SetReceiveAbsoluteSendTimeStatus(enable);
}
}  // namespace webrtc
