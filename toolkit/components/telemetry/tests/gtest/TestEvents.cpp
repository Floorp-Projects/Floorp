/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#include "core/TelemetryEvent.h"
#include "gtest/gtest.h"
#include "mozilla/Maybe.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Unused.h"
#include "TelemetryFixture.h"
#include "TelemetryTestHelpers.h"

using namespace mozilla;
using namespace TelemetryTestHelpers;

namespace mozilla {
namespace Telemetry {

struct EventExtraEntry {
  nsCString key;
  nsCString value;
};

}  // namespace Telemetry
}  // namespace mozilla

// Test that we can properly record events using the C++ API.
TEST_F(TelemetryTestFixture, RecordEventNative) {
  AutoJSContextWithGlobal cx(mCleanGlobal);

  // Make sure we don't get events from other tests.
  Unused << mTelemetry->ClearEvents();

  const nsLiteralCString category("telemetry.test");
  const nsLiteralCString method("test1");
  const nsLiteralCString method2("test2");
  const nsLiteralCString object("object1");
  const nsLiteralCString object2("object2");
  const nsLiteralCString value("value");
  const nsLiteralCString valueLong(
      "this value is much too long and must be truncated to fit in the limit "
      "which at time of writing was 80 bytes.");
  const nsLiteralCString extraKey("key1");
  const nsLiteralCString extraValue("extra value");
  const nsLiteralCString extraValueLong(
      "this extra value is much too long and must be truncated to fit in the "
      "limit which at time of writing was 80 bytes.");

  // Try recording before category's enabled.
  Telemetry::RecordEvent(Telemetry::EventID::TelemetryTest_Test1_Object1,
                         Nothing(), Nothing());

  // Ensure "telemetry.test" is enabled
  Telemetry::SetEventRecordingEnabled(category, true);

  // Try recording after it's enabled.
  Telemetry::RecordEvent(Telemetry::EventID::TelemetryTest_Test2_Object1,
                         Nothing(), Nothing());

  // Try recording with normal value, extra
  nsTArray<EventExtraEntry> extra({EventExtraEntry{extraKey, extraValue}});
  Telemetry::RecordEvent(Telemetry::EventID::TelemetryTest_Test1_Object2,
                         mozilla::Some(value), mozilla::Some(extra));

  // Try recording with too-long value, extra
  nsTArray<EventExtraEntry> longish(
      {EventExtraEntry{extraKey, extraValueLong}});
  Telemetry::RecordEvent(Telemetry::EventID::TelemetryTest_Test2_Object2,
                         mozilla::Some(valueLong), mozilla::Some(longish));

  JS::RootedValue eventsSnapshot(cx.GetJSContext());
  GetEventSnapshot(cx.GetJSContext(), &eventsSnapshot);

  ASSERT_TRUE(!EventPresent(cx.GetJSContext(), eventsSnapshot, category, method,
                            object))
  << "Test event must not be present when recorded before enabled.";
  ASSERT_TRUE(EventPresent(cx.GetJSContext(), eventsSnapshot, category, method2,
                           object))
  << "Test event must be present.";
  ASSERT_TRUE(EventPresent(cx.GetJSContext(), eventsSnapshot, category, method,
                           object2))
  << "Test event with value and extra must be present.";
  ASSERT_TRUE(EventPresent(cx.GetJSContext(), eventsSnapshot, category, method2,
                           object2))
  << "Test event with truncated value and extra must be present.";

  // Ensure that the truncations happened appropriately.
  JSContext* aCx = cx.GetJSContext();
  JS::RootedObject arrayObj(aCx, &eventsSnapshot.toObject());
  JS::Rooted<JS::Value> eventRecord(aCx);
  ASSERT_TRUE(JS_GetElement(aCx, arrayObj, 2, &eventRecord))
  << "Must be able to get record.";
  JS::RootedObject recordArray(aCx, &eventRecord.toObject());
  uint32_t recordLength;
  ASSERT_TRUE(JS_GetArrayLength(aCx, recordArray, &recordLength))
  << "Event record array must have length.";
  ASSERT_TRUE(recordLength == 6)
  << "Event record must have 6 elements.";

  JS::Rooted<JS::Value> str(aCx);
  nsAutoJSString jsStr;
  // The value string is at index 4
  ASSERT_TRUE(JS_GetElement(aCx, recordArray, 4, &str))
  << "Must be able to get value.";
  ASSERT_TRUE(jsStr.init(aCx, str))
  << "Value must be able to be init'd to a jsstring.";
  ASSERT_EQ(NS_ConvertUTF16toUTF8(jsStr).Length(), (uint32_t)80)
      << "Value must have been truncated to 80 bytes.";

  // Extra is at index 5
  JS::Rooted<JS::Value> obj(aCx);
  ASSERT_TRUE(JS_GetElement(aCx, recordArray, 5, &obj))
  << "Must be able to get extra.";
  JS::RootedObject extraObj(aCx, &obj.toObject());
  JS::Rooted<JS::Value> extraVal(aCx);
  ASSERT_TRUE(JS_GetProperty(aCx, extraObj, extraKey.get(), &extraVal))
  << "Must be able to get the extra key's value.";
  ASSERT_TRUE(jsStr.init(aCx, extraVal))
  << "Extra must be able to be init'd to a jsstring.";
  ASSERT_EQ(NS_ConvertUTF16toUTF8(jsStr).Length(), (uint32_t)80)
      << "Extra must have been truncated to 80 bytes.";
}
