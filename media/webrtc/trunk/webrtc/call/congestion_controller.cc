/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/call/congestion_controller.h"

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/common.h"
#include "webrtc/modules/bitrate_controller/include/bitrate_controller.h"
#include "webrtc/modules/pacing/paced_sender.h"
#include "webrtc/modules/pacing/packet_router.h"
#include "webrtc/modules/remote_bitrate_estimator/include/send_time_history.h"
#include "webrtc/modules/remote_bitrate_estimator/remote_bitrate_estimator_abs_send_time.h"
#include "webrtc/modules/remote_bitrate_estimator/remote_bitrate_estimator_single_stream.h"
#include "webrtc/modules/remote_bitrate_estimator/remote_estimator_proxy.h"
#include "webrtc/modules/remote_bitrate_estimator/transport_feedback_adapter.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp.h"
#include "webrtc/modules/utility/include/process_thread.h"
#include "webrtc/system_wrappers/include/critical_section_wrapper.h"
#include "webrtc/video/call_stats.h"
#include "webrtc/video/payload_router.h"
#include "webrtc/video/vie_encoder.h"
#include "webrtc/video/vie_remb.h"
#include "webrtc/voice_engine/include/voe_video_sync.h"

namespace webrtc {
namespace {

static const uint32_t kTimeOffsetSwitchThreshold = 30;

class WrappingBitrateEstimator : public RemoteBitrateEstimator {
 public:
  WrappingBitrateEstimator(RemoteBitrateObserver* observer, Clock* clock)
      : observer_(observer),
        clock_(clock),
        crit_sect_(CriticalSectionWrapper::CreateCriticalSection()),
        rbe_(new RemoteBitrateEstimatorSingleStream(observer_, clock_)),
        using_absolute_send_time_(false),
        packets_since_absolute_send_time_(0),
        min_bitrate_bps_(RemoteBitrateEstimator::kDefaultMinBitrateBps) {}

  virtual ~WrappingBitrateEstimator() {}

  void IncomingPacket(int64_t arrival_time_ms,
                      size_t payload_size,
                      const RTPHeader& header,
                      bool was_paced) override {
    CriticalSectionScoped cs(crit_sect_.get());
    PickEstimatorFromHeader(header);
    rbe_->IncomingPacket(arrival_time_ms, payload_size, header, was_paced);
  }

  int32_t Process() override {
    CriticalSectionScoped cs(crit_sect_.get());
    return rbe_->Process();
  }

  int64_t TimeUntilNextProcess() override {
    CriticalSectionScoped cs(crit_sect_.get());
    return rbe_->TimeUntilNextProcess();
  }

  void OnRttUpdate(int64_t avg_rtt_ms, int64_t max_rtt_ms) override {
    CriticalSectionScoped cs(crit_sect_.get());
    rbe_->OnRttUpdate(avg_rtt_ms, max_rtt_ms);
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

  void SetMinBitrate(int min_bitrate_bps) {
    CriticalSectionScoped cs(crit_sect_.get());
    rbe_->SetMinBitrate(min_bitrate_bps);
    min_bitrate_bps_ = min_bitrate_bps;
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
      rbe_.reset(new RemoteBitrateEstimatorAbsSendTime(observer_, clock_));
    } else {
      rbe_.reset(new RemoteBitrateEstimatorSingleStream(observer_, clock_));
    }
    rbe_->SetMinBitrate(min_bitrate_bps_);
  }

  RemoteBitrateObserver* observer_;
  Clock* clock_;
  rtc::scoped_ptr<CriticalSectionWrapper> crit_sect_;
  rtc::scoped_ptr<RemoteBitrateEstimator> rbe_;
  bool using_absolute_send_time_;
  uint32_t packets_since_absolute_send_time_;
  int min_bitrate_bps_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(WrappingBitrateEstimator);
};

}  // namespace

CongestionController::CongestionController(ProcessThread* process_thread,
                                           CallStats* call_stats,
                                           BitrateObserver* bitrate_observer)
    : remb_(new VieRemb(Clock::GetRealTimeClock())),
      packet_router_(new PacketRouter()),
      pacer_(new PacedSender(Clock::GetRealTimeClock(),
                             packet_router_.get(),
                             BitrateController::kDefaultStartBitrateKbps,
                             PacedSender::kDefaultPaceMultiplier *
                                 BitrateController::kDefaultStartBitrateKbps,
                             0)),
      remote_bitrate_estimator_(
          new WrappingBitrateEstimator(remb_.get(), Clock::GetRealTimeClock())),
      remote_estimator_proxy_(
          new RemoteEstimatorProxy(Clock::GetRealTimeClock(),
                                   packet_router_.get())),
      process_thread_(process_thread),
      call_stats_(call_stats),
      pacer_thread_(ProcessThread::Create("PacerThread")),
      // Constructed last as this object calls the provided callback on
      // construction.
      bitrate_controller_(
          BitrateController::CreateBitrateController(Clock::GetRealTimeClock(),
                                                     bitrate_observer)),
      min_bitrate_bps_(RemoteBitrateEstimator::kDefaultMinBitrateBps) {
  call_stats_->RegisterStatsObserver(remote_bitrate_estimator_.get());

  pacer_thread_->RegisterModule(pacer_.get());
  pacer_thread_->Start();

  process_thread->RegisterModule(remote_estimator_proxy_.get());
  process_thread->RegisterModule(remote_bitrate_estimator_.get());
  process_thread->RegisterModule(bitrate_controller_.get());
}

CongestionController::~CongestionController() {
  pacer_thread_->Stop();
  pacer_thread_->DeRegisterModule(pacer_.get());
  process_thread_->DeRegisterModule(bitrate_controller_.get());
  process_thread_->DeRegisterModule(remote_bitrate_estimator_.get());
  process_thread_->DeRegisterModule(remote_estimator_proxy_.get());
  call_stats_->DeregisterStatsObserver(remote_bitrate_estimator_.get());
  if (transport_feedback_adapter_.get())
    call_stats_->DeregisterStatsObserver(transport_feedback_adapter_.get());
  RTC_DCHECK(!remb_->InUse());
  RTC_DCHECK(encoders_.empty());
}

void CongestionController::AddEncoder(ViEEncoder* encoder) {
  rtc::CritScope lock(&encoder_crit_);
  encoders_.push_back(encoder);
}

void CongestionController::RemoveEncoder(ViEEncoder* encoder) {
  rtc::CritScope lock(&encoder_crit_);
  for (auto it = encoders_.begin(); it != encoders_.end(); ++it) {
    if (*it == encoder) {
      encoders_.erase(it);
      return;
    }
  }
}

void CongestionController::SetBweBitrates(int min_bitrate_bps,
                                          int start_bitrate_bps,
                                          int max_bitrate_bps) {
  if (start_bitrate_bps > 0)
    bitrate_controller_->SetStartBitrate(start_bitrate_bps);
  bitrate_controller_->SetMinMaxBitrate(min_bitrate_bps, max_bitrate_bps);
  if (remote_bitrate_estimator_.get())
    remote_bitrate_estimator_->SetMinBitrate(min_bitrate_bps);
  if (transport_feedback_adapter_.get())
    transport_feedback_adapter_->GetBitrateEstimator()->SetMinBitrate(
        min_bitrate_bps);
  min_bitrate_bps_ = min_bitrate_bps;
}

BitrateController* CongestionController::GetBitrateController() const {
  return bitrate_controller_.get();
}

RemoteBitrateEstimator* CongestionController::GetRemoteBitrateEstimator(
    bool send_side_bwe) const {

  if (send_side_bwe)
    return remote_estimator_proxy_.get();
  else
    return remote_bitrate_estimator_.get();
}

TransportFeedbackObserver*
CongestionController::GetTransportFeedbackObserver() {
  if (transport_feedback_adapter_.get() == nullptr) {
    transport_feedback_adapter_.reset(new TransportFeedbackAdapter(
        bitrate_controller_->CreateRtcpBandwidthObserver(),
        Clock::GetRealTimeClock(), process_thread_));
    transport_feedback_adapter_->SetBitrateEstimator(
        new RemoteBitrateEstimatorAbsSendTime(
            transport_feedback_adapter_.get(), Clock::GetRealTimeClock()));
    transport_feedback_adapter_->GetBitrateEstimator()->SetMinBitrate(
        min_bitrate_bps_);
    call_stats_->RegisterStatsObserver(transport_feedback_adapter_.get());
  }
  return transport_feedback_adapter_.get();
}

void CongestionController::UpdatePacerBitrate(int bitrate_kbps,
                                              int max_bitrate_kbps,
                                              int min_bitrate_kbps) {
  pacer_->UpdateBitrate(bitrate_kbps, max_bitrate_kbps, min_bitrate_kbps);
}

int64_t CongestionController::GetPacerQueuingDelayMs() const {
  return pacer_->QueueInMs();
}

// TODO(mflodman): Move out of this class.
void CongestionController::SetChannelRembStatus(bool sender,
                                                bool receiver,
                                                RtpRtcp* rtp_module) {
  rtp_module->SetREMBStatus(sender || receiver);
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

void CongestionController::SignalNetworkState(NetworkState state) {
  if (state == kNetworkUp) {
    pacer_->Resume();
  } else {
    pacer_->Pause();
  }
}

void CongestionController::OnSentPacket(const rtc::SentPacket& sent_packet) {
  if (transport_feedback_adapter_) {
    transport_feedback_adapter_->OnSentPacket(sent_packet.packet_id,
                                              sent_packet.send_time_ms);
  }
}
}  // namespace webrtc
