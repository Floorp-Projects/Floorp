/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#include "TelemetryTestHelpers.h"

#include "gtest/gtest.h"
#include "mozilla/CycleCollectedJSContext.h"
#include "TelemetryCommon.h"
#include "mozilla/Unused.h"

using namespace mozilla;

// Helper methods provided to simplify writing tests and meant to be used in C++ Gtests.
namespace TelemetryTestHelpers {

void
CheckUintScalar(const char* aName, JSContext* aCx, JS::HandleValue aSnapshot, uint32_t expectedValue)
{
  // Validate the value of the test scalar.
  JS::RootedValue value(aCx);
  JS::RootedObject scalarObj(aCx, &aSnapshot.toObject());
  ASSERT_TRUE(JS_GetProperty(aCx, scalarObj, aName, &value)) << "The test scalar must be reported.";
  JS_GetProperty(aCx, scalarObj, aName, &value);

  ASSERT_TRUE(value.isInt32()) << "The scalar value must be of the correct type.";
  ASSERT_TRUE(value.toInt32() >= 0) << "The uint scalar type must contain a value >= 0.";
  ASSERT_EQ(static_cast<uint32_t>(value.toInt32()), expectedValue) << "The scalar value must match the expected value.";
}

void
CheckBoolScalar(const char* aName, JSContext* aCx, JS::HandleValue aSnapshot, bool expectedValue)
{
  // Validate the value of the test scalar.
  JS::RootedValue value(aCx);
  JS::RootedObject scalarObj(aCx, &aSnapshot.toObject());
  ASSERT_TRUE(JS_GetProperty(aCx, scalarObj, aName, &value)) << "The test scalar must be reported.";
  ASSERT_TRUE(value.isBoolean()) << "The scalar value must be of the correct type.";
  ASSERT_EQ(static_cast<bool>(value.toBoolean()), expectedValue) << "The scalar value must match the expected value.";
}

void
CheckStringScalar(const char* aName, JSContext* aCx, JS::HandleValue aSnapshot, const char* expectedValue)
{
  // Validate the value of the test scalar.
  JS::RootedValue value(aCx);
  JS::RootedObject scalarObj(aCx, &aSnapshot.toObject());
  ASSERT_TRUE(JS_GetProperty(aCx, scalarObj, aName, &value)) << "The test scalar must be reported.";
  ASSERT_TRUE(value.isString()) << "The scalar value must be of the correct type.";

  bool sameString;
  ASSERT_TRUE(JS_StringEqualsAscii(aCx, value.toString(), expectedValue, &sameString)) << "JS String comparison failed";
  ASSERT_TRUE(sameString) << "The scalar value must match the expected string";
}

void
CheckKeyedUintScalar(const char* aName, const char* aKey, JSContext* aCx, JS::HandleValue aSnapshot,
                     uint32_t expectedValue)
{
  JS::RootedValue keyedScalar(aCx);
  JS::RootedObject scalarObj(aCx, &aSnapshot.toObject());
  // Get the aName keyed scalar object from the scalars snapshot.
  ASSERT_TRUE(JS_GetProperty(aCx, scalarObj, aName, &keyedScalar))
    << "The keyed scalar must be reported.";

  CheckUintScalar(aKey, aCx, keyedScalar, expectedValue);
}

void
CheckKeyedBoolScalar(const char* aName, const char* aKey, JSContext* aCx, JS::HandleValue aSnapshot,
                     bool expectedValue)
{
  JS::RootedValue keyedScalar(aCx);
  JS::RootedObject scalarObj(aCx, &aSnapshot.toObject());
  // Get the aName keyed scalar object from the scalars snapshot.
  ASSERT_TRUE(JS_GetProperty(aCx, scalarObj, aName, &keyedScalar))
    << "The keyed scalar must be reported.";

  CheckBoolScalar(aKey, aCx, keyedScalar, expectedValue);
}

void
CheckNumberOfProperties(const char* aName, JSContext* aCx, JS::HandleValue aSnapshot,
                        uint32_t expectedNumProperties)
{
  JS::RootedValue keyedScalar(aCx);
  JS::RootedObject scalarObj(aCx, &aSnapshot.toObject());
  // Get the aName keyed scalar object from the scalars snapshot.
  ASSERT_TRUE(JS_GetProperty(aCx, scalarObj, aName, &keyedScalar))
    << "The keyed scalar must be reported.";

  JS::RootedObject keyedScalarObj(aCx, &keyedScalar.toObject());
  JS::Rooted<JS::IdVector> ids(aCx, JS::IdVector(aCx));
  ASSERT_TRUE(JS_Enumerate(aCx, keyedScalarObj, &ids))
    << "We must be able to get keyed scalar members.";

  ASSERT_EQ(expectedNumProperties, ids.length())
    << "The scalar must report the expected number of properties.";
}

void
GetScalarsSnapshot(bool aKeyed, JSContext* aCx, JS::MutableHandle<JS::Value> aResult,
                   ProcessID aProcessType)
{
  nsCOMPtr<nsITelemetry> telemetry = do_GetService("@mozilla.org/base/telemetry;1");

  // Get a snapshot of the scalars.
  JS::RootedValue scalarsSnapshot(aCx);
  nsresult rv;

  if (aKeyed) {
    rv = telemetry->SnapshotKeyedScalars(nsITelemetry::DATASET_RELEASE_CHANNEL_OPTIN,
                                         false, aCx, 0, &scalarsSnapshot);
  } else {
    rv = telemetry->SnapshotScalars(nsITelemetry::DATASET_RELEASE_CHANNEL_OPTIN,
                                    false, aCx, 0, &scalarsSnapshot);
  }

  // Validate the snapshot.
  ASSERT_EQ(rv, NS_OK) << "Creating a snapshot of the data must not fail.";
  ASSERT_TRUE(scalarsSnapshot.isObject()) << "The snapshot must be an object.";

  JS::RootedValue processScalars(aCx);
  JS::RootedObject scalarObj(aCx, &scalarsSnapshot.toObject());
  // Don't complain if no scalars for the process can be found. Just
  // return an empty object.
  Unused << JS_GetProperty(aCx,
                           scalarObj,
                           Telemetry::Common::GetNameForProcessID(aProcessType),
                           &processScalars);

  aResult.set(processScalars);
}

void
GetAndClearHistogram(JSContext* cx, nsCOMPtr<nsITelemetry> mTelemetry,
                     const nsACString &name, bool is_keyed)
{
  JS::RootedValue testHistogram(cx);
  nsresult rv = is_keyed ? mTelemetry->GetKeyedHistogramById(name, cx, &testHistogram)
                         : mTelemetry->GetHistogramById(name, cx, &testHistogram);

  ASSERT_EQ(rv, NS_OK) << "Cannot fetch histogram";

  // Clear the stored value
  JS::RootedObject testHistogramObj(cx, &testHistogram.toObject());
  JS::RootedValue rval(cx);
  ASSERT_TRUE(JS_CallFunctionName(cx, testHistogramObj, "clear",
                  JS::HandleValueArray::empty(), &rval)) << "Cannot clear histogram";
}

void
GetProperty(JSContext* cx, const char* name, JS::HandleValue valueIn,
            JS::MutableHandleValue valueOut)
{
  JS::RootedValue property(cx);
  JS::RootedObject valueInObj(cx, &valueIn.toObject());
  ASSERT_TRUE(JS_GetProperty(cx, valueInObj, name, &property))
    << "Cannot get property '" << name << "'";
  valueOut.set(property);
}

void
GetElement(JSContext* cx, uint32_t index, JS::HandleValue valueIn,
           JS::MutableHandleValue valueOut)
{
  JS::RootedValue element(cx);
  JS::RootedObject valueInObj(cx, &valueIn.toObject());
  ASSERT_TRUE(JS_GetElement(cx, valueInObj, index, &element))
    << "Cannot get element at index '" << index << "'";
  valueOut.set(element);
}

void
GetSnapshots(JSContext* cx, nsCOMPtr<nsITelemetry> mTelemetry,
             const char* name, JS::MutableHandleValue valueOut, bool is_keyed)
{
  JS::RootedValue snapshots(cx);
  nsresult rv = is_keyed ? mTelemetry->SnapshotKeyedHistograms(nsITelemetry::DATASET_RELEASE_CHANNEL_OPTIN, false, cx, &snapshots)
                         : mTelemetry->SnapshotHistograms(nsITelemetry::DATASET_RELEASE_CHANNEL_OPTIN, false, cx, &snapshots);

  JS::RootedValue snapshot(cx);
  GetProperty(cx, "parent", snapshots, &snapshot);

  ASSERT_EQ(rv, NS_OK) << "Cannot call histogram snapshots";
  valueOut.set(snapshot);
}

} // namespace TelemetryTestHelpers
