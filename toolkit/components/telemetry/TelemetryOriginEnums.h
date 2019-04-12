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
  ContentBlocking_Blocked_TestOnly = 1,
  ContentBlocking_StorageAccessAPIExempt = 2,
  ContentBlocking_StorageAccessAPIExempt_TestOnly = 3,
  ContentBlocking_OpenerAfterUserInteractionExempt = 4,
  ContentBlocking_OpenerAfterUserInteractionExempt_TestOnly = 5,
  ContentBlocking_OpenerExempt = 6,
  ContentBlocking_OpenerExempt_TestOnly = 7,
  // category: telemetry.test
  TelemetryTest_Test1 = 8,
  TelemetryTest_Test2 = 9,
  // meta
  Count = 10,
};

static const char* const MetricIDToString[10] = {
    "content.blocking_blocked",
    "content.blocking_blocked_TESTONLY",
    "content.blocking_storage_access_api_exempt",
    "content.blocking_storage_access_api_exempt_TESTONLY",
    "content.blocking_opener_after_user_interaction_exempt",
    "content.blocking_opener_after_user_interaction_exempt_TESTONLY",
    "content.blocking_opener_exempt",
    "content.blocking_opener_exempt_TESTONLY",
    "telemetry.test_test1",
    "telemetry.test_test2",
};

}  // namespace Telemetry
}  // namespace mozilla

#endif  // mozilla_TelemetryOriginEnums_h
