/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#include "core/TelemetryOrigin.h"
#include "gtest/gtest.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Unused.h"
#include "TelemetryFixture.h"
#include "TelemetryTestHelpers.h"

using namespace mozilla;
using namespace TelemetryTestHelpers;
using mozilla::Telemetry::OriginMetricID;

// Test that we can properly record origin stuff using the C++ API.
TEST_F(TelemetryTestFixture, RecordOrigin) {
  AutoJSContextWithGlobal cx(mCleanGlobal);
  JSContext* aCx = cx.GetJSContext();

  Unused << mTelemetry->ClearOrigins();

  const nsLiteralCString doubleclick("doubleclick.de");
  const nsLiteralCString telemetryTest1("telemetry.test_test1");

  Telemetry::RecordOrigin(OriginMetricID::TelemetryTest_Test1, doubleclick);

  JS::RootedValue originSnapshot(aCx);
  GetOriginSnapshot(aCx, &originSnapshot);

  ASSERT_FALSE(originSnapshot.isNullOrUndefined())
      << "Origin snapshot must not be null/undefined.";

  JS::RootedValue origins(aCx);
  JS::RootedObject snapshotObj(aCx, &originSnapshot.toObject());
  ASSERT_TRUE(JS_GetProperty(aCx, snapshotObj, telemetryTest1.get(), &origins))
      << "telemetry.test_test1 must be in the snapshot.";

  JS::RootedObject originsObj(aCx, &origins.toObject());
  bool isArray = false;
  ASSERT_TRUE(JS_IsArrayObject(aCx, originsObj, &isArray) && isArray)
      << "The metric's origins must be in an array.";

  uint32_t length = 0;
  ASSERT_TRUE(JS_GetArrayLength(aCx, originsObj, &length) && length == 1)
      << "Length of returned array must be 1.";

  JS::RootedValue origin(aCx);
  ASSERT_TRUE(JS_GetElement(aCx, originsObj, 0, &origin));
  ASSERT_TRUE(origin.isString());
  nsAutoJSString jsStr;
  ASSERT_TRUE(jsStr.init(aCx, origin));
  ASSERT_TRUE(NS_ConvertUTF16toUTF8(jsStr) == doubleclick)
      << "Origin must be faithfully stored and snapshotted.";

  // Now test that the snapshot didn't clear things out.
  GetOriginSnapshot(aCx, &originSnapshot);
  ASSERT_FALSE(originSnapshot.isNullOrUndefined());
  JS::RootedObject unemptySnapshotObj(aCx, &originSnapshot.toObject());
  JS::Rooted<JS::IdVector> ids(aCx, JS::IdVector(aCx));
  ASSERT_TRUE(JS_Enumerate(aCx, unemptySnapshotObj, &ids));
  ASSERT_GE(ids.length(), (unsigned)0) << "Returned object must not be empty.";
}

TEST_F(TelemetryTestFixture, RecordOriginTwiceAndClear) {
  AutoJSContextWithGlobal cx(mCleanGlobal);
  JSContext* aCx = cx.GetJSContext();

  Unused << mTelemetry->ClearOrigins();

  const nsLiteralCString doubleclick("doubleclick.de");
  const nsLiteralCString telemetryTest1("telemetry.test_test1");

  Telemetry::RecordOrigin(OriginMetricID::TelemetryTest_Test1, doubleclick);
  Telemetry::RecordOrigin(OriginMetricID::TelemetryTest_Test1, doubleclick);

  JS::RootedValue originSnapshot(aCx);
  GetOriginSnapshot(aCx, &originSnapshot, true /* aClear */);

  ASSERT_FALSE(originSnapshot.isNullOrUndefined())
      << "Origin snapshot must not be null/undefined.";

  JS::RootedValue origins(aCx);
  JS::RootedObject snapshotObj(aCx, &originSnapshot.toObject());
  ASSERT_TRUE(JS_GetProperty(aCx, snapshotObj, telemetryTest1.get(), &origins))
      << "telemetry.test_test1 must be in the snapshot.";

  JS::RootedObject originsObj(aCx, &origins.toObject());
  bool isArray = false;
  ASSERT_TRUE(JS_IsArrayObject(aCx, originsObj, &isArray) && isArray)
      << "The metric's origins must be in an array.";

  uint32_t length = 0;
  ASSERT_TRUE(JS_GetArrayLength(aCx, originsObj, &length) && length == 2)
      << "Length of returned array must be 1.";

  for (uint32_t i = 0; i < length; ++i) {
    JS::RootedValue origin(aCx);
    ASSERT_TRUE(JS_GetElement(aCx, originsObj, i, &origin));
    ASSERT_TRUE(origin.isString());
    nsAutoJSString jsStr;
    ASSERT_TRUE(jsStr.init(aCx, origin));
    ASSERT_TRUE(NS_ConvertUTF16toUTF8(jsStr) == doubleclick)
        << "Origin must be faithfully stored and snapshotted.";
  }

  // Now check that snapshotting with clear actually cleared it.
  GetOriginSnapshot(aCx, &originSnapshot);
  ASSERT_FALSE(originSnapshot.isNullOrUndefined());
  JS::RootedObject emptySnapshotObj(aCx, &originSnapshot.toObject());
  JS::Rooted<JS::IdVector> ids(aCx, JS::IdVector(aCx));
  ASSERT_TRUE(JS_Enumerate(aCx, emptySnapshotObj, &ids));
  ASSERT_EQ(ids.length(), (unsigned)0) << "Returned object must be empty.";
}
