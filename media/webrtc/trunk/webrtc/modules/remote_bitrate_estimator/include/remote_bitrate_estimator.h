/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This class estimates the incoming available bandwidth.

#ifndef WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_INCLUDE_REMOTE_BITRATE_ESTIMATOR_H_
#define WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_INCLUDE_REMOTE_BITRATE_ESTIMATOR_H_

#include <map>
#include <vector>

#include "webrtc/common_types.h"
#include "webrtc/modules/interface/module.h"
#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class Clock;

// RemoteBitrateObserver is used to signal changes in bitrate estimates for
// the incoming streams.
class RemoteBitrateObserver {
 public:
  // Called when a receive channel group has a new bitrate estimate for the
  // incoming streams.
  virtual void OnReceiveBitrateChanged(std::vector<unsigned int>* ssrcs,
                                       unsigned int bitrate) = 0;

  virtual ~RemoteBitrateObserver() {}
};

class RemoteBitrateEstimator : public CallStatsObserver, public Module {
 public:
  enum EstimationMode {
    kMultiStreamEstimation,
    kSingleStreamEstimation
  };

  virtual ~RemoteBitrateEstimator() {}

  static RemoteBitrateEstimator* Create(const OverUseDetectorOptions& options,
                                        EstimationMode mode,
                                        RemoteBitrateObserver* observer,
                                        Clock* clock);

  // Stores an RTCP SR (NTP, RTP timestamp) tuple for a specific SSRC to be used
  // in future RTP timestamp to NTP time conversions. As soon as any SSRC has
  // two tuples the RemoteBitrateEstimator will switch to multi-stream mode.
  virtual void IncomingRtcp(unsigned int ssrc, uint32_t ntp_secs,
                            uint32_t ntp_frac, uint32_t rtp_timestamp) = 0;

  // Called for each incoming packet. Updates the incoming payload bitrate
  // estimate and the over-use detector. If an over-use is detected the
  // remote bitrate estimate will be updated. Note that |payload_size| is the
  // packet size excluding headers.
  virtual void IncomingPacket(unsigned int ssrc,
                              int payload_size,
                              int64_t arrival_time,
                              uint32_t rtp_timestamp) = 0;

  // Removes all data for |ssrc|.
  virtual void RemoveStream(unsigned int ssrc) = 0;

  // Returns true if a valid estimate exists and sets |bitrate_bps| to the
  // estimated payload bitrate in bits per second. |ssrcs| is the list of ssrcs
  // currently being received and of which the bitrate estimate is based upon.
  virtual bool LatestEstimate(std::vector<unsigned int>* ssrcs,
                              unsigned int* bitrate_bps) const = 0;

 protected:
  static const int kProcessIntervalMs = 1000;
  static const int kStreamTimeOutMs = 2000;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_INCLUDE_REMOTE_BITRATE_ESTIMATOR_H_
