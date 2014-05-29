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
#include <map>
#include <utility>

#include "webrtc/modules/bitrate_controller/send_side_bandwidth_estimation.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

namespace webrtc {

class RtcpBandwidthObserverImpl;
class LowRateStrategy;

class BitrateControllerImpl : public BitrateController {
 public:
  friend class RtcpBandwidthObserverImpl;

  struct BitrateConfiguration {
    BitrateConfiguration(uint32_t start_bitrate,
                         uint32_t min_bitrate,
                         uint32_t max_bitrate)
        : start_bitrate_(start_bitrate),
          min_bitrate_(min_bitrate),
          max_bitrate_(max_bitrate) {
    }
    uint32_t start_bitrate_;
    uint32_t min_bitrate_;
    uint32_t max_bitrate_;
  };
  struct ObserverConfiguration {
    ObserverConfiguration(BitrateObserver* observer,
                          uint32_t bitrate)
        : observer_(observer),
          min_bitrate_(bitrate) {
    }
    BitrateObserver* observer_;
    uint32_t min_bitrate_;
  };
  typedef std::pair<BitrateObserver*, BitrateConfiguration*>
      BitrateObserverConfiguration;
  typedef std::list<BitrateObserverConfiguration> BitrateObserverConfList;

  explicit BitrateControllerImpl(bool enforce_min_bitrate);
  virtual ~BitrateControllerImpl();

  virtual bool AvailableBandwidth(uint32_t* bandwidth) const OVERRIDE;

  virtual RtcpBandwidthObserver* CreateRtcpBandwidthObserver() OVERRIDE;

  virtual void SetBitrateObserver(BitrateObserver* observer,
                                  const uint32_t start_bitrate,
                                  const uint32_t min_bitrate,
                                  const uint32_t max_bitrate) OVERRIDE;

  virtual void RemoveBitrateObserver(BitrateObserver* observer) OVERRIDE;

  virtual void EnforceMinBitrate(bool enforce_min_bitrate) OVERRIDE;

 private:
  // Called by BitrateObserver's direct from the RTCP module.
  void OnReceivedEstimatedBitrate(const uint32_t bitrate);

  void OnReceivedRtcpReceiverReport(const uint8_t fraction_loss,
                                    const uint32_t rtt,
                                    const int number_of_packets,
                                    const uint32_t now_ms);

  typedef std::multimap<uint32_t, ObserverConfiguration*> ObserverSortingMap;

  BitrateObserverConfList::iterator
      FindObserverConfigurationPair(const BitrateObserver* observer);
  void OnNetworkChanged(const uint32_t bitrate,
                        const uint8_t fraction_loss,  // 0 - 255.
                        const uint32_t rtt);

  CriticalSectionWrapper* critsect_;
  SendSideBandwidthEstimation bandwidth_estimation_;
  BitrateObserverConfList bitrate_observers_;
  scoped_ptr<LowRateStrategy> low_rate_strategy_;
};
}  // namespace webrtc
#endif  // WEBRTC_MODULES_BITRATE_CONTROLLER_BITRATE_CONTROLLER_IMPL_H_
