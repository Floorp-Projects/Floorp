/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/remote_bitrate_estimator/overuse_detector.h"

#include <algorithm>
#include <math.h>
#include <stdlib.h>

#include "webrtc/modules/remote_bitrate_estimator/include/bwe_defines.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
#include "webrtc/system_wrappers/interface/trace.h"

namespace webrtc {

enum { kOverUsingTimeThreshold = 100 };

OveruseDetector::OveruseDetector(const OverUseDetectorOptions& options)
    : options_(options),
      threshold_(options_.initial_threshold),
      prev_offset_(0.0),
      time_over_using_(-1),
      overuse_counter_(0),
      hypothesis_(kBwNormal) {}

OveruseDetector::~OveruseDetector() {}

BandwidthUsage OveruseDetector::State() const {
  return hypothesis_;
}


void OveruseDetector::SetRateControlRegion(RateControlRegion region) {
  switch (region) {
    case kRcMaxUnknown: {
      threshold_ = options_.initial_threshold;
      break;
    }
    case kRcAboveMax:
    case kRcNearMax: {
      threshold_ = options_.initial_threshold / 2;
      break;
    }
  }
}

BandwidthUsage OveruseDetector::Detect(double offset, double ts_delta,
                                       int num_of_deltas) {
  if (num_of_deltas < 2) {
    return kBwNormal;
  }
  const double prev_offset = prev_offset_;
  prev_offset_ = offset;
  const double T = std::min(num_of_deltas, 60) * offset;
  if (T > threshold_) {
    if (time_over_using_ == -1) {
      // Initialize the timer. Assume that we've been
      // over-using half of the time since the previous
      // sample.
      time_over_using_ = ts_delta / 2;
    } else {
      // Increment timer
      time_over_using_ += ts_delta;
    }
    overuse_counter_++;
    if (time_over_using_ > kOverUsingTimeThreshold
        && overuse_counter_ > 1) {
      if (offset >= prev_offset) {
        time_over_using_ = 0;
        overuse_counter_ = 0;
        hypothesis_ = kBwOverusing;
      }
    }
  } else if (T < -threshold_) {
    time_over_using_ = -1;
    overuse_counter_ = 0;
    hypothesis_ = kBwUnderusing;
  } else {
    time_over_using_ = -1;
    overuse_counter_ = 0;
    hypothesis_ = kBwNormal;
  }
  return hypothesis_;
}
}  // namespace webrtc
