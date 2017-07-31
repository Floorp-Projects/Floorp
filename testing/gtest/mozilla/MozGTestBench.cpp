/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MozGTestBench.h"
#include "mozilla/TimeStamp.h"
#include <stdio.h>
#include <string>
#include <vector>

#define MOZ_GTEST_BENCH_FRAMEWORK "platform_microbench"
#define MOZ_GTEST_NUM_ITERATIONS 5

using mozilla::TimeStamp;

namespace mozilla {
void GTestBench(const char* aSuite, const char* aName,
                const std::function<void()>& aTest)
{
#ifdef DEBUG
  // Run the test to make sure that it doesn't fail but don't log
  // any measurements since it's not an optimized build.
  aTest();
#else
  bool shouldAlert = bool(getenv("PERFHERDER_ALERTING_ENABLED"));
  std::vector<int> durations;

  for (int i=0; i<MOZ_GTEST_NUM_ITERATIONS; i++) {
    mozilla::TimeStamp start = TimeStamp::Now();

    aTest();

    durations.push_back((TimeStamp::Now() - start).ToMicroseconds());
  }

  std::string replicatesStr = "[" + std::to_string(durations[0]);
  for (int i=1; i<MOZ_GTEST_NUM_ITERATIONS; i++) {
    replicatesStr += "," + std::to_string(durations[i]);
  }
  replicatesStr += "]";

  // median is at index floor(i/2) if number of replicates is odd,
  // (i/2-1) if even
  std::sort(durations.begin(), durations.end());
  int medianIndex = (MOZ_GTEST_NUM_ITERATIONS / 2) + ((durations.size() % 2 == 0) ? (-1) : 0);

  // Print the result for each test. Let perfherder aggregate for us
  printf("PERFHERDER_DATA: {\"framework\": {\"name\": \"%s\"}, "
         "\"suites\": [{\"name\": \"%s\", \"subtests\": "
         "[{\"name\": \"%s\", \"value\": %i, \"replicates\": %s, "
         "\"lowerIsBetter\": true, \"shouldAlert\": %s}]"
         "}]}\n",
         MOZ_GTEST_BENCH_FRAMEWORK, aSuite, aName, durations[medianIndex],
         replicatesStr.c_str(), shouldAlert ? "true" : "false");
#endif
}

} // mozilla

