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
#include "webrtc/experiments.h"
#include "webrtc/modules/bitrate_controller/include/bitrate_controller.h"
#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "webrtc/modules/utility/interface/process_thread.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/trace.h"
#include "webrtc/video_engine/call_stats.h"
#include "webrtc/video_engine/encoder_state_feedback.h"
#include "webrtc/video_engine/vie_channel.h"
#include "webrtc/video_engine/vie_encoder.h"
#include "webrtc/video_engine/vie_remb.h"

namespace webrtc {
namespace {

static const uint32_t kTimeOffsetSwitchThreshold = 30;

class WrappingBitrateEstimator : public RemoteBitrateEstimator {
 public:
  WrappingBitrateEstimator(int engine_id, RemoteBitrateObserver* observer,
                           Clock* clock, ProcessThread* process_thread,
                           const Config& config)
      : observer_(observer),
        clock_(clock),
        process_thread_(process_thread),
        crit_sect_(CriticalSectionWrapper::CreateCriticalSection()),
        engine_id_(engine_id),
        min_bitrate_bps_(config.Get<RemoteBitrateEstimatorMinRate>().min_rate),
        rbe_(RemoteBitrateEstimatorFactory().Create(observer_, clock_,
                                                    min_bitrate_bps_)),
        using_absolute_send_time_(false),
        packets_since_absolute_send_time_(0) {
    assert(process_thread_ != NULL);
    process_thread_->RegisterModule(rbe_.get());
  }
  virtual ~WrappingBitrateEstimator() {
    process_thread_->DeRegisterModule(rbe_.get());
  }

  virtual void IncomingPacket(int64_t arrival_time_ms,
                              int payload_size,
                              const RTPHeader& header) {
    CriticalSectionScoped cs(crit_sect_.get());
    PickEstimator(header);
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

  virtual bool GetStats(ReceiveBandwidthEstimatorStats* output) const {
    CriticalSectionScoped cs(crit_sect_.get());
    return rbe_->GetStats(output);
  }

 private:
  // Instantiate RBE for Time Offset or Absolute Send Time extensions.
  void PickEstimator(const RTPHeader& header) {
    if (header.extension.hasAbsoluteSendTime) {
      // If we see AST in header, switch RBE strategy immediately.
      if (!using_absolute_send_time_) {
        process_thread_->DeRegisterModule(rbe_.get());
        WEBRTC_TRACE(kTraceStateInfo, kTraceVideo, ViEId(engine_id_),
            "WrappingBitrateEstimator: Switching to absolute send time RBE.");
        rbe_.reset(AbsoluteSendTimeRemoteBitrateEstimatorFactory().Create(
            observer_, clock_, min_bitrate_bps_));
        process_thread_->RegisterModule(rbe_.get());
        using_absolute_send_time_ = true;
      }
      packets_since_absolute_send_time_ = 0;
    } else {
      // When we don't see AST, wait for a few packets before going back to TOF.
      if (using_absolute_send_time_) {
        ++packets_since_absolute_send_time_;
        if (packets_since_absolute_send_time_ >= kTimeOffsetSwitchThreshold) {
          process_thread_->DeRegisterModule(rbe_.get());
          WEBRTC_TRACE(kTraceStateInfo, kTraceVideo, ViEId(engine_id_),
              "WrappingBitrateEstimator: Switching to transmission time offset "
              "RBE.");
          rbe_.reset(RemoteBitrateEstimatorFactory().Create(observer_, clock_,
              min_bitrate_bps_));
          process_thread_->RegisterModule(rbe_.get());
          using_absolute_send_time_ = false;
        }
      }
    }
  }

  RemoteBitrateObserver* observer_;
  Clock* clock_;
  ProcessThread* process_thread_;
  scoped_ptr<CriticalSectionWrapper> crit_sect_;
  const int engine_id_;
  const uint32_t min_bitrate_bps_;
  scoped_ptr<RemoteBitrateEstimator> rbe_;
  bool using_absolute_send_time_;
  uint32_t packets_since_absolute_send_time_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(WrappingBitrateEstimator);
};
}  // namespace

ChannelGroup::ChannelGroup(int engine_id, ProcessThread* process_thread,
                           const Config* config)
    : remb_(new VieRemb()),
      bitrate_controller_(BitrateController::CreateBitrateController(true)),
      call_stats_(new CallStats()),
      encoder_state_feedback_(new EncoderStateFeedback()),
      config_(config),
      own_config_(),
      process_thread_(process_thread) {
  if (!config) {
    own_config_.reset(new Config);
    config_ = own_config_.get();
  }
  assert(config_);  // Must have a valid config pointer here.
  remote_bitrate_estimator_.reset(
      new WrappingBitrateEstimator(engine_id, remb_.get(),
                                   Clock::GetRealTimeClock(), process_thread,
                                   *config_)),
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
}  // namespace webrtc
