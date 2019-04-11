/* This file might later be auto-generated */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_TelemetryOriginEnums_h
#define mozilla_TelemetryOriginEnums_h

#include <stdint.h>

namespace mozilla {
namespace Telemetry {

enum class OriginMetricID : uint32_t {
  // category: content.blocking
  ContentBlocking_Blocked = 0,
  ContentBlocking_Exempt = 1,
  ContentBlocking_Blocked_TestOnly = 2,
  ContentBlocking_Exempt_TestOnly = 3,
  // category: telemetry.test
  TelemetryTest_Test1 = 4,
  TelemetryTest_Test2 = 5,
  // meta
  Count = 6,
};

static const char* const MetricIDToString[6] = {
    "content.blocking_blocked",
    "content.blocking_exempt",
    "content.blocking_blocked_TESTONLY",
    "content.blocking_exempt_TESTONLY",
    "telemetry.test_test1",
    "telemetry.test_test2",
};

}  // namespace Telemetry
}  // namespace mozilla

#endif  // mozilla_TelemetryOriginEnums_h
