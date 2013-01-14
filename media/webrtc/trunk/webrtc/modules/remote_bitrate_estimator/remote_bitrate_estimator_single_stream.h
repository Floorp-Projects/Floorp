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

#ifndef WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_INCLUDE_REMOTE_BITRATE_ESTIMATOR_SINGLE_STREAM_H_
#define WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_INCLUDE_REMOTE_BITRATE_ESTIMATOR_SINGLE_STREAM_H_

#include <map>

#include "modules/remote_bitrate_estimator/bitrate_estimator.h"
#include "modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "modules/remote_bitrate_estimator/overuse_detector.h"
#include "modules/remote_bitrate_estimator/remote_rate_control.h"
#include "system_wrappers/interface/critical_section_wrapper.h"
#include "system_wrappers/interface/scoped_ptr.h"
#include "typedefs.h"

namespace webrtc {

class RemoteBitrateEstimatorSingleStream : public RemoteBitrateEstimator {
 public:
  RemoteBitrateEstimatorSingleStream(RemoteBitrateObserver* observer,
                                     const OverUseDetectorOptions& options);

  void IncomingRtcp(unsigned int ssrc, uint32_t ntp_secs, uint32_t ntp_frac,
                    uint32_t rtp_timestamp) {}

  // Called for each incoming packet. If this is a new SSRC, a new
  // BitrateControl will be created. Updates the incoming payload bitrate
  // estimate and the over-use detector. If an over-use is detected the
  // remote bitrate estimate will be updated. Note that |payload_size| is the
  // packet size excluding headers.
  void IncomingPacket(unsigned int ssrc,
                      int payload_size,
                      int64_t arrival_time,
                      uint32_t rtp_timestamp);

  // Triggers a new estimate calculation for the stream identified by |ssrc|.
  void UpdateEstimate(unsigned int ssrc, int64_t time_now);

  // Set the current round-trip time experienced by the stream identified by
  // |ssrc|.
  void SetRtt(unsigned int ssrc);

  // Removes all data for |ssrc|.
  void RemoveStream(unsigned int ssrc);

  // Returns true if a valid estimate exists and sets |bitrate_bps| to the
  // estimated payload bitrate in bits per second. |ssrcs| is the list of ssrcs
  // currently being received and of which the bitrate estimate is based upon.
  bool LatestEstimate(std::vector<unsigned int>* ssrcs,
                      unsigned int* bitrate_bps) const;

 private:
  typedef std::map<unsigned int, OveruseDetector> SsrcOveruseDetectorMap;

  void GetSsrcs(std::vector<unsigned int>* ssrcs) const;

  const OverUseDetectorOptions& options_;
  SsrcOveruseDetectorMap overuse_detectors_;
  BitRateStats incoming_bitrate_;
  RemoteRateControl remote_rate_;
  RemoteBitrateObserver* observer_;
  scoped_ptr<CriticalSectionWrapper> crit_sect_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_INCLUDE_REMOTE_BITRATE_ESTIMATOR_SINGLE_STREAM_H_
