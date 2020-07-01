/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#include "core/TelemetryScalar.h"
#include "gtest/gtest.h"
#include "js/Conversions.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TelemetryProcessEnums.h"
#include "mozilla/Unused.h"
#include "nsJSUtils.h"  // nsAutoJSString
#include "nsThreadUtils.h"
#include "TelemetryFixture.h"
#include "TelemetryTestHelpers.h"

using namespace mozilla;
using namespace TelemetryTestHelpers;
using mozilla::Telemetry::ProcessID;

#define EXPECTED_STRING "Nice, expected and creative string."

// Test that we can properly write unsigned scalars using the C++ API.
TEST_F(TelemetryTestFixture, ScalarUnsigned) {
  AutoJSContextWithGlobal cx(mCleanGlobal);

  // Make sure we don't get scalars from other tests.
  Unused << mTelemetry->ClearScalars();

  // Set the test scalar to a known value.
  const uint32_t kInitialValue = 1172015;
  const uint32_t kExpectedUint = 1172017;
  Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_TEST_UNSIGNED_INT_KIND,
                       kInitialValue);
  Telemetry::ScalarAdd(Telemetry::ScalarID::TELEMETRY_TEST_UNSIGNED_INT_KIND,
                       kExpectedUint - kInitialValue);

  // Check the recorded value.
  JS::RootedValue scalarsSnapshot(cx.GetJSContext());
  GetScalarsSnapshot(false, cx.GetJSContext(), &scalarsSnapshot);
  CheckUintScalar("telemetry.test.unsigned_int_kind", cx.GetJSContext(),
                  scalarsSnapshot, kExpectedUint);

  // Try to use SetMaximum.
  const uint32_t kExpectedUintMaximum = kExpectedUint * 2;
  Telemetry::ScalarSetMaximum(
      Telemetry::ScalarID::TELEMETRY_TEST_UNSIGNED_INT_KIND,
      kExpectedUintMaximum);

// Make sure that calls of the unsupported type don't corrupt the stored value.
// Don't run this part in debug builds as that intentionally asserts.
#ifndef DEBUG
  Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_TEST_UNSIGNED_INT_KIND,
                       false);
  Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_TEST_UNSIGNED_INT_KIND,
                       u"test"_ns);
#endif

  // Check the recorded value.
  GetScalarsSnapshot(false, cx.GetJSContext(), &scalarsSnapshot);
  CheckUintScalar("telemetry.test.unsigned_int_kind", cx.GetJSContext(),
                  scalarsSnapshot, kExpectedUintMaximum);
}

// Test that the AutoScalarTimer records a proper uint32_t value to a
// scalar once it goes out of scope.
TEST_F(TelemetryTestFixture, AutoScalarTimer) {
  AutoJSContextWithGlobal cx(mCleanGlobal);

  // Make sure we don't get scalars from other tests.
  Unused << mTelemetry->ClearScalars();
  {
    Telemetry::AutoScalarTimer<
        Telemetry::ScalarID::TELEMETRY_TEST_UNSIGNED_INT_KIND>
        timer;
  }

  const char* kScalarName = "telemetry.test.unsigned_int_kind";

  // Check that there's a recorded value that is greater than 0. Since
  // this is a timer, we'll not check the non-deterministic value - just
  // that it exists.
  JS::RootedValue scalarsSnapshot(cx.GetJSContext());
  GetScalarsSnapshot(false, cx.GetJSContext(), &scalarsSnapshot);

  // Validate the value of the test scalar.
  JS::RootedValue value(cx.GetJSContext());
  JS::RootedObject scalarObj(cx.GetJSContext(), &scalarsSnapshot.toObject());
  ASSERT_TRUE(JS_GetProperty(cx.GetJSContext(), scalarObj, kScalarName, &value))
  << "The test scalar must be reported.";

  JS_GetProperty(cx.GetJSContext(), scalarObj, kScalarName, &value);
  ASSERT_TRUE(value.isInt32())
  << "The scalar value must be of the correct type.";
  ASSERT_TRUE(value.toInt32() >= 0)
  << "The uint scalar type must contain a value >= 0.";
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
  Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_TEST_BOOLEAN_KIND,
                       static_cast<uint32_t>(12));
  Telemetry::ScalarSetMaximum(Telemetry::ScalarID::TELEMETRY_TEST_BOOLEAN_KIND,
                              20);
  Telemetry::ScalarAdd(Telemetry::ScalarID::TELEMETRY_TEST_BOOLEAN_KIND, 2);
  Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_TEST_BOOLEAN_KIND,
                       u"test"_ns);
#endif

  // Check the recorded value.
  JS::RootedValue scalarsSnapshot(cx.GetJSContext());
  GetScalarsSnapshot(false, cx.GetJSContext(), &scalarsSnapshot);
  CheckBoolScalar("telemetry.test.boolean_kind", cx.GetJSContext(),
                  scalarsSnapshot, true);
}

// Test that we can properly write string scalars using the C++ API.
TEST_F(TelemetryTestFixture, ScalarString) {
  AutoJSContextWithGlobal cx(mCleanGlobal);

  Unused << mTelemetry->ClearScalars();

  // Set the test scalar to a known value.
  Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_TEST_STRING_KIND,
                       NS_LITERAL_STRING_FROM_CSTRING(EXPECTED_STRING));

// Make sure that calls of the unsupported type don't corrupt the stored value.
// Don't run this part in debug builds as that intentionally asserts.
#ifndef DEBUG
  Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_TEST_STRING_KIND,
                       static_cast<uint32_t>(12));
  Telemetry::ScalarSetMaximum(Telemetry::ScalarID::TELEMETRY_TEST_STRING_KIND,
                              20);
  Telemetry::ScalarAdd(Telemetry::ScalarID::TELEMETRY_TEST_STRING_KIND, 2);
  Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_TEST_STRING_KIND, true);
#endif

  // Check the recorded value.
  JS::RootedValue scalarsSnapshot(cx.GetJSContext());
  GetScalarsSnapshot(false, cx.GetJSContext(), &scalarsSnapshot);
  CheckStringScalar("telemetry.test.string_kind", cx.GetJSContext(),
                    scalarsSnapshot, EXPECTED_STRING);
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
                       u"key1"_ns, kKey1Value);
  Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_TEST_KEYED_UNSIGNED_INT,
                       u"key2"_ns, kKey1Value);
  Telemetry::ScalarAdd(Telemetry::ScalarID::TELEMETRY_TEST_KEYED_UNSIGNED_INT,
                       u"key2"_ns, 2);

// Make sure that calls of the unsupported type don't corrupt the stored value.
// Don't run this part in debug builds as that intentionally asserts.
#ifndef DEBUG
  Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_TEST_KEYED_UNSIGNED_INT,
                       u"key1"_ns, false);
  Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_TEST_KEYED_UNSIGNED_INT,
                       u"test"_ns);
#endif

  // Check the recorded value.
  JS::RootedValue scalarsSnapshot(cx.GetJSContext());
  GetScalarsSnapshot(true, cx.GetJSContext(), &scalarsSnapshot);

  // Check the keyed scalar we're interested in.
  CheckKeyedUintScalar(kScalarName, "key1", cx.GetJSContext(), scalarsSnapshot,
                       kKey1Value);
  CheckKeyedUintScalar(kScalarName, "key2", cx.GetJSContext(), scalarsSnapshot,
                       kKey2Value);
  CheckNumberOfProperties(kScalarName, cx.GetJSContext(), scalarsSnapshot, 2);

  // Try to use SetMaximum.
  const uint32_t kExpectedUintMaximum = kKey1Value * 2;
  Telemetry::ScalarSetMaximum(
      Telemetry::ScalarID::TELEMETRY_TEST_KEYED_UNSIGNED_INT, u"key1"_ns,
      kExpectedUintMaximum);

  GetScalarsSnapshot(true, cx.GetJSContext(), &scalarsSnapshot);
  // The first key should be different and te second is expected to be the same.
  CheckKeyedUintScalar(kScalarName, "key1", cx.GetJSContext(), scalarsSnapshot,
                       kExpectedUintMaximum);
  CheckKeyedUintScalar(kScalarName, "key2", cx.GetJSContext(), scalarsSnapshot,
                       kKey2Value);
  CheckNumberOfProperties(kScalarName, cx.GetJSContext(), scalarsSnapshot, 2);
}

TEST_F(TelemetryTestFixture, KeyedScalarBoolean) {
  AutoJSContextWithGlobal cx(mCleanGlobal);

  Unused << mTelemetry->ClearScalars();

  // Set the test scalar to a known value.
  Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_TEST_KEYED_BOOLEAN_KIND,
                       u"key1"_ns, false);
  Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_TEST_KEYED_BOOLEAN_KIND,
                       u"key2"_ns, true);

// Make sure that calls of the unsupported type don't corrupt the stored value.
// Don't run this part in debug builds as that intentionally asserts.
#ifndef DEBUG
  Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_TEST_KEYED_BOOLEAN_KIND,
                       u"key1"_ns, static_cast<uint32_t>(12));
  Telemetry::ScalarSetMaximum(
      Telemetry::ScalarID::TELEMETRY_TEST_KEYED_BOOLEAN_KIND, u"key1"_ns, 20);
  Telemetry::ScalarAdd(Telemetry::ScalarID::TELEMETRY_TEST_KEYED_BOOLEAN_KIND,
                       u"key1"_ns, 2);
#endif

  // Check the recorded value.
  JS::RootedValue scalarsSnapshot(cx.GetJSContext());
  GetScalarsSnapshot(true, cx.GetJSContext(), &scalarsSnapshot);

  // Make sure that the keys contain the expected values.
  const char* kScalarName = "telemetry.test.keyed_boolean_kind";
  CheckKeyedBoolScalar(kScalarName, "key1", cx.GetJSContext(), scalarsSnapshot,
                       false);
  CheckKeyedBoolScalar(kScalarName, "key2", cx.GetJSContext(), scalarsSnapshot,
                       true);
  CheckNumberOfProperties(kScalarName, cx.GetJSContext(), scalarsSnapshot, 2);
}

TEST_F(TelemetryTestFixture, NonMainThreadAdd) {
  AutoJSContextWithGlobal cx(mCleanGlobal);

  Unused << mTelemetry->ClearScalars();

  // Define the function that will be called on the testing thread.
  nsCOMPtr<nsIRunnable> runnable = NS_NewRunnableFunction(
      "TelemetryTestFixture_NonMainThreadAdd_Test::TestBody", []() -> void {
        Telemetry::ScalarAdd(
            Telemetry::ScalarID::TELEMETRY_TEST_UNSIGNED_INT_KIND, 37);
      });

  // Spawn the testing thread and run the function.
  nsCOMPtr<nsIThread> testingThread;
  nsresult rv =
      NS_NewNamedThread("Test thread", getter_AddRefs(testingThread), runnable);
  ASSERT_EQ(rv, NS_OK);

  // Shutdown the thread. This also waits for the runnable to complete.
  testingThread->Shutdown();

  // Check the recorded value.
  JS::RootedValue scalarsSnapshot(cx.GetJSContext());
  GetScalarsSnapshot(false, cx.GetJSContext(), &scalarsSnapshot);
  CheckUintScalar("telemetry.test.unsigned_int_kind", cx.GetJSContext(),
                  scalarsSnapshot, 37);
}

TEST_F(TelemetryTestFixture, ScalarUnknownID) {
  AutoJSContextWithGlobal cx(mCleanGlobal);

  // Make sure we don't get scalars from other tests.
  Unused << mTelemetry->ClearScalars();

// Don't run this part in debug builds as that intentionally asserts.
#ifndef DEBUG
  const uint32_t kTestFakeIds[] = {
      static_cast<uint32_t>(Telemetry::ScalarID::ScalarCount),
      static_cast<uint32_t>(Telemetry::ScalarID::ScalarCount) + 378537,
      std::numeric_limits<uint32_t>::max()};

  for (auto id : kTestFakeIds) {
    Telemetry::ScalarID scalarId = static_cast<Telemetry::ScalarID>(id);
    Telemetry::ScalarSet(scalarId, static_cast<uint32_t>(1));
    Telemetry::ScalarSet(scalarId, true);
    Telemetry::ScalarSet(scalarId, u"test"_ns);
    Telemetry::ScalarAdd(scalarId, 1);
    Telemetry::ScalarSetMaximum(scalarId, 1);

    // Make sure that nothing was recorded in the plain scalars.
    JS::RootedValue scalarsSnapshot(cx.GetJSContext());
    GetScalarsSnapshot(false, cx.GetJSContext(), &scalarsSnapshot);
    ASSERT_TRUE(scalarsSnapshot.isUndefined())
    << "No scalar must be recorded";

    // Same for the keyed scalars.
    Telemetry::ScalarSet(scalarId, u"key1"_ns, static_cast<uint32_t>(1));
    Telemetry::ScalarSet(scalarId, u"key1"_ns, true);
    Telemetry::ScalarAdd(scalarId, u"key1"_ns, 1);
    Telemetry::ScalarSetMaximum(scalarId, u"key1"_ns, 1);

    // Make sure that nothing was recorded in the keyed scalars.
    JS::RootedValue keyedSnapshot(cx.GetJSContext());
    GetScalarsSnapshot(true, cx.GetJSContext(), &keyedSnapshot);
    ASSERT_TRUE(keyedSnapshot.isUndefined())
    << "No keyed scalar must be recorded";
  }
#endif
}

TEST_F(TelemetryTestFixture, ScalarEventSummary) {
  AutoJSContextWithGlobal cx(mCleanGlobal);

  // Make sure we don't get scalars from other tests.
  Unused << mTelemetry->ClearScalars();

  const char* kScalarName = "telemetry.event_counts";

  const char* kLongestEvent =
      "oohwowlookthiscategoryissolong#thismethodislongtooo#"
      "thisobjectisnoslouch";
  TelemetryScalar::SummarizeEvent(nsCString(kLongestEvent), ProcessID::Parent,
                                  false /* aDynamic */);

  // Check the recorded value.
  JS::RootedValue scalarsSnapshot(cx.GetJSContext());
  GetScalarsSnapshot(true, cx.GetJSContext(), &scalarsSnapshot);

  CheckKeyedUintScalar(kScalarName, kLongestEvent, cx.GetJSContext(),
                       scalarsSnapshot, 1);

// Don't run this part in debug builds as that intentionally asserts.
#ifndef DEBUG
  const char* kTooLongEvent =
      "oohwowlookthiscategoryissolong#thismethodislongtooo#"
      "thisobjectisnoslouch2";
  TelemetryScalar::SummarizeEvent(nsCString(kTooLongEvent), ProcessID::Parent,
                                  false /* aDynamic */);

  GetScalarsSnapshot(true, cx.GetJSContext(), &scalarsSnapshot);
  CheckNumberOfProperties(kScalarName, cx.GetJSContext(), scalarsSnapshot, 1);
#endif  // #ifndef DEBUG

  // Test we can fill the next 499 keys up to our 500 maximum
  for (int i = 1; i < 500; i++) {
    std::ostringstream eventName;
    eventName << "category#method#object" << i;
    TelemetryScalar::SummarizeEvent(nsCString(eventName.str().c_str()),
                                    ProcessID::Parent, false /* aDynamic */);
  }

  GetScalarsSnapshot(true, cx.GetJSContext(), &scalarsSnapshot);
  CheckNumberOfProperties(kScalarName, cx.GetJSContext(), scalarsSnapshot, 500);

// Don't run this part in debug builds as that intentionally asserts.
#ifndef DEBUG
  TelemetryScalar::SummarizeEvent(nsCString("whoops#too#many"),
                                  ProcessID::Parent, false /* aDynamic */);

  GetScalarsSnapshot(true, cx.GetJSContext(), &scalarsSnapshot);
  CheckNumberOfProperties(kScalarName, cx.GetJSContext(), scalarsSnapshot, 500);
#endif  // #ifndef DEBUG
}

TEST_F(TelemetryTestFixture, ScalarEventSummary_Dynamic) {
  AutoJSContextWithGlobal cx(mCleanGlobal);

  // Make sure we don't get scalars from other tests.
  Unused << mTelemetry->ClearScalars();

  const char* kScalarName = "telemetry.dynamic_event_counts";
  const char* kLongestEvent =
      "oohwowlookthiscategoryissolong#thismethodislongtooo#"
      "thisobjectisnoslouch";
  TelemetryScalar::SummarizeEvent(nsCString(kLongestEvent), ProcessID::Parent,
                                  true /* aDynamic */);
  TelemetryScalar::SummarizeEvent(nsCString(kLongestEvent), ProcessID::Content,
                                  true /* aDynamic */);

  // Check the recorded value.
  JS::RootedValue scalarsSnapshot(cx.GetJSContext());
  GetScalarsSnapshot(true, cx.GetJSContext(), &scalarsSnapshot,
                     ProcessID::Dynamic);

  // Recording in parent or content doesn't matter for dynamic scalars
  // which all end up in the same place.
  CheckKeyedUintScalar(kScalarName, kLongestEvent, cx.GetJSContext(),
                       scalarsSnapshot, 2);
}

TEST_F(TelemetryTestFixture, WrongScalarOperator) {
  AutoJSContextWithGlobal cx(mCleanGlobal);

  // Make sure we don't get scalars from other tests.
  Unused << mTelemetry->ClearScalars();

  const uint32_t expectedValue = 1172015;

  Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_TEST_UNSIGNED_INT_KIND,
                       expectedValue);
  Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_TEST_STRING_KIND,
                       NS_LITERAL_STRING_FROM_CSTRING(EXPECTED_STRING));
  Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_TEST_BOOLEAN_KIND, true);

  TelemetryScalar::DeserializationStarted();

  Telemetry::ScalarAdd(Telemetry::ScalarID::TELEMETRY_TEST_STRING_KIND, 1447);
  Telemetry::ScalarAdd(Telemetry::ScalarID::TELEMETRY_TEST_BOOLEAN_KIND, 1447);
  Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_TEST_UNSIGNED_INT_KIND,
                       true);
  TelemetryScalar::ApplyPendingOperations();

  JS::RootedValue scalarsSnapshot(cx.GetJSContext());
  GetScalarsSnapshot(false, cx.GetJSContext(), &scalarsSnapshot);
  CheckStringScalar("telemetry.test.string_kind", cx.GetJSContext(),
                    scalarsSnapshot, EXPECTED_STRING);
  CheckBoolScalar("telemetry.test.boolean_kind", cx.GetJSContext(),
                  scalarsSnapshot, true);
  CheckUintScalar("telemetry.test.unsigned_int_kind", cx.GetJSContext(),
                  scalarsSnapshot, expectedValue);
}

TEST_F(TelemetryTestFixture, WrongKeyedScalarOperator) {
  AutoJSContextWithGlobal cx(mCleanGlobal);

  // Make sure we don't get scalars from other tests.
  Unused << mTelemetry->ClearScalars();

  const uint32_t kExpectedUint = 1172017;

  Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_TEST_KEYED_UNSIGNED_INT,
                       u"key1"_ns, kExpectedUint);
  Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_TEST_KEYED_BOOLEAN_KIND,
                       u"key2"_ns, true);

  TelemetryScalar::DeserializationStarted();

  Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_TEST_KEYED_UNSIGNED_INT,
                       u"key1"_ns, false);
  Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_TEST_KEYED_BOOLEAN_KIND,
                       u"key2"_ns, static_cast<uint32_t>(13));

  TelemetryScalar::ApplyPendingOperations();

  JS::RootedValue scalarsSnapshot(cx.GetJSContext());
  GetScalarsSnapshot(true, cx.GetJSContext(), &scalarsSnapshot);
  CheckKeyedUintScalar("telemetry.test.keyed_unsigned_int", "key1",
                       cx.GetJSContext(), scalarsSnapshot, kExpectedUint);
  CheckKeyedBoolScalar("telemetry.test.keyed_boolean_kind", "key2",
                       cx.GetJSContext(), scalarsSnapshot, true);
}

TEST_F(TelemetryTestFixture, TestKeyedScalarAllowedKeys) {
  AutoJSContextWithGlobal cx(mCleanGlobal);
  // Make sure we don't get scalars from other tests.
  Unused << mTelemetry->ClearScalars();

  const uint32_t kExpectedUint = 1172017;

  Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_TEST_KEYED_WITH_KEYS,
                       u"only"_ns, kExpectedUint);
  Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_TEST_KEYED_WITH_KEYS,
                       u"meant"_ns, kExpectedUint);
  Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_TEST_KEYED_WITH_KEYS,
                       u"for"_ns, kExpectedUint);
  Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_TEST_KEYED_WITH_KEYS,
                       u"testing"_ns, kExpectedUint);

  Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_TEST_KEYED_WITH_KEYS,
                       u"invalid"_ns, kExpectedUint);
  Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_TEST_KEYED_WITH_KEYS,
                       u"not-valid"_ns, kExpectedUint);

  JS::RootedValue scalarsSnapshot(cx.GetJSContext());
  GetScalarsSnapshot(true, cx.GetJSContext(), &scalarsSnapshot);
  CheckKeyedUintScalar("telemetry.test.keyed_with_keys", "only",
                       cx.GetJSContext(), scalarsSnapshot, kExpectedUint);
  CheckKeyedUintScalar("telemetry.test.keyed_with_keys", "meant",
                       cx.GetJSContext(), scalarsSnapshot, kExpectedUint);
  CheckKeyedUintScalar("telemetry.test.keyed_with_keys", "for",
                       cx.GetJSContext(), scalarsSnapshot, kExpectedUint);
  CheckKeyedUintScalar("telemetry.test.keyed_with_keys", "testing",
                       cx.GetJSContext(), scalarsSnapshot, kExpectedUint);
  CheckNumberOfProperties("telemetry.test.keyed_with_keys", cx.GetJSContext(),
                          scalarsSnapshot, 4);

  CheckKeyedUintScalar("telemetry.keyed_scalars_unknown_keys",
                       "telemetry.test.keyed_with_keys", cx.GetJSContext(),
                       scalarsSnapshot, 2);
}
