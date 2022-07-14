/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#include "TelemetryTestHelpers.h"

#include "core/TelemetryCommon.h"
#include "core/TelemetryOrigin.h"
#include "gtest/gtest.h"
#include "js/Array.h"               // JS::GetArrayLength, JS::IsArrayObject
#include "js/CallAndConstruct.h"    // JS_CallFunctionName
#include "js/PropertyAndElement.h"  // JS_Enumerate, JS_GetElement, JS_GetProperty
#include "mozilla/CycleCollectedJSContext.h"
#include "mozilla/Unused.h"
#include "nsTArray.h"
#include "nsPrintfCString.h"

using namespace mozilla;

// Helper methods provided to simplify writing tests and meant to be used in C++
// Gtests.
namespace TelemetryTestHelpers {

void CheckUintScalar(const char* aName, JSContext* aCx,
                     JS::Handle<JS::Value> aSnapshot, uint32_t expectedValue) {
  // Validate the value of the test scalar.
  JS::Rooted<JS::Value> value(aCx);
  JS::Rooted<JSObject*> scalarObj(aCx, &aSnapshot.toObject());
  ASSERT_TRUE(JS_GetProperty(aCx, scalarObj, aName, &value))
  << "The test scalar must be reported.";
  JS_GetProperty(aCx, scalarObj, aName, &value);

  ASSERT_TRUE(value.isInt32())
  << "The scalar value must be of the correct type.";
  ASSERT_TRUE(value.toInt32() >= 0)
  << "The uint scalar type must contain a value >= 0.";
  ASSERT_EQ(static_cast<uint32_t>(value.toInt32()), expectedValue)
      << "The scalar value must match the expected value.";
}

void CheckBoolScalar(const char* aName, JSContext* aCx,
                     JS::Handle<JS::Value> aSnapshot, bool expectedValue) {
  // Validate the value of the test scalar.
  JS::Rooted<JS::Value> value(aCx);
  JS::Rooted<JSObject*> scalarObj(aCx, &aSnapshot.toObject());
  ASSERT_TRUE(JS_GetProperty(aCx, scalarObj, aName, &value))
  << "The test scalar must be reported.";
  ASSERT_TRUE(value.isBoolean())
  << "The scalar value must be of the correct type.";
  ASSERT_EQ(static_cast<bool>(value.toBoolean()), expectedValue)
      << "The scalar value must match the expected value.";
}

void CheckStringScalar(const char* aName, JSContext* aCx,
                       JS::Handle<JS::Value> aSnapshot,
                       const char* expectedValue) {
  // Validate the value of the test scalar.
  JS::Rooted<JS::Value> value(aCx);
  JS::Rooted<JSObject*> scalarObj(aCx, &aSnapshot.toObject());
  ASSERT_TRUE(JS_GetProperty(aCx, scalarObj, aName, &value))
  << "The test scalar must be reported.";
  ASSERT_TRUE(value.isString())
  << "The scalar value must be of the correct type.";

  bool sameString;
  ASSERT_TRUE(
      JS_StringEqualsAscii(aCx, value.toString(), expectedValue, &sameString))
  << "JS String comparison failed";
  ASSERT_TRUE(sameString)
  << "The scalar value must match the expected string";
}

void CheckKeyedUintScalar(const char* aName, const char* aKey, JSContext* aCx,
                          JS::Handle<JS::Value> aSnapshot,
                          uint32_t expectedValue) {
  JS::Rooted<JS::Value> keyedScalar(aCx);
  JS::Rooted<JSObject*> scalarObj(aCx, &aSnapshot.toObject());
  // Get the aName keyed scalar object from the scalars snapshot.
  ASSERT_TRUE(JS_GetProperty(aCx, scalarObj, aName, &keyedScalar))
  << "The keyed scalar must be reported.";

  CheckUintScalar(aKey, aCx, keyedScalar, expectedValue);
}

void CheckKeyedBoolScalar(const char* aName, const char* aKey, JSContext* aCx,
                          JS::Handle<JS::Value> aSnapshot, bool expectedValue) {
  JS::Rooted<JS::Value> keyedScalar(aCx);
  JS::Rooted<JSObject*> scalarObj(aCx, &aSnapshot.toObject());
  // Get the aName keyed scalar object from the scalars snapshot.
  ASSERT_TRUE(JS_GetProperty(aCx, scalarObj, aName, &keyedScalar))
  << "The keyed scalar must be reported.";

  CheckBoolScalar(aKey, aCx, keyedScalar, expectedValue);
}

void CheckNumberOfProperties(const char* aName, JSContext* aCx,
                             JS::Handle<JS::Value> aSnapshot,
                             uint32_t expectedNumProperties) {
  JS::Rooted<JS::Value> keyedScalar(aCx);
  JS::Rooted<JSObject*> scalarObj(aCx, &aSnapshot.toObject());
  // Get the aName keyed scalar object from the scalars snapshot.
  ASSERT_TRUE(JS_GetProperty(aCx, scalarObj, aName, &keyedScalar))
  << "The keyed scalar must be reported.";

  JS::Rooted<JSObject*> keyedScalarObj(aCx, &keyedScalar.toObject());
  JS::Rooted<JS::IdVector> ids(aCx, JS::IdVector(aCx));
  ASSERT_TRUE(JS_Enumerate(aCx, keyedScalarObj, &ids))
  << "We must be able to get keyed scalar members.";

  ASSERT_EQ(expectedNumProperties, ids.length())
      << "The scalar must report the expected number of properties.";
}

bool EventPresent(JSContext* aCx, const JS::RootedValue& aSnapshot,
                  const nsACString& aCategory, const nsACString& aMethod,
                  const nsACString& aObject) {
  EXPECT_FALSE(aSnapshot.isNullOrUndefined())
      << "Event snapshot must not be null/undefined.";
  bool isArray = false;
  EXPECT_TRUE(JS::IsArrayObject(aCx, aSnapshot, &isArray) && isArray)
      << "The snapshot must be an array.";
  JS::Rooted<JSObject*> arrayObj(aCx, &aSnapshot.toObject());
  uint32_t arrayLength = 0;
  EXPECT_TRUE(JS::GetArrayLength(aCx, arrayObj, &arrayLength))
      << "Array must have a length.";
  EXPECT_TRUE(arrayLength > 0) << "Array must have at least one element.";

  for (uint32_t arrayIdx = 0; arrayIdx < arrayLength; ++arrayIdx) {
    JS::Rooted<JS::Value> element(aCx);
    EXPECT_TRUE(JS_GetElement(aCx, arrayObj, arrayIdx, &element))
        << "Must be able to get element.";
    EXPECT_TRUE(JS::IsArrayObject(aCx, element, &isArray) && isArray)
        << "Element must be an array.";
    JS::Rooted<JSObject*> eventArray(aCx, &element.toObject());
    uint32_t eventLength;
    EXPECT_TRUE(JS::GetArrayLength(aCx, eventArray, &eventLength))
        << "Event array must have a length.";
    EXPECT_TRUE(eventLength >= 4)
        << "Event array must have at least 4 elements (timestamp, category, "
           "method, object).";

    JS::Rooted<JS::Value> str(aCx);
    nsAutoJSString jsStr;
    EXPECT_TRUE(JS_GetElement(aCx, eventArray, 1, &str))
        << "Must be able to get category.";
    EXPECT_TRUE(str.isString()) << "Category must be a string.";
    EXPECT_TRUE(jsStr.init(aCx, str))
        << "Category must be able to be init'd to a jsstring.";
    if (NS_ConvertUTF16toUTF8(jsStr) != aCategory) {
      continue;
    }

    EXPECT_TRUE(JS_GetElement(aCx, eventArray, 2, &str))
        << "Must be able to get method.";
    EXPECT_TRUE(str.isString()) << "Method must be a string.";
    EXPECT_TRUE(jsStr.init(aCx, str))
        << "Method must be able to be init'd to a jsstring.";
    if (NS_ConvertUTF16toUTF8(jsStr) != aMethod) {
      continue;
    }

    EXPECT_TRUE(JS_GetElement(aCx, eventArray, 3, &str))
        << "Must be able to get object.";
    EXPECT_TRUE(str.isString()) << "Object must be a string.";
    EXPECT_TRUE(jsStr.init(aCx, str))
        << "Object must be able to be init'd to a jsstring.";
    if (NS_ConvertUTF16toUTF8(jsStr) != aObject) {
      continue;
    }

    // We found it!
    return true;
  }

  // We didn't find it!
  return false;
}

nsTArray<nsString> EventValuesToArray(JSContext* aCx,
                                      const JS::RootedValue& aSnapshot,
                                      const nsAString& aCategory,
                                      const nsAString& aMethod,
                                      const nsAString& aObject) {
  constexpr int kIndexOfCategory = 1;
  constexpr int kIndexOfMethod = 2;
  constexpr int kIndexOfObject = 3;
  constexpr int kIndexOfValueString = 4;

  nsTArray<nsString> valueArray;
  if (aSnapshot.isNullOrUndefined()) {
    return valueArray;
  }

  bool isArray = false;
  EXPECT_TRUE(JS::IsArrayObject(aCx, aSnapshot, &isArray) && isArray)
      << "The snapshot must be an array.";

  JS::Rooted<JSObject*> arrayObj(aCx, &aSnapshot.toObject());

  uint32_t arrayLength = 0;
  EXPECT_TRUE(JS::GetArrayLength(aCx, arrayObj, &arrayLength))
      << "Array must have a length.";

  JS::Rooted<JS::Value> jsVal(aCx);
  nsAutoJSString jsStr;

  for (uint32_t arrayIdx = 0; arrayIdx < arrayLength; ++arrayIdx) {
    JS::Rooted<JS::Value> element(aCx);
    EXPECT_TRUE(JS_GetElement(aCx, arrayObj, arrayIdx, &element))
        << "Must be able to get element.";

    EXPECT_TRUE(JS::IsArrayObject(aCx, element, &isArray) && isArray)
        << "Element must be an array.";

    JS::Rooted<JSObject*> eventArray(aCx, &element.toObject());
    uint32_t eventLength;
    EXPECT_TRUE(JS::GetArrayLength(aCx, eventArray, &eventLength))
        << "Event array must have a length.";
    EXPECT_TRUE(eventLength >= kIndexOfValueString)
        << "Event array must have at least 4 elements (timestamp, category, "
           "method, object).";

    EXPECT_TRUE(JS_GetElement(aCx, eventArray, kIndexOfCategory, &jsVal))
        << "Must be able to get category.";
    EXPECT_TRUE(jsVal.isString()) << "Category must be a string.";
    EXPECT_TRUE(jsStr.init(aCx, jsVal))
        << "Category must be able to be init'd to a jsstring.";
    if (jsStr != aCategory) {
      continue;
    }

    EXPECT_TRUE(JS_GetElement(aCx, eventArray, kIndexOfMethod, &jsVal))
        << "Must be able to get method.";
    EXPECT_TRUE(jsVal.isString()) << "Method must be a string.";
    EXPECT_TRUE(jsStr.init(aCx, jsVal))
        << "Method must be able to be init'd to a jsstring.";
    if (jsStr != aMethod) {
      continue;
    }

    EXPECT_TRUE(JS_GetElement(aCx, eventArray, kIndexOfObject, &jsVal))
        << "Must be able to get object.";
    EXPECT_TRUE(jsVal.isString()) << "Object must be a string.";
    EXPECT_TRUE(jsStr.init(aCx, jsVal))
        << "Object must be able to be init'd to a jsstring.";
    if (jsStr != aObject) {
      continue;
    }

    if (!JS_GetElement(aCx, eventArray, kIndexOfValueString, &jsVal)) {
      continue;
    }

    nsString str;
    EXPECT_TRUE(AssignJSString(aCx, str, jsVal.toString()))
        << "Value must be able to be init'd to a string.";
    Unused << valueArray.EmplaceBack(std::move(str));
  }

  return valueArray;
}

void GetOriginSnapshot(JSContext* aCx, JS::MutableHandle<JS::Value> aResult,
                       bool aClear) {
  nsCOMPtr<nsITelemetry> telemetry =
      do_GetService("@mozilla.org/base/telemetry;1");

  JS::Rooted<JS::Value> originSnapshot(aCx);
  nsresult rv;
  rv = telemetry->GetOriginSnapshot(aClear, aCx, &originSnapshot);
  ASSERT_EQ(rv, NS_OK) << "Snapshotting origin data must not fail.";
  ASSERT_TRUE(originSnapshot.isObject())
  << "The snapshot must be an object.";

  aResult.set(originSnapshot);
}

/*
 * Extracts the `a` and `b` strings from the prioData snapshot object
 * of any length. Which looks like:
 *
 * [{
 *   encoding: encodingName,
 *   prio: {
 *     a: <string>,
 *     b: <string>,
 *   },
 * }, ...]
 */
void GetEncodedOriginStrings(
    JSContext* aCx, const nsCString& aEncoding,
    nsTArray<Tuple<nsCString, nsCString>>& aPrioStrings) {
  JS::Rooted<JS::Value> snapshot(aCx);
  nsresult rv;
  rv = TelemetryOrigin::GetEncodedOriginSnapshot(false /* clear */, aCx,
                                                 &snapshot);

  ASSERT_FALSE(NS_FAILED(rv));
  ASSERT_FALSE(snapshot.isNullOrUndefined())
  << "Encoded snapshot must not be null/undefined.";

  JS::Rooted<JSObject*> prioDataObj(aCx, &snapshot.toObject());
  bool isArray = false;
  ASSERT_TRUE(JS::IsArrayObject(aCx, prioDataObj, &isArray) && isArray)
  << "The metric's origins must be in an array.";

  uint32_t length = 0;
  ASSERT_TRUE(JS::GetArrayLength(aCx, prioDataObj, &length));
  ASSERT_TRUE(length > 0)
  << "Length of returned array must greater than 0";

  for (auto i = 0u; i < length; ++i) {
    JS::Rooted<JS::Value> arrayItem(aCx);
    ASSERT_TRUE(JS_GetElement(aCx, prioDataObj, i, &arrayItem));
    ASSERT_TRUE(arrayItem.isObject());
    ASSERT_FALSE(arrayItem.isNullOrUndefined());

    JS::Rooted<JSObject*> arrayItemObj(aCx, &arrayItem.toObject());

    JS::Rooted<JS::Value> encodingVal(aCx);
    ASSERT_TRUE(JS_GetProperty(aCx, arrayItemObj, "encoding", &encodingVal));
    ASSERT_TRUE(encodingVal.isString());
    nsAutoJSString jsStr;
    ASSERT_TRUE(jsStr.init(aCx, encodingVal));

    nsPrintfCString encoding(aEncoding.get(),
                             i % TelemetryOrigin::SizeOfPrioDatasPerMetric());
    ASSERT_TRUE(NS_ConvertUTF16toUTF8(jsStr) == encoding)
    << "Actual 'encoding' (" << NS_ConvertUTF16toUTF8(jsStr).get()
    << ") must match expected (" << encoding << ")";

    JS::Rooted<JS::Value> prioVal(aCx);
    ASSERT_TRUE(JS_GetProperty(aCx, arrayItemObj, "prio", &prioVal));
    ASSERT_TRUE(prioVal.isObject());
    ASSERT_FALSE(prioVal.isNullOrUndefined());

    JS::Rooted<JSObject*> prioObj(aCx, &prioVal.toObject());

    JS::Rooted<JS::Value> aVal(aCx);
    nsAutoJSString aStr;
    ASSERT_TRUE(JS_GetProperty(aCx, prioObj, "a", &aVal));
    ASSERT_TRUE(aVal.isString());
    ASSERT_TRUE(aStr.init(aCx, aVal));

    JS::Rooted<JS::Value> bVal(aCx);
    nsAutoJSString bStr;
    ASSERT_TRUE(JS_GetProperty(aCx, prioObj, "b", &bVal));
    ASSERT_TRUE(bVal.isString());
    ASSERT_TRUE(bStr.init(aCx, bVal));

    aPrioStrings.AppendElement(Tuple<nsCString, nsCString>(
        NS_ConvertUTF16toUTF8(aStr), NS_ConvertUTF16toUTF8(bStr)));
  }
}

void GetEventSnapshot(JSContext* aCx, JS::MutableHandle<JS::Value> aResult,
                      ProcessID aProcessType) {
  nsCOMPtr<nsITelemetry> telemetry =
      do_GetService("@mozilla.org/base/telemetry;1");

  JS::Rooted<JS::Value> eventSnapshot(aCx);
  nsresult rv;
  rv = telemetry->SnapshotEvents(1 /* PRERELEASE_CHANNELS */, false /* clear */,
                                 0 /* eventLimit */, aCx, 1 /* argc */,
                                 &eventSnapshot);
  ASSERT_EQ(rv, NS_OK) << "Snapshotting events must not fail.";
  ASSERT_TRUE(eventSnapshot.isObject())
  << "The snapshot must be an object.";

  JS::Rooted<JS::Value> processEvents(aCx);
  JS::Rooted<JSObject*> eventObj(aCx, &eventSnapshot.toObject());
  Unused << JS_GetProperty(aCx, eventObj,
                           Telemetry::Common::GetNameForProcessID(aProcessType),
                           &processEvents);

  aResult.set(processEvents);
}

void GetScalarsSnapshot(bool aKeyed, JSContext* aCx,
                        JS::MutableHandle<JS::Value> aResult,
                        ProcessID aProcessType) {
  nsCOMPtr<nsITelemetry> telemetry =
      do_GetService("@mozilla.org/base/telemetry;1");

  // Get a snapshot of the scalars.
  JS::Rooted<JS::Value> scalarsSnapshot(aCx);
  nsresult rv;

  if (aKeyed) {
    rv = telemetry->GetSnapshotForKeyedScalars(
        "main"_ns, false, false /* filter */, aCx, &scalarsSnapshot);
  } else {
    rv = telemetry->GetSnapshotForScalars("main"_ns, false, false /* filter */,
                                          aCx, &scalarsSnapshot);
  }

  // Validate the snapshot.
  ASSERT_EQ(rv, NS_OK) << "Creating a snapshot of the data must not fail.";
  ASSERT_TRUE(scalarsSnapshot.isObject())
  << "The snapshot must be an object.";

  JS::Rooted<JS::Value> processScalars(aCx);
  JS::Rooted<JSObject*> scalarObj(aCx, &scalarsSnapshot.toObject());
  // Don't complain if no scalars for the process can be found. Just
  // return an empty object.
  Unused << JS_GetProperty(aCx, scalarObj,
                           Telemetry::Common::GetNameForProcessID(aProcessType),
                           &processScalars);

  aResult.set(processScalars);
}

void GetAndClearHistogram(JSContext* cx, nsCOMPtr<nsITelemetry> mTelemetry,
                          const nsACString& name, bool is_keyed) {
  JS::Rooted<JS::Value> testHistogram(cx);
  nsresult rv =
      is_keyed ? mTelemetry->GetKeyedHistogramById(name, cx, &testHistogram)
               : mTelemetry->GetHistogramById(name, cx, &testHistogram);

  ASSERT_EQ(rv, NS_OK) << "Cannot fetch histogram";

  // Clear the stored value
  JS::Rooted<JSObject*> testHistogramObj(cx, &testHistogram.toObject());
  JS::Rooted<JS::Value> rval(cx);
  ASSERT_TRUE(JS_CallFunctionName(cx, testHistogramObj, "clear",
                                  JS::HandleValueArray::empty(), &rval))
  << "Cannot clear histogram";
}

void GetProperty(JSContext* cx, const char* name, JS::Handle<JS::Value> valueIn,
                 JS::MutableHandle<JS::Value> valueOut) {
  JS::Rooted<JS::Value> property(cx);
  JS::Rooted<JSObject*> valueInObj(cx, &valueIn.toObject());
  ASSERT_TRUE(JS_GetProperty(cx, valueInObj, name, &property))
  << "Cannot get property '" << name << "'";
  valueOut.set(property);
}

void GetElement(JSContext* cx, uint32_t index, JS::Handle<JS::Value> valueIn,
                JS::MutableHandle<JS::Value> valueOut) {
  JS::Rooted<JS::Value> element(cx);
  JS::Rooted<JSObject*> valueInObj(cx, &valueIn.toObject());
  ASSERT_TRUE(JS_GetElement(cx, valueInObj, index, &element))
  << "Cannot get element at index '" << index << "'";
  valueOut.set(element);
}

void GetSnapshots(JSContext* cx, nsCOMPtr<nsITelemetry> mTelemetry,
                  const char* name, JS::MutableHandle<JS::Value> valueOut,
                  bool is_keyed) {
  JS::Rooted<JS::Value> snapshots(cx);
  nsresult rv = is_keyed
                    ? mTelemetry->GetSnapshotForKeyedHistograms(
                          "main"_ns, false, false /* filter */, cx, &snapshots)
                    : mTelemetry->GetSnapshotForHistograms(
                          "main"_ns, false, false /* filter */, cx, &snapshots);

  JS::Rooted<JS::Value> snapshot(cx);
  GetProperty(cx, "parent", snapshots, &snapshot);

  ASSERT_EQ(rv, NS_OK) << "Cannot call histogram snapshots";
  valueOut.set(snapshot);
}

}  // namespace TelemetryTestHelpers
