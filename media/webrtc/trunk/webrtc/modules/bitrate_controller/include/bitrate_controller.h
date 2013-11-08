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
 *  and push the result to the encoders via BitrateObserver(s).
 */

#ifndef WEBRTC_MODULES_BITRATE_CONTROLLER_INCLUDE_BITRATE_CONTROLLER_H_
#define WEBRTC_MODULES_BITRATE_CONTROLLER_INCLUDE_BITRATE_CONTROLLER_H_

#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"

namespace webrtc {

class BitrateObserver {
 /*
  * Observer class for the encoders, each encoder should implement this class
  * to get the target bitrate. It also get the fraction loss and rtt to
  * optimize its settings for this type of network. |target_bitrate| is the
  * target media/payload bitrate excluding packet headers, measured in bits
  * per second.
  */
 public:
  virtual void OnNetworkChanged(const uint32_t target_bitrate,
                                const uint8_t fraction_loss,  // 0 - 255.
                                const uint32_t rtt) = 0;

  virtual ~BitrateObserver() {}
};

class BitrateController {
/*
 * This class collects feedback from all streams sent to a peer (via
 * RTCPBandwidthObservers). It does one  aggregated send side bandwidth
 * estimation and divide the available bitrate between all its registered
 * BitrateObservers.
 */
 public:
  static BitrateController* CreateBitrateController();
  virtual ~BitrateController() {}

  virtual RtcpBandwidthObserver* CreateRtcpBandwidthObserver() = 0;

  // Gets the available payload bandwidth in bits per second. Note that
  // this bandwidth excludes packet headers.
  virtual bool AvailableBandwidth(uint32_t* bandwidth) const = 0;

  /*
  *  Set the start and max send bitrate used by the bandwidth management.
  *
  *  observer, updates bitrates if already in use.
  *  min_bitrate_kbit = 0 equals no min bitrate.
  *  max_bitrate_kit = 0 equals no max bitrate.
  */
  virtual void SetBitrateObserver(BitrateObserver* observer,
                                  const uint32_t start_bitrate,
                                  const uint32_t min_bitrate,
                                  const uint32_t max_bitrate) = 0;

  virtual void RemoveBitrateObserver(BitrateObserver* observer) = 0;
};
}  // namespace webrtc
#endif  // WEBRTC_MODULES_BITRATE_CONTROLLER_INCLUDE_BITRATE_CONTROLLER_H_
