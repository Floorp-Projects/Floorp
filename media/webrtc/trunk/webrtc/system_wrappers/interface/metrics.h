//
// Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
//

#ifndef WEBRTC_SYSTEM_WRAPPERS_INTERFACE_METRICS_H_
#define WEBRTC_SYSTEM_WRAPPERS_INTERFACE_METRICS_H_

#include <string>

#include "webrtc/common_types.h"

// Macros for allowing WebRTC clients (e.g. Chrome) to gather and aggregate
// statistics.
//
// Histogram for counters.
// RTC_HISTOGRAM_COUNTS(name, sample, min, max, bucket_count);
//
// Histogram for enumerators.
// The boundary should be above the max enumerator sample.
// RTC_HISTOGRAM_ENUMERATION(name, sample, boundary);
//
//
// The macros use the methods HistogramFactoryGetCounts,
// HistogramFactoryGetEnumeration and HistogramAdd.
//
// Therefore, WebRTC clients must either:
//
// - provide implementations of
//   Histogram* webrtc::metrics::HistogramFactoryGetCounts(
//       const std::string& name, int sample, int min, int max,
//       int bucket_count);
//   Histogram* webrtc::metrics::HistogramFactoryGetEnumeration(
//       const std::string& name, int sample, int boundary);
//   void webrtc::metrics::HistogramAdd(
//       Histogram* histogram_pointer, const std::string& name, int sample);
//
// - or link with the default implementations (i.e.
//   system_wrappers/source/system_wrappers.gyp:metrics_default).
//
//
// Example usage:
//
// RTC_HISTOGRAM_COUNTS("WebRTC.Video.NacksSent", nacks_sent, 1, 100000, 100);
//
// enum Types {
//   kTypeX,
//   kTypeY,
//   kBoundary,
// };
//
// RTC_HISTOGRAM_ENUMERATION("WebRTC.Types", kTypeX, kBoundary);


// Macros for adding samples to a named histogram.
//
// NOTE: this is a temporary solution.
// The aim is to mimic the behaviour in Chromium's src/base/metrics/histograms.h
// However as atomics are not supported in webrtc, this is for now a modified
// and temporary solution. Note that the histogram is constructed/found for
// each call. Therefore, for now only use this implementation for metrics
// that do not need to be updated frequently.
// TODO(asapersson): Change implementation when atomics are supported.
// Also consider changing string to const char* when switching to atomics.

// Histogram for counters.
#define RTC_HISTOGRAM_COUNTS_100(name, sample) RTC_HISTOGRAM_COUNTS( \
    name, sample, 1, 100, 50)

#define RTC_HISTOGRAM_COUNTS_1000(name, sample) RTC_HISTOGRAM_COUNTS( \
    name, sample, 1, 1000, 50)

#define RTC_HISTOGRAM_COUNTS_10000(name, sample) RTC_HISTOGRAM_COUNTS( \
    name, sample, 1, 10000, 50)

#define RTC_HISTOGRAM_COUNTS(name, sample, min, max, bucket_count) \
    RTC_HISTOGRAM_COMMON_BLOCK(name, sample, \
        webrtc::metrics::HistogramFactoryGetCounts( \
            name, min, max, bucket_count))

// Histogram for percentage.
#define RTC_HISTOGRAM_PERCENTAGE(name, sample) \
    RTC_HISTOGRAM_ENUMERATION(name, sample, 101)

// Histogram for enumerators.
// |boundary| should be above the max enumerator sample.
#define RTC_HISTOGRAM_ENUMERATION(name, sample, boundary) \
    RTC_HISTOGRAM_COMMON_BLOCK(name, sample, \
        webrtc::metrics::HistogramFactoryGetEnumeration(name, boundary))

#define RTC_HISTOGRAM_COMMON_BLOCK(constant_name, sample, \
                                   factory_get_invocation) \
  do { \
    webrtc::metrics::Histogram* histogram_pointer = factory_get_invocation; \
    webrtc::metrics::HistogramAdd(histogram_pointer, constant_name, sample); \
  } while (0)


namespace webrtc {
namespace metrics {

class Histogram;

// Functions for getting pointer to histogram (constructs or finds the named
// histogram).

// Get histogram for counters.
Histogram* HistogramFactoryGetCounts(
    const std::string& name, int min, int max, int bucket_count);

// Get histogram for enumerators.
// |boundary| should be above the max enumerator sample.
Histogram* HistogramFactoryGetEnumeration(
    const std::string& name, int boundary);

// Function for adding a |sample| to a histogram.
// |name| can be used to verify that it matches the histogram name.
void HistogramAdd(
    Histogram* histogram_pointer, const std::string& name, int sample);

}  // namespace metrics
}  // namespace webrtc

#endif  // WEBRTC_SYSTEM_WRAPPERS_INTERFACE_METRICS_H_

