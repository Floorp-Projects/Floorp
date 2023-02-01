/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_METRICS_GLOBAL_METRICS_LOGGER_AND_EXPORTER_H_
#define API_TEST_METRICS_GLOBAL_METRICS_LOGGER_AND_EXPORTER_H_

#include <memory>
#include <vector>

#include "api/test/metrics/metrics_exporter.h"
#include "api/test/metrics/metrics_logger_and_exporter.h"

namespace webrtc {
namespace test {

// Returns non-null global `MetricsLogger` to log metrics.
MetricsLogger* GetGlobalMetricsLogger();

bool ExportPerfMetric(MetricsLogger& logger,
                      std::vector<std::unique_ptr<MetricsExporter>> exporters);

// Returns current global `MetricsLoggerAndExporter`. Returns `nullptr` if there
// is global instance wasn't initialized.
MetricsLoggerAndExporter* GetGlobalMetricsLoggerAndExporter();

// Initialize global `MetricsLoggerAndExporter` with provided vector of
// exporters. Crashes if there is already initialized global instance.
void SetupGlobalMetricsLoggerAndExporter(
    std::vector<std::unique_ptr<MetricsExporter>> exporters);

// Destroys global `MetricsLoggerAndExporter` forcing it to export all collected
// metrics to the configured exporters. Crashes if there is no initialized
// global instance.
void ExportAndDestroyGlobalMetricsLoggerAndExporter();

}  // namespace test
}  // namespace webrtc

#endif  // API_TEST_METRICS_GLOBAL_METRICS_LOGGER_AND_EXPORTER_H_
