/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MozGTestBench.h"
#include "mozilla/TimeStamp.h"
#include <stdio.h>

#define MOZ_GTEST_BENCH_FRAMEWORK "platform_microbench"

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
  mozilla::TimeStamp start = TimeStamp::Now();

  aTest();

  int msDuration = (TimeStamp::Now() - start).ToMicroseconds();

  // Print the result for each test. Let perfherder aggregate for us
  printf("PERFHERDER_DATA: {\"framework\": {\"name\": \"%s\"}, "
                           "\"suites\": [{\"name\": \"%s\", \"subtests\": "
                               "[{\"name\": \"%s\", \"value\": %i, \"lowerIsBetter\": true}]"
                           "}]}\n",
                           MOZ_GTEST_BENCH_FRAMEWORK, aSuite, aName, msDuration);
#endif
}

} // mozilla

