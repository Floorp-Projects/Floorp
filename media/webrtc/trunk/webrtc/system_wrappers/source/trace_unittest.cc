/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "gtest/gtest.h"
#include "webrtc/system_wrappers/interface/trace.h"
#include "webrtc/system_wrappers/source/cpu_measurement_harness.h"
#include "webrtc/test/testsupport/fileutils.h"

using webrtc::CpuMeasurementHarness;
using webrtc::Trace;
using webrtc::kTraceWarning;
using webrtc::kTraceUtility;

class Logger : public webrtc::CpuTarget {
 public:
  Logger() {
    Trace::CreateTrace();
    std::string trace_file = webrtc::test::OutputPath() + "trace_unittest.txt";
    Trace::SetTraceFile(trace_file.c_str());
    Trace::SetLevelFilter(webrtc::kTraceAll);
  }
  virtual ~Logger() {
    Trace::ReturnTrace();
  }

  virtual bool DoWork() {
    // Use input paremeters to WEBRTC_TRACE that are not likely to be removed
    // in future code. E.g. warnings will likely be kept and this file is in
    // utility so it should use kTraceUtility.
    WEBRTC_TRACE(kTraceWarning, kTraceUtility, 0, "Log line");
    return true;
  }
};

// This test is disabled because it measures CPU usage. This is flaky because
// the CPU usage for a machine may spike due to OS or other application.
TEST(TraceTest, DISABLED_CpuUsage) {
  Logger logger;
  const int periodicity_ms = 1;
  const int iterations_per_period = 10;
  const int duration_ms = 1000;
  CpuMeasurementHarness* cpu_harness =
    CpuMeasurementHarness::Create(&logger, periodicity_ms,
                                  iterations_per_period, duration_ms);
  cpu_harness->Run();
  const int average_cpu = cpu_harness->AverageCpu();
  EXPECT_GE(5, average_cpu);
}
