/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/test/histogram.h"

#include <map>

#include "webrtc/system_wrappers/interface/metrics.h"

// Test implementation of histogram methods in
// webrtc/system_wrappers/interface/metrics.h.

namespace webrtc {
namespace {
// Map holding the last added sample to a histogram (mapped by histogram name).
std::map<std::string, int> histograms_;
}  // namespace

namespace metrics {
Histogram* HistogramFactoryGetCounts(const std::string& name, int min, int max,
    int bucket_count) { return NULL; }

Histogram* HistogramFactoryGetEnumeration(const std::string& name,
    int boundary) { return NULL; }

void HistogramAdd(
    Histogram* histogram_pointer, const std::string& name, int sample) {
  histograms_[name] = sample;
}
}  // namespace metrics

namespace test {
int LastHistogramSample(const std::string& name) {
  std::map<std::string, int>::const_iterator it = histograms_.find(name);
  if (it == histograms_.end()) {
    return -1;
  }
  return it->second;
}
}  // namespace test
}  // namespace webrtc

