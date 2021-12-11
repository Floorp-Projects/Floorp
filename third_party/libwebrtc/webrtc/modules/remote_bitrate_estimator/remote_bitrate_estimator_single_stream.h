/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_REMOTE_BITRATE_ESTIMATOR_REMOTE_BITRATE_ESTIMATOR_SINGLE_STREAM_H_
#define MODULES_REMOTE_BITRATE_ESTIMATOR_REMOTE_BITRATE_ESTIMATOR_SINGLE_STREAM_H_

#include <map>
#include <memory>
#include <vector>

#include "modules/remote_bitrate_estimator/aimd_rate_control.h"
#include "modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/rate_statistics.h"

namespace webrtc {

class RemoteBitrateEstimatorSingleStream : public RemoteBitrateEstimator {
 public:
  RemoteBitrateEstimatorSingleStream(RemoteBitrateObserver* observer,
                                     const Clock* clock);
  virtual ~RemoteBitrateEstimatorSingleStream();

  void IncomingPacket(int64_t arrival_time_ms,
                      size_t payload_size,
                      const RTPHeader& header) override;
  void Process() override;
  int64_t TimeUntilNextProcess() override;
  void OnRttUpdate(int64_t avg_rtt_ms, int64_t max_rtt_ms) override;
  void RemoveStream(uint32_t ssrc) override;
  bool LatestEstimate(std::vector<uint32_t>* ssrcs,
                      uint32_t* bitrate_bps) const override;
  void SetMinBitrate(int min_bitrate_bps) override;

 private:
  struct Detector;

  typedef std::map<uint32_t, Detector*> SsrcOveruseEstimatorMap;

  // Triggers a new estimate calculation.
  void UpdateEstimate(int64_t time_now)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);

  void GetSsrcs(std::vector<uint32_t>* ssrcs) const
      RTC_SHARED_LOCKS_REQUIRED(crit_sect_);

  // Returns |remote_rate_| if the pointed to object exists,
  // otherwise creates it.
  AimdRateControl* GetRemoteRate() RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);

  const Clock* const clock_;
  SsrcOveruseEstimatorMap overuse_detectors_ RTC_GUARDED_BY(crit_sect_);
  RateStatistics incoming_bitrate_ RTC_GUARDED_BY(crit_sect_);
  uint32_t last_valid_incoming_bitrate_ RTC_GUARDED_BY(crit_sect_);
  std::unique_ptr<AimdRateControl> remote_rate_ RTC_GUARDED_BY(crit_sect_);
  RemoteBitrateObserver* const observer_ RTC_GUARDED_BY(crit_sect_);
  rtc::CriticalSection crit_sect_;
  int64_t last_process_time_;
  int64_t process_interval_ms_ RTC_GUARDED_BY(crit_sect_);
  bool uma_recorded_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(RemoteBitrateEstimatorSingleStream);
};

}  // namespace webrtc

#endif  // MODULES_REMOTE_BITRATE_ESTIMATOR_REMOTE_BITRATE_ESTIMATOR_SINGLE_STREAM_H_
