/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/HangAnnotations.h"
#include "ThreadHangStats.h"
#include "nsITelemetry.h"
#include "HangReports.h"
#include "jsapi.h"

namespace {

using namespace mozilla;
using namespace mozilla::HangMonitor;
using namespace mozilla::Telemetry;

static JSObject*
CreateJSTimeHistogram(JSContext* cx, const Telemetry::TimeHistogram& time)
{
  /* Create JS representation of TimeHistogram,
     in the format of Chromium-style histograms. */
  JS::RootedObject ret(cx, JS_NewPlainObject(cx));
  if (!ret) {
    return nullptr;
  }

  if (!JS_DefineProperty(cx, ret, "min", time.GetBucketMin(0),
                         JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, ret, "max",
                         time.GetBucketMax(ArrayLength(time) - 1),
                         JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, ret, "histogram_type",
                         nsITelemetry::HISTOGRAM_EXPONENTIAL,
                         JSPROP_ENUMERATE)) {
    return nullptr;
  }
  // TODO: calculate "sum"
  if (!JS_DefineProperty(cx, ret, "sum", 0, JSPROP_ENUMERATE)) {
    return nullptr;
  }

  JS::RootedObject ranges(
    cx, JS_NewArrayObject(cx, ArrayLength(time) + 1));
  JS::RootedObject counts(
    cx, JS_NewArrayObject(cx, ArrayLength(time) + 1));
  if (!ranges || !counts) {
    return nullptr;
  }
  /* In a Chromium-style histogram, the first bucket is an "under" bucket
     that represents all values below the histogram's range. */
  if (!JS_DefineElement(cx, ranges, 0, time.GetBucketMin(0), JSPROP_ENUMERATE) ||
      !JS_DefineElement(cx, counts, 0, 0, JSPROP_ENUMERATE)) {
    return nullptr;
  }
  for (size_t i = 0; i < ArrayLength(time); i++) {
    if (!JS_DefineElement(cx, ranges, i + 1, time.GetBucketMax(i),
                          JSPROP_ENUMERATE) ||
        !JS_DefineElement(cx, counts, i + 1, time[i], JSPROP_ENUMERATE)) {
      return nullptr;
    }
  }
  if (!JS_DefineProperty(cx, ret, "ranges", ranges, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, ret, "counts", counts, JSPROP_ENUMERATE)) {
    return nullptr;
  }
  return ret;
}

static JSObject*
CreateJSHangStack(JSContext* cx, const Telemetry::HangStack& stack)
{
  JS::RootedObject ret(cx, JS_NewArrayObject(cx, stack.length()));
  if (!ret) {
    return nullptr;
  }
  for (size_t i = 0; i < stack.length(); i++) {
    JS::RootedString string(cx, JS_NewStringCopyZ(cx, stack[i]));
    if (!JS_DefineElement(cx, ret, i, string, JSPROP_ENUMERATE)) {
      return nullptr;
    }
  }
  return ret;
}

static void
CreateJSHangAnnotations(JSContext* cx, const HangAnnotationsVector& annotations,
                        JS::MutableHandleObject returnedObject)
{
  JS::RootedObject annotationsArray(cx, JS_NewArrayObject(cx, 0));
  if (!annotationsArray) {
    returnedObject.set(nullptr);
    return;
  }
  // We keep track of the annotations we reported in this hash set, so we can
  // discard duplicated ones.
  nsTHashtable<nsStringHashKey> reportedAnnotations;
  size_t annotationIndex = 0;
  for (const auto & curAnnotations : annotations) {
    JS::RootedObject jsAnnotation(cx, JS_NewPlainObject(cx));
    if (!jsAnnotation) {
      continue;
    }
    // Build a key to index the current annotations in our hash set.
    nsAutoString annotationsKey;
    nsresult rv = ComputeAnnotationsKey(curAnnotations, annotationsKey);
    if (NS_FAILED(rv)) {
      continue;
    }
    // Check if the annotations are in the set. If that's the case, don't double report.
    if (reportedAnnotations.GetEntry(annotationsKey)) {
      continue;
    }
    // If not, report them.
    reportedAnnotations.PutEntry(annotationsKey);
    UniquePtr<HangAnnotations::Enumerator> annotationsEnum =
      curAnnotations->GetEnumerator();
    if (!annotationsEnum) {
      continue;
    }
    nsAutoString key;
    nsAutoString value;
    while (annotationsEnum->Next(key, value)) {
      JS::RootedValue jsValue(cx);
      jsValue.setString(JS_NewUCStringCopyN(cx, value.get(), value.Length()));
      if (!JS_DefineUCProperty(cx, jsAnnotation, key.get(), key.Length(),
                               jsValue, JSPROP_ENUMERATE)) {
        returnedObject.set(nullptr);
        return;
      }
    }
    if (!JS_SetElement(cx, annotationsArray, annotationIndex, jsAnnotation)) {
      continue;
    }
    ++annotationIndex;
  }
  // Return the array using a |MutableHandleObject| to avoid triggering a false
  // positive rooting issue in the hazard analysis build.
  returnedObject.set(annotationsArray);
}

static JSObject*
CreateJSHangHistogram(JSContext* cx, const Telemetry::HangHistogram& hang)
{
  JS::RootedObject ret(cx, JS_NewPlainObject(cx));
  if (!ret) {
    return nullptr;
  }

  JS::RootedObject stack(cx, CreateJSHangStack(cx, hang.GetStack()));
  JS::RootedObject time(cx, CreateJSTimeHistogram(cx, hang));
  auto& hangAnnotations = hang.GetAnnotations();
  JS::RootedObject annotations(cx);
  CreateJSHangAnnotations(cx, hangAnnotations, &annotations);

  if (!stack ||
      !time ||
      !annotations ||
      !JS_DefineProperty(cx, ret, "stack", stack, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, ret, "histogram", time, JSPROP_ENUMERATE) ||
      (!hangAnnotations.empty() && // <-- Only define annotations when nonempty
        !JS_DefineProperty(cx, ret, "annotations", annotations, JSPROP_ENUMERATE))) {
    return nullptr;
  }

  return ret;
}

} // namespace

namespace mozilla {
namespace Telemetry {

JSObject*
CreateJSThreadHangStats(JSContext* cx, const Telemetry::ThreadHangStats& thread)
{
  JS::RootedObject ret(cx, JS_NewPlainObject(cx));
  if (!ret) {
    return nullptr;
  }
  JS::RootedString name(cx, JS_NewStringCopyZ(cx, thread.GetName()));
  if (!name ||
      !JS_DefineProperty(cx, ret, "name", name, JSPROP_ENUMERATE)) {
    return nullptr;
  }

  JS::RootedObject activity(cx, CreateJSTimeHistogram(cx, thread.mActivity));
  if (!activity ||
      !JS_DefineProperty(cx, ret, "activity", activity, JSPROP_ENUMERATE)) {
    return nullptr;
  }

  // Process the hangs into a hangs object.
  JS::RootedObject hangs(cx, JS_NewArrayObject(cx, 0));
  if (!hangs) {
    return nullptr;
  }
  for (size_t i = 0; i < thread.mHangs.length(); i++) {
    JS::RootedObject obj(cx, CreateJSHangHistogram(cx, thread.mHangs[i]));
    if (!ret) {
      return nullptr;
    }

    JS::RootedString runnableName(cx, JS_NewStringCopyZ(cx, thread.mHangs[i].GetRunnableName()));
    if (!runnableName ||
        !JS_DefineProperty(cx, ret, "runnableName", runnableName, JSPROP_ENUMERATE)) {
      return nullptr;
    }

    // Check if we have a cached native stack index, and if we do record it.
    uint32_t index = thread.mHangs[i].GetNativeStackIndex();
    if (index != Telemetry::HangHistogram::NO_NATIVE_STACK_INDEX) {
      if (!JS_DefineProperty(cx, obj, "nativeStack", index, JSPROP_ENUMERATE)) {
        return nullptr;
      }
    }

    if (!JS_DefineElement(cx, hangs, i, obj, JSPROP_ENUMERATE)) {
      return nullptr;
    }
  }
  if (!JS_DefineProperty(cx, ret, "hangs", hangs, JSPROP_ENUMERATE)) {
    return nullptr;
  }

  // We should already have a CombinedStacks object on the ThreadHangStats, so
  // add that one.
  JS::RootedObject fullReportObj(cx, CreateJSStackObject(cx, thread.mCombinedStacks));
  if (!fullReportObj) {
    return nullptr;
  }

  if (!JS_DefineProperty(cx, ret, "nativeStacks", fullReportObj, JSPROP_ENUMERATE)) {
    return nullptr;
  }

  return ret;
}

void
TimeHistogram::Add(PRIntervalTime aTime)
{
  uint32_t timeMs = PR_IntervalToMilliseconds(aTime);
  size_t index = mozilla::FloorLog2(timeMs);
  operator[](index)++;
}

const char*
HangStack::InfallibleAppendViaBuffer(const char* aText, size_t aLength)
{
  MOZ_ASSERT(this->canAppendWithoutRealloc(1));
  // Include null-terminator in length count.
  MOZ_ASSERT(mBuffer.canAppendWithoutRealloc(aLength + 1));

  const char* const entry = mBuffer.end();
  mBuffer.infallibleAppend(aText, aLength);
  mBuffer.infallibleAppend('\0'); // Explicitly append null-terminator
  this->infallibleAppend(entry);
  return entry;
}

const char*
HangStack::AppendViaBuffer(const char* aText, size_t aLength)
{
  if (!this->reserve(this->length() + 1)) {
    return nullptr;
  }

  // Keep track of the previous buffer in case we need to adjust pointers later.
  const char* const prevStart = mBuffer.begin();
  const char* const prevEnd = mBuffer.end();

  // Include null-terminator in length count.
  if (!mBuffer.reserve(mBuffer.length() + aLength + 1)) {
    return nullptr;
  }

  if (prevStart != mBuffer.begin()) {
    // The buffer has moved; we have to adjust pointers in the stack.
    for (auto & entry : *this) {
      if (entry >= prevStart && entry < prevEnd) {
        // Move from old buffer to new buffer.
        entry += mBuffer.begin() - prevStart;
      }
    }
  }

  return InfallibleAppendViaBuffer(aText, aLength);
}

uint32_t
HangHistogram::GetHash(const HangStack& aStack)
{
  uint32_t hash = 0;
  for (const char* const* label = aStack.begin();
       label != aStack.end(); label++) {
    /* If the string is within our buffer, we need to hash its content.
       Otherwise, the string is statically allocated, and we only need
       to hash the pointer instead of the content. */
    if (aStack.IsInBuffer(*label)) {
      hash = AddToHash(hash, HashString(*label));
    } else {
      hash = AddToHash(hash, *label);
    }
  }
  return hash;
}

bool
HangHistogram::operator==(const HangHistogram& aOther) const
{
  if (mHash != aOther.mHash) {
    return false;
  }
  if (mStack.length() != aOther.mStack.length()) {
    return false;
  }
  return mStack == aOther.mStack;
}

} // namespace Telemetry
} // namespace mozilla
