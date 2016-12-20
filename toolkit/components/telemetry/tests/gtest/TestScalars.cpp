/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#include "gtest/gtest.h"

#include "js/Conversions.h"
#include "mozilla/Unused.h"
#include "nsJSUtils.h" // nsAutoJSString
#include "nsITelemetry.h"
#include "Telemetry.h"
#include "TelemetryFixture.h"

using namespace mozilla;

#define EXPECTED_STRING "Nice, expected and creative string."

namespace {

void
CheckUintScalar(const char* aName, JSContext* aCx, JS::HandleValue aSnapshot, uint32_t expectedValue)
{
  // Validate the value of the test scalar.
  JS::RootedValue value(aCx);
  JS::RootedObject scalarObj(aCx, &aSnapshot.toObject());
  ASSERT_TRUE(JS_GetProperty(aCx, scalarObj, aName, &value)) << "The test scalar must be reported.";
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
GetScalarsSnapshot(bool aKeyed, JSContext* aCx, JS::MutableHandle<JS::Value> aResult)
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

  aResult.set(scalarsSnapshot);
}

} // Anonymous namespace.

// Test that we can properly write unsigned scalars using the C++ API.
TEST_F(TelemetryTestFixture, ScalarUnsigned) {
  AutoJSContextWithGlobal cx(mCleanGlobal);

  // Make sure we don't get scalars from other tests.
  Unused << mTelemetry->ClearScalars();

  // Set the test scalar to a known value.
  const uint32_t kInitialValue = 1172015;
  const uint32_t kExpectedUint = 1172017;
  Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_TEST_UNSIGNED_INT_KIND, kInitialValue);
  Telemetry::ScalarAdd(Telemetry::ScalarID::TELEMETRY_TEST_UNSIGNED_INT_KIND, kExpectedUint - kInitialValue);

  // Check the recorded value.
  JS::RootedValue scalarsSnapshot(cx.GetJSContext());
  GetScalarsSnapshot(false, cx.GetJSContext(), &scalarsSnapshot);
  CheckUintScalar("telemetry.test.unsigned_int_kind", cx.GetJSContext(), scalarsSnapshot, kExpectedUint);

  // Try to use SetMaximum.
  const uint32_t kExpectedUintMaximum = kExpectedUint * 2;
  Telemetry::ScalarSetMaximum(Telemetry::ScalarID::TELEMETRY_TEST_UNSIGNED_INT_KIND, kExpectedUintMaximum);

  // Make sure that calls of the unsupported type don't corrupt the stored value.
  // Don't run this part in debug builds as that intentionally asserts.
  #ifndef DEBUG
    Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_TEST_UNSIGNED_INT_KIND, false);
    Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_TEST_UNSIGNED_INT_KIND, NS_LITERAL_STRING("test"));
  #endif

  // Check the recorded value.
  GetScalarsSnapshot(false, cx.GetJSContext(), &scalarsSnapshot);
  CheckUintScalar("telemetry.test.unsigned_int_kind", cx.GetJSContext(), scalarsSnapshot, kExpectedUintMaximum);
}

// Test that we can properly write boolean scalars using the C++ API.
TEST_F(TelemetryTestFixture, ScalarBoolean) {
  AutoJSContextWithGlobal cx(mCleanGlobal);

  Unused << mTelemetry->ClearScalars();

  // Set the test scalar to a known value.
  Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_TEST_BOOLEAN_KIND, true);

  // Make sure that calls of the unsupported type don't corrupt the stored value.
  // Don't run this part in debug builds as that intentionally asserts.
  #ifndef DEBUG
    Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_TEST_BOOLEAN_KIND, static_cast<uint32_t>(12));
    Telemetry::ScalarSetMaximum(Telemetry::ScalarID::TELEMETRY_TEST_BOOLEAN_KIND, 20);
    Telemetry::ScalarAdd(Telemetry::ScalarID::TELEMETRY_TEST_BOOLEAN_KIND, 2);
    Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_TEST_BOOLEAN_KIND, NS_LITERAL_STRING("test"));
  #endif

  // Check the recorded value.
  JS::RootedValue scalarsSnapshot(cx.GetJSContext());
  GetScalarsSnapshot(false, cx.GetJSContext(), &scalarsSnapshot);
  CheckBoolScalar("telemetry.test.boolean_kind", cx.GetJSContext(), scalarsSnapshot, true);
}

// Test that we can properly write string scalars using the C++ API.
TEST_F(TelemetryTestFixture, ScalarString) {
  AutoJSContextWithGlobal cx(mCleanGlobal);

  Unused << mTelemetry->ClearScalars();

  // Set the test scalar to a known value.
  Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_TEST_STRING_KIND, NS_LITERAL_STRING(EXPECTED_STRING));

  // Make sure that calls of the unsupported type don't corrupt the stored value.
  // Don't run this part in debug builds as that intentionally asserts.
  #ifndef DEBUG
    Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_TEST_STRING_KIND, static_cast<uint32_t>(12));
    Telemetry::ScalarSetMaximum(Telemetry::ScalarID::TELEMETRY_TEST_STRING_KIND, 20);
    Telemetry::ScalarAdd(Telemetry::ScalarID::TELEMETRY_TEST_STRING_KIND, 2);
    Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_TEST_STRING_KIND, true);
  #endif

  // Check the recorded value.
  JS::RootedValue scalarsSnapshot(cx.GetJSContext());
  GetScalarsSnapshot(false, cx.GetJSContext(), &scalarsSnapshot);
  CheckStringScalar("telemetry.test.string_kind", cx.GetJSContext(), scalarsSnapshot, EXPECTED_STRING);
}

// Test that we can properly write keyed unsigned scalars using the C++ API.
TEST_F(TelemetryTestFixture, KeyedScalarUnsigned) {
  AutoJSContextWithGlobal cx(mCleanGlobal);

  Unused << mTelemetry->ClearScalars();

  // Set the test scalar to a known value.
  const char* kScalarName = "telemetry.test.keyed_unsigned_int";
  const uint32_t kKey1Value = 1172015;
  const uint32_t kKey2Value = 1172017;
  Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_TEST_KEYED_UNSIGNED_INT,
                       NS_LITERAL_STRING("key1"), kKey1Value);
  Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_TEST_KEYED_UNSIGNED_INT,
                       NS_LITERAL_STRING("key2"), kKey1Value);
  Telemetry::ScalarAdd(Telemetry::ScalarID::TELEMETRY_TEST_KEYED_UNSIGNED_INT,
                       NS_LITERAL_STRING("key2"), 2);

  // Make sure that calls of the unsupported type don't corrupt the stored value.
  // Don't run this part in debug builds as that intentionally asserts.
  #ifndef DEBUG
    Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_TEST_KEYED_UNSIGNED_INT,
                         NS_LITERAL_STRING("key1"), false);
    Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_TEST_KEYED_UNSIGNED_INT, NS_LITERAL_STRING("test"));
  #endif

  // Check the recorded value.
  JS::RootedValue scalarsSnapshot(cx.GetJSContext());
  GetScalarsSnapshot(true, cx.GetJSContext(), &scalarsSnapshot);

  // Check the keyed scalar we're interested in.
  CheckKeyedUintScalar(kScalarName, "key1", cx.GetJSContext(), scalarsSnapshot, kKey1Value);
  CheckKeyedUintScalar(kScalarName, "key2", cx.GetJSContext(), scalarsSnapshot, kKey2Value);
  CheckNumberOfProperties(kScalarName, cx.GetJSContext(), scalarsSnapshot, 2);

  // Try to use SetMaximum.
  const uint32_t kExpectedUintMaximum = kKey1Value * 2;
  Telemetry::ScalarSetMaximum(Telemetry::ScalarID::TELEMETRY_TEST_KEYED_UNSIGNED_INT,
                              NS_LITERAL_STRING("key1"), kExpectedUintMaximum);

  GetScalarsSnapshot(true, cx.GetJSContext(), &scalarsSnapshot);
  // The first key should be different and te second is expected to be the same.
  CheckKeyedUintScalar(kScalarName, "key1", cx.GetJSContext(), scalarsSnapshot, kExpectedUintMaximum);
  CheckKeyedUintScalar(kScalarName, "key2", cx.GetJSContext(), scalarsSnapshot, kKey2Value);
  CheckNumberOfProperties(kScalarName, cx.GetJSContext(), scalarsSnapshot, 2);
}

TEST_F(TelemetryTestFixture, KeyedScalarBoolean) {
  AutoJSContextWithGlobal cx(mCleanGlobal);

  Unused << mTelemetry->ClearScalars();

  // Set the test scalar to a known value.
  Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_TEST_KEYED_BOOLEAN_KIND,
                       NS_LITERAL_STRING("key1"), false);
  Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_TEST_KEYED_BOOLEAN_KIND,
                       NS_LITERAL_STRING("key2"), true);

  // Make sure that calls of the unsupported type don't corrupt the stored value.
  // Don't run this part in debug builds as that intentionally asserts.
  #ifndef DEBUG
    Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_TEST_KEYED_BOOLEAN_KIND,
                         NS_LITERAL_STRING("key1"), static_cast<uint32_t>(12));
    Telemetry::ScalarSetMaximum(Telemetry::ScalarID::TELEMETRY_TEST_KEYED_BOOLEAN_KIND,
                                NS_LITERAL_STRING("key1"), 20);
    Telemetry::ScalarAdd(Telemetry::ScalarID::TELEMETRY_TEST_KEYED_BOOLEAN_KIND,
                         NS_LITERAL_STRING("key1"), 2);
  #endif

  // Check the recorded value.
  JS::RootedValue scalarsSnapshot(cx.GetJSContext());
  GetScalarsSnapshot(true, cx.GetJSContext(), &scalarsSnapshot);

  // Make sure that the keys contain the expected values.
  const char* kScalarName = "telemetry.test.keyed_boolean_kind";
  CheckKeyedBoolScalar(kScalarName, "key1", cx.GetJSContext(), scalarsSnapshot, false);
  CheckKeyedBoolScalar(kScalarName, "key2", cx.GetJSContext(), scalarsSnapshot, true);
  CheckNumberOfProperties(kScalarName, cx.GetJSContext(), scalarsSnapshot, 2);
}
