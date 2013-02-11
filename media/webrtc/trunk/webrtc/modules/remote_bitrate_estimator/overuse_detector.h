/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_OVERUSE_DETECTOR_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_OVERUSE_DETECTOR_H_

#include <list>

#include "modules/interface/module_common_types.h"
#include "modules/remote_bitrate_estimator/include/bwe_defines.h"
#include "typedefs.h"  // NOLINT(build/include)

#ifdef WEBRTC_BWE_MATLAB
#include "../test/BWEStandAlone/MatlabPlot.h"
#endif

namespace webrtc {
enum RateControlRegion;

class OveruseDetector {
 public:
  explicit OveruseDetector(const OverUseDetectorOptions& options);
  ~OveruseDetector();
  void Update(uint16_t packet_size,
              int64_t timestamp_ms,
              uint32_t rtp_timestamp,
              int64_t now_ms);
  BandwidthUsage State() const;
  double NoiseVar() const;
  void SetRateControlRegion(RateControlRegion region);

 private:
  struct FrameSample {
    FrameSample()
        : size(0),
          complete_time_ms(-1),
          timestamp(-1),
          timestamp_ms(-1) {}

    uint32_t size;
    int64_t complete_time_ms;
    int64_t timestamp;
    int64_t timestamp_ms;
  };

  struct DebugPlots {
#ifdef WEBRTC_BWE_MATLAB
    DebugPlots() : plot1(NULL), plot2(NULL), plot3(NULL), plot4(NULL) {}
    MatlabPlot* plot1;
    MatlabPlot* plot2;
    MatlabPlot* plot3;
    MatlabPlot* plot4;
#endif
  };

  // Returns true if |timestamp| represent a time which is later than
  // |prev_timestamp|.
  static bool InOrderTimestamp(uint32_t timestamp, uint32_t prev_timestamp);

  bool PacketInOrder(uint32_t timestamp, int64_t timestamp_ms);

  // Prepares the overuse detector to start using timestamps in milliseconds
  // instead of 90 kHz timestamps.
  void SwitchTimeBase();

  void TimeDeltas(const FrameSample& current_frame,
                  const FrameSample& prev_frame,
                  int64_t* t_delta,
                  double* ts_delta);
  void UpdateKalman(int64_t t_delta,
                    double ts_elta,
                    uint32_t frame_size,
                    uint32_t prev_frame_size);
  double UpdateMinFramePeriod(double ts_delta);
  void UpdateNoiseEstimate(double residual, double ts_delta, bool stable_state);
  BandwidthUsage Detect(double ts_delta);
  double CurrentDrift();

  OverUseDetectorOptions options_;  // Must be first member
                                    // variable. Cannot be const
                                    // because we need to be copyable.
  FrameSample current_frame_;
  FrameSample prev_frame_;
  uint16_t num_of_deltas_;
  double slope_;
  double offset_;
  double E_[2][2];
  double process_noise_[2];
  double avg_noise_;
  double var_noise_;
  double threshold_;
  std::list<double> ts_delta_hist_;
  double prev_offset_;
  double time_over_using_;
  uint16_t over_use_counter_;
  BandwidthUsage hypothesis_;
#ifdef WEBRTC_BWE_MATLAB
  DebugPlots plots_;
#endif
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_OVERUSE_DETECTOR_H_
