/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_CALL_CONGESTION_CONTROLLER_H_
#define WEBRTC_CALL_CONGESTION_CONTROLLER_H_

#include <vector>

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/socket.h"
#include "webrtc/stream.h"

namespace webrtc {

class BitrateController;
class BitrateObserver;
class CallStats;
class Config;
class PacedSender;
class PacketRouter;
class ProcessThread;
class RemoteBitrateEstimator;
class RemoteEstimatorProxy;
class RtpRtcp;
class SendStatisticsProxy;
class TransportFeedbackAdapter;
class TransportFeedbackObserver;
class ViEEncoder;
class VieRemb;

class CongestionController {
 public:
  CongestionController(ProcessThread* process_thread, CallStats* call_stats,
                       BitrateObserver* bitrate_observer);
  virtual ~CongestionController();
  virtual void AddEncoder(ViEEncoder* encoder);
  virtual void RemoveEncoder(ViEEncoder* encoder);
  virtual void SetBweBitrates(int min_bitrate_bps,
                              int start_bitrate_bps,
                              int max_bitrate_bps);

  virtual void SetChannelRembStatus(bool sender,
                                    bool receiver,
                                    RtpRtcp* rtp_module);

  virtual void SignalNetworkState(NetworkState state);

  virtual BitrateController* GetBitrateController() const;
  virtual RemoteBitrateEstimator* GetRemoteBitrateEstimator(
      bool send_side_bwe) const;
  virtual int64_t GetPacerQueuingDelayMs() const;
  virtual PacedSender* pacer() const { return pacer_.get(); }
  virtual PacketRouter* packet_router() const { return packet_router_.get(); }
  virtual TransportFeedbackObserver* GetTransportFeedbackObserver();

  virtual void UpdatePacerBitrate(int bitrate_kbps,
                                  int max_bitrate_kbps,
                                  int min_bitrate_kbps);

  virtual void OnSentPacket(const rtc::SentPacket& sent_packet);

 private:
  rtc::scoped_ptr<VieRemb> remb_;
  rtc::scoped_ptr<PacketRouter> packet_router_;
  rtc::scoped_ptr<PacedSender> pacer_;
  rtc::scoped_ptr<RemoteBitrateEstimator> remote_bitrate_estimator_;
  rtc::scoped_ptr<RemoteEstimatorProxy> remote_estimator_proxy_;

  mutable rtc::CriticalSection encoder_crit_;
  std::vector<ViEEncoder*> encoders_ GUARDED_BY(encoder_crit_);

  // Registered at construct time and assumed to outlive this class.
  ProcessThread* const process_thread_;
  CallStats* const call_stats_;

  rtc::scoped_ptr<ProcessThread> pacer_thread_;

  rtc::scoped_ptr<BitrateController> bitrate_controller_;
  rtc::scoped_ptr<TransportFeedbackAdapter> transport_feedback_adapter_;
  int min_bitrate_bps_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(CongestionController);
};

}  // namespace webrtc

#endif  // WEBRTC_CALL_CONGESTION_CONTROLLER_H_
