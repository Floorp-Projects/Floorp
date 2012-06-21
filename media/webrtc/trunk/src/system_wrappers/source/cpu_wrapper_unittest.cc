/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "system_wrappers/interface/cpu_wrapper.h"

#include "gtest/gtest.h"
#include "system_wrappers/interface/cpu_info.h"
#include "system_wrappers/interface/event_wrapper.h"
#include "system_wrappers/interface/scoped_ptr.h"
#include "system_wrappers/interface/trace.h"
#include "testsupport/fileutils.h"

using webrtc::CpuInfo;
using webrtc::CpuWrapper;
using webrtc::EventWrapper;
using webrtc::scoped_ptr;
using webrtc::Trace;

// This test is flaky on Windows/Release.
// http://code.google.com/p/webrtc/issues/detail?id=290
#ifdef _WIN32
#define MAYBE_Usage DISABLED_Usage
#else
#define MAYBE_Usage Usage
#endif
TEST(CpuWrapperTest, MAYBE_Usage) {
  Trace::CreateTrace();
  std::string trace_file = webrtc::test::OutputPath() +
      "cpu_wrapper_unittest.txt";
  Trace::SetTraceFile(trace_file.c_str());
  Trace::SetLevelFilter(webrtc::kTraceAll);
  printf("Number of cores detected:%u\n", CpuInfo::DetectNumberOfCores());
  scoped_ptr<CpuWrapper> cpu(CpuWrapper::CreateCpu());
  ASSERT_TRUE(cpu.get() != NULL);
  scoped_ptr<EventWrapper> sleep_event(EventWrapper::Create());
  ASSERT_TRUE(sleep_event.get() != NULL);

  int num_iterations = 0;
  WebRtc_UWord32 num_cores = 0;
  WebRtc_UWord32* cores = NULL;
  bool cpu_usage_available = cpu->CpuUsageMultiCore(num_cores, cores) != -1;
  // Initializing the CPU measurements may take a couple of seconds on Windows.
  // Since the initialization is lazy we need to wait until it is completed.
  // Should not take more than 10000 ms.
  while (!cpu_usage_available && (++num_iterations < 10000)) {
    if (cores != NULL) {
      ASSERT_GT(num_cores, 0u);
      break;
    }
    sleep_event->Wait(1);
    cpu_usage_available = cpu->CpuUsageMultiCore(num_cores, cores) != -1;
  }
  ASSERT_TRUE(cpu_usage_available);

  const WebRtc_Word32 average = cpu->CpuUsageMultiCore(num_cores, cores);
  ASSERT_TRUE(cores != NULL);
  EXPECT_GT(num_cores, 0u);
  EXPECT_GE(average, 0);
  EXPECT_LE(average, 100);

  printf("\nNumber of cores:%d\n", num_cores);
  printf("Average cpu:%d\n", average);
  for (WebRtc_UWord32 i = 0; i < num_cores; i++) {
    printf("Core:%u CPU:%u \n", i, cores[i]);
    EXPECT_GE(cores[i], 0u);
    EXPECT_LE(cores[i], 100u);
  }

  Trace::ReturnTrace();
};
