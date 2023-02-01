/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "api/test/metrics/print_result_proxy_metrics_exporter.h"

#include <string>

#include "api/array_view.h"
#include "api/test/metrics/metric.h"
#include "test/testsupport/perf_test.h"

namespace webrtc {
namespace test {
namespace {

std::string ToPrintResultUnit(Unit unit) {
  switch (unit) {
    case Unit::kMilliseconds:
      return "msBestFitFormat";
    case Unit::kPercent:
      return "n%";
    case Unit::kBytes:
      return "sizeInBytes";
    case Unit::kKilobitsPerSecond:
      // PrintResults prefer Chrome Perf Dashboard units, which doesn't have
      // kpbs units, so we change the unit and value accordingly.
      return "bytesPerSecond";
    case Unit::kHertz:
      return "Hz";
    case Unit::kUnitless:
      return "unitless";
    case Unit::kCount:
      return "count";
  }
}

double ToPrintResultValue(double value, Unit unit) {
  switch (unit) {
    case Unit::kKilobitsPerSecond:
      // PrintResults prefer Chrome Perf Dashboard units, which doesn't have
      // kpbs units, so we change the unit and value accordingly.
      return value * 1000 / 8;
    default:
      return value;
  }
}

ImproveDirection ToPrintResultImproveDirection(ImprovementDirection direction) {
  switch (direction) {
    case ImprovementDirection::kBiggerIsBetter:
      return ImproveDirection::kBiggerIsBetter;
    case ImprovementDirection::kNeitherIsBetter:
      return ImproveDirection::kNone;
    case ImprovementDirection::kSmallerIsBetter:
      return ImproveDirection::kSmallerIsBetter;
  }
}

bool IsEmpty(const Metric::Stats& stats) {
  return !stats.mean.has_value() && !stats.stddev.has_value() &&
         !stats.min.has_value() && !stats.max.has_value();
}

}  // namespace

bool PrintResultProxyMetricsExporter::Export(
    rtc::ArrayView<const Metric> metrics) {
  for (const Metric& metric : metrics) {
    if (metric.time_series.samples.empty() && IsEmpty(metric.stats)) {
      // If there were no data collected for the metric it is expected that 0
      // will be exported, so add 0 to the samples.
      PrintResult(metric.name, /*modifier=*/"", metric.test_case,
                  ToPrintResultValue(0, metric.unit),
                  ToPrintResultUnit(metric.unit), /*important=*/false,
                  ToPrintResultImproveDirection(metric.improvement_direction));
      continue;
    }

    if (metric.time_series.samples.empty()) {
      PrintResultMeanAndError(
          metric.name, /*modifier=*/"", metric.test_case,
          ToPrintResultValue(*metric.stats.mean, metric.unit),
          ToPrintResultValue(*metric.stats.stddev, metric.unit),
          ToPrintResultUnit(metric.unit),
          /*important=*/false,
          ToPrintResultImproveDirection(metric.improvement_direction));
      continue;
    }

    SamplesStatsCounter counter;
    for (size_t i = 0; i < metric.time_series.samples.size(); ++i) {
      counter.AddSample(SamplesStatsCounter::StatsSample{
          .value = ToPrintResultValue(metric.time_series.samples[i].value,
                                      metric.unit),
          .time = metric.time_series.samples[i].timestamp,
          .metadata = metric.time_series.samples[i].sample_metadata});
    }

    PrintResult(metric.name, /*modifier=*/"", metric.test_case, counter,
                ToPrintResultUnit(metric.unit),
                /*important=*/false,
                ToPrintResultImproveDirection(metric.improvement_direction));
  }
  return true;
}

}  // namespace test
}  // namespace webrtc
