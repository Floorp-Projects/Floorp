/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "api/test/metrics/global_metrics_logger_and_exporter.h"

#include <memory>
#include <utility>
#include <vector>

#include "api/test/metrics/metrics_exporter.h"
#include "api/test/metrics/metrics_logger_and_exporter.h"
#include "rtc_base/checks.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {
namespace test {
namespace {

MetricsLoggerAndExporter* global_metrics_logger_and_exporter = nullptr;

}  // namespace

MetricsLoggerAndExporter* GetGlobalMetricsLoggerAndExporter() {
  return global_metrics_logger_and_exporter;
}

void SetupGlobalMetricsLoggerAndExporter(
    std::vector<std::unique_ptr<MetricsExporter>> exporters) {
  RTC_CHECK(global_metrics_logger_and_exporter == nullptr);
  global_metrics_logger_and_exporter = new MetricsLoggerAndExporter(
      Clock::GetRealTimeClock(), std::move(exporters));
}

void ExportAndDestroyGlobalMetricsLoggerAndExporter() {
  RTC_CHECK(global_metrics_logger_and_exporter != nullptr);
  delete global_metrics_logger_and_exporter;
}

}  // namespace test
}  // namespace webrtc
