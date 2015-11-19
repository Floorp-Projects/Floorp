/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 *  Usage: this class will register multiple RtcpBitrateObserver's one at each
 *  RTCP module. It will aggregate the results and run one bandwidth estimation
 *  and push the result to the encoder via VideoEncoderCallback.
 */

#ifndef WEBRTC_MODULES_BITRATE_CONTROLLER_BITRATE_CONTROLLER_IMPL_H_
#define WEBRTC_MODULES_BITRATE_CONTROLLER_BITRATE_CONTROLLER_IMPL_H_

#include "webrtc/modules/bitrate_controller/include/bitrate_controller.h"

#include <list>
#include <utility>

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/bitrate_controller/send_side_bandwidth_estimation.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"

namespace webrtc {

class BitrateControllerImpl : public BitrateController {
 public:
  BitrateControllerImpl(Clock* clock, BitrateObserver* observer);
  virtual ~BitrateControllerImpl() {}

  bool AvailableBandwidth(uint32_t* bandwidth) const override;

  RtcpBandwidthObserver* CreateRtcpBandwidthObserver() override;

  void SetStartBitrate(int start_bitrate_bps) override;
  void SetMinMaxBitrate(int min_bitrate_bps,
                                int max_bitrate_bps) override;

  void SetReservedBitrate(uint32_t reserved_bitrate_bps) override;

  int64_t TimeUntilNextProcess() override;
  int32_t Process() override;

 private:
  class RtcpBandwidthObserverImpl;

  // Called by BitrateObserver's direct from the RTCP module.
  void OnReceivedEstimatedBitrate(uint32_t bitrate);

  void OnReceivedRtcpReceiverReport(uint8_t fraction_loss,
                                    int64_t rtt,
                                    int number_of_packets,
                                    int64_t now_ms);

  void MaybeTriggerOnNetworkChanged();

  // Returns true if the parameters have changed since the last call.
  bool GetNetworkParameters(uint32_t* bitrate,
                            uint8_t* fraction_loss,
                            int64_t* rtt);

  void OnNetworkChanged(uint32_t bitrate,
                        uint8_t fraction_loss,  // 0 - 255.
                        int64_t rtt)
      EXCLUSIVE_LOCKS_REQUIRED(*critsect_);

  // Used by process thread.
  Clock* clock_;
  BitrateObserver* observer_;
  int64_t last_bitrate_update_ms_;

  const rtc::scoped_ptr<CriticalSectionWrapper> critsect_;
  SendSideBandwidthEstimation bandwidth_estimation_ GUARDED_BY(*critsect_);
  uint32_t reserved_bitrate_bps_ GUARDED_BY(*critsect_);

  uint32_t last_bitrate_bps_ GUARDED_BY(*critsect_);
  uint8_t last_fraction_loss_ GUARDED_BY(*critsect_);
  int64_t last_rtt_ms_ GUARDED_BY(*critsect_);
  uint32_t last_reserved_bitrate_bps_ GUARDED_BY(*critsect_);

  DISALLOW_IMPLICIT_CONSTRUCTORS(BitrateControllerImpl);
};
}  // namespace webrtc
#endif  // WEBRTC_MODULES_BITRATE_CONTROLLER_BITRATE_CONTROLLER_IMPL_H_
