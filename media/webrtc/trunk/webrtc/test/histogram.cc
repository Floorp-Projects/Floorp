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

#include "webrtc/base/checks.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/system_wrappers/include/metrics.h"

// Test implementation of histogram methods in
// webrtc/system_wrappers/include/metrics.h.

namespace webrtc {
namespace {
struct SampleInfo {
  SampleInfo(const std::string& name) : name_(name), last_(-1), total_(0) {}
  const std::string name_;
  int last_;   // Last added sample.
  int total_;  // Total number of added samples.
};

rtc::CriticalSection histogram_crit_;
// Map holding info about added samples to a histogram (mapped by the histogram
// name).
std::map<std::string, SampleInfo> histograms_ GUARDED_BY(histogram_crit_);
}  // namespace

namespace metrics {
Histogram* HistogramFactoryGetCounts(const std::string& name, int min, int max,
                                     int bucket_count) {
  rtc::CritScope cs(&histogram_crit_);
  if (histograms_.find(name) == histograms_.end()) {
    histograms_.insert(std::make_pair(name, SampleInfo(name)));
  }
  auto it = histograms_.find(name);
  return reinterpret_cast<Histogram*>(&it->second);
}

Histogram* HistogramFactoryGetEnumeration(const std::string& name,
                                          int boundary) {
  rtc::CritScope cs(&histogram_crit_);
  if (histograms_.find(name) == histograms_.end()) {
    histograms_.insert(std::make_pair(name, SampleInfo(name)));
  }
  auto it = histograms_.find(name);
  return reinterpret_cast<Histogram*>(&it->second);
}

void HistogramAdd(
    Histogram* histogram_pointer, const std::string& name, int sample) {
  rtc::CritScope cs(&histogram_crit_);
  SampleInfo* ptr = reinterpret_cast<SampleInfo*>(histogram_pointer);
  // The name should not vary.
  RTC_CHECK(ptr->name_ == name);
  ptr->last_ = sample;
  ++ptr->total_;
}
}  // namespace metrics

namespace test {
int LastHistogramSample(const std::string& name) {
  rtc::CritScope cs(&histogram_crit_);
  const auto it = histograms_.find(name);
  if (it == histograms_.end()) {
    return -1;
  }
  return it->second.last_;
}

int NumHistogramSamples(const std::string& name) {
  rtc::CritScope cs(&histogram_crit_);
  const auto it = histograms_.find(name);
  if (it == histograms_.end()) {
    return 0;
  }
  return it->second.total_;
}

void ClearHistograms() {
  rtc::CritScope cs(&histogram_crit_);
  for (auto& it : histograms_) {
    it.second.last_ = -1;
    it.second.total_ = 0;
  }
}
}  // namespace test
}  // namespace webrtc
