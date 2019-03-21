/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Telemetry.h"
#include "TelemetryOrigin.h"

#include "nsDataHashtable.h"
#include "nsIXULAppInfo.h"
#include "TelemetryCommon.h"
#include "TelemetryOriginEnums.h"
#include "TelemetryOriginData.h"

#include "mozilla/Atomics.h"
#include "mozilla/Base64.h"
#include "mozilla/dom/PrioEncoder.h"
#include "mozilla/StaticMutex.h"

#include <type_traits>

using mozilla::ErrorResult;
using mozilla::MallocSizeOf;
using mozilla::StaticMutex;
using mozilla::StaticMutexAutoLock;
using mozilla::dom::PrioEncoder;
using mozilla::Telemetry::OriginMetricID;
using mozilla::Telemetry::Common::ToJSString;

/***********************************************************************
 *
 * Firefox Origin Telemetry
 * Docs:
 * https://firefox-source-docs.mozilla.org/toolkit/components/telemetry/telemetry/collection/origin.html
 *
 * Origin Telemetry stores pairs of information (metric, origin) which boils
 * down to "$metric happened on $origin".
 *
 * Prio can only encode up-to-2046-length bit vectors. The process of
 * transforming these pairs of information into bit vectors is called "App
 * Encoding". The bit vectors are then "Prio Encoded" into binary goop. The
 * binary goop is then "Base64 Encoded" into strings.
 *
 */


////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// PRIVATE TYPES

namespace {

class OriginMetricIDHashKey : public PLDHashEntryHdr {
 public:
  typedef const OriginMetricID& KeyType;
  typedef const OriginMetricID* KeyTypePointer;

  explicit OriginMetricIDHashKey(KeyTypePointer aKey) : mValue(*aKey) {}
  OriginMetricIDHashKey(OriginMetricIDHashKey&& aOther)
      : PLDHashEntryHdr(std::move(aOther)), mValue(std::move(aOther.mValue)) {}
  ~OriginMetricIDHashKey() {}

  KeyType GetKey() const { return mValue; }
  bool KeyEquals(KeyTypePointer aKey) const { return *aKey == mValue; }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }
  static PLDHashNumber HashKey(KeyTypePointer aKey) {
    return static_cast<std::underlying_type<OriginMetricID>::type>(*aKey);
  }
  enum { ALLOW_MEMMOVE = true };

 private:
  const OriginMetricID mValue;
};

}  // anonymous namespace

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// PRIVATE STATE, SHARED BY ALL THREADS

namespace {

// This is a StaticMutex rather than a plain Mutex (1) so that
// it gets initialised in a thread-safe manner the first time
// it is used, and (2) because it is never de-initialised, and
// a normal Mutex would show up as a leak in BloatView.  StaticMutex
// also has the "OffTheBooks" property, so it won't show as a leak
// in BloatView.
// Another reason to use a StaticMutex instead of a plain Mutex is
// that, due to the nature of Telemetry, we cannot rely on having a
// mutex initialized in InitializeGlobalState. Unfortunately, we
// cannot make sure that no other function is called before this point.
static StaticMutex gTelemetryOriginMutex;

typedef nsDataHashtable<nsCStringHashKey, size_t> OriginToIndexMap;
OriginToIndexMap* gOriginToIndexMap;

typedef nsDataHashtable<OriginMetricIDHashKey, nsTArray<nsCString>>
    IdToOriginsMap;

IdToOriginsMap* gMetricToOriginsMap;

mozilla::Atomic<bool, mozilla::Relaxed> gInitDone(false);

// Useful for app-encoded data
typedef nsDataHashtable<OriginMetricIDHashKey, nsTArray<bool>> IdToBoolsMap;

static nsCString gBatchID;
#define CANARY_BATCH_ID "decaffcoffee"

}  // namespace

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// PRIVATE: thread-safe helpers

namespace {

const char* GetNameForMetricID(OriginMetricID aId) {
  MOZ_ASSERT(aId < OriginMetricID::Count);
  return mozilla::Telemetry::MetricIDToString[static_cast<uint32_t>(aId)];
}

nsresult AppEncodeTo(const StaticMutexAutoLock& lock, IdToBoolsMap& aResult) {
  // TODO: support sharding for an origins list longer than gNumBooleans.
  // For now, assert that it's not a problem.
  MOZ_ASSERT(gOriginsList.Length() <= PrioEncoder::gNumBooleans);

  auto iter = gMetricToOriginsMap->ConstIter();
  for (; !iter.Done(); iter.Next()) {
    OriginMetricID id = iter.Key();

    nsTArray<bool> metricData(gOriginsList.Length());
    metricData.SetLength(gOriginsList.Length());
    for (auto& metricDatum : metricData) {
      metricDatum = false;
    }

    for (const auto& origin : iter.Data()) {
      size_t index;
      if (!gOriginToIndexMap->Get(origin, &index)) {
        return NS_ERROR_FAILURE;
      }
      MOZ_ASSERT(index < gOriginsList.Length());
      metricData[index] = true;
    }
    aResult.Put(id, metricData);
  }
  return NS_OK;
}

}  // anonymous namespace

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// EXTERNALLY VISIBLE FUNCTIONS in namespace TelemetryOrigin::

void TelemetryOrigin::InitializeGlobalState() {
  if (!XRE_IsParentProcess()) {
    return;
  }

  StaticMutexAutoLock locker(gTelemetryOriginMutex);

  MOZ_ASSERT(!gInitDone,
             "TelemetryOrigin::InitializeGlobalState "
             "may only be called once");

  gOriginToIndexMap = new OriginToIndexMap(gOriginsList.Length());
  for (size_t i = 0; i < gOriginsList.Length(); ++i) {
    gOriginToIndexMap->Put(nsDependentCString(gOriginsList[i]), i);
  }

  gMetricToOriginsMap = new IdToOriginsMap();

  // This map shouldn't change at runtime, so make debug builds complain
  // if it tries.
#ifdef DEBUG
  gOriginToIndexMap->MarkImmutable();
#endif  // DEBUG

  // We use the app's buildid for the prio batch ID
  nsCOMPtr<nsIXULAppInfo> appInfo =
      do_GetService("@mozilla.org/xre/app-info;1");
  if (!appInfo || NS_FAILED(appInfo->GetAppBuildID(gBatchID))) {
    // Some tests forget to set either of build ID or xpc::IsInAutomation(),
    // so all we can do is warn.
    NS_WARNING("Cannot get app build ID. Defaulting to canary.");
    gBatchID.AssignLiteral(CANARY_BATCH_ID);
  }

  gInitDone = true;
}

void TelemetryOrigin::DeInitializeGlobalState() {
  if (!XRE_IsParentProcess()) {
    return;
  }

  StaticMutexAutoLock locker(gTelemetryOriginMutex);
  MOZ_ASSERT(gInitDone);

  if (!gInitDone) {
    return;
  }

  delete gOriginToIndexMap;
  gOriginToIndexMap = nullptr;

  delete gMetricToOriginsMap;
  gMetricToOriginsMap = nullptr;

  gInitDone = false;
}

nsresult TelemetryOrigin::RecordOrigin(OriginMetricID aId,
                                       const nsACString& aOrigin) {
  if (!XRE_IsParentProcess()) {
    return NS_ERROR_FAILURE;
  }

  StaticMutexAutoLock locker(gTelemetryOriginMutex);

  // Common Telemetry error-handling practices for recording functions:
  // only illegal calls return errors whereas merely incorrect ones are mutely
  // ignored.
  if (!gInitDone) {
    return NS_OK;
  }

  if (!gOriginToIndexMap->Contains(aOrigin)) {
    return NS_OK;
  }

  auto& originArray = gMetricToOriginsMap->GetOrInsert(aId);
  originArray.AppendElement(aOrigin);

  return NS_OK;
}

nsresult TelemetryOrigin::GetOriginSnapshot(bool aClear, JSContext* aCx,
                                            JS::MutableHandleValue aResult) {
  if (NS_WARN_IF(!XRE_IsParentProcess())) {
    return NS_ERROR_FAILURE;
  }

  if (!gInitDone) {
    return NS_OK;
  }

  // Step 1: Grab the lock, copy into stack-local storage, optionally clear.
  IdToOriginsMap copy;
  {
    StaticMutexAutoLock locker(gTelemetryOriginMutex);

    if (aClear) {
      // I'd really prefer to clear after we're sure the snapshot didn't go
      // awry, but we can't hold a lock preventing recording while using JS
      // APIs. And replaying any interleaving recording sounds like too much
      // squeeze for not enough juice.

      gMetricToOriginsMap->SwapElements(copy);
    } else {
      auto iter = gMetricToOriginsMap->ConstIter();
      for (; !iter.Done(); iter.Next()) {
        copy.Put(iter.Key(), iter.Data());
      }
    }
  }

  // Step 2: Without the lock, generate JS datastructure for snapshotting
  JS::Rooted<JSObject*> rootObj(aCx, JS_NewPlainObject(aCx));
  if (NS_WARN_IF(!rootObj)) {
    return NS_ERROR_FAILURE;
  }
  aResult.setObject(*rootObj);
  for (auto iter = copy.ConstIter(); !iter.Done(); iter.Next()) {
    const auto& origins = iter.Data();
    JS::RootedObject originsArrayObj(aCx,
                                     JS_NewArrayObject(aCx, origins.Length()));
    if (NS_WARN_IF(!originsArrayObj)) {
      return NS_ERROR_FAILURE;
    }
    if (!JS_DefineProperty(aCx, rootObj, GetNameForMetricID(iter.Key()),
                           originsArrayObj, JSPROP_ENUMERATE)) {
      NS_WARNING("Failed to define property in origin snapshot.");
      return NS_ERROR_FAILURE;
    }

    for (uint32_t i = 0; i < origins.Length(); ++i) {
      JS::RootedValue origin(aCx);
      origin.setString(ToJSString(aCx, origins[i]));
      if (!JS_DefineElement(aCx, originsArrayObj, i, origin,
                            JSPROP_ENUMERATE)) {
        NS_WARNING("Failed to define element in origin snapshot array.");
        return NS_ERROR_FAILURE;
      }
    }
  }

  return NS_OK;
}

nsresult TelemetryOrigin::GetEncodedOriginSnapshot(
    bool aClear, JSContext* aCx, JS::MutableHandleValue aSnapshot) {
  if (!XRE_IsParentProcess()) {
    return NS_ERROR_FAILURE;
  }

  if (!gInitDone) {
    return NS_OK;
  }

  // Step 1: Take the lock and app-encode
  nsresult rv;
  IdToBoolsMap appEncodedMetricData;
  {
    StaticMutexAutoLock lock(gTelemetryOriginMutex);

    rv = AppEncodeTo(lock, appEncodedMetricData);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  // Step 2: Don't need the lock to prio-encode and base64-encode and JS-encode

  JS::RootedObject prioDataArray(
      aCx, JS_NewArrayObject(aCx, appEncodedMetricData.Count()));
  if (NS_WARN_IF(!prioDataArray)) {
    return NS_ERROR_FAILURE;
  }

  auto it = appEncodedMetricData.ConstIter();
  uint32_t i = 0;
  for (; !it.Done(); it.Next()) {
    nsCString aResult;
    nsCString bResult;
    rv = PrioEncoder::EncodeNative(gBatchID, it.Data(), aResult, bResult);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    nsCString aBase64;
    rv = mozilla::Base64Encode(aResult, aBase64);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    nsCString bBase64;
    rv = mozilla::Base64Encode(bResult, bBase64);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    JS::RootedObject rootObj(aCx, JS_NewPlainObject(aCx));
    if (NS_WARN_IF(!rootObj)) {
      return NS_ERROR_FAILURE;
    }
    JSString* metricName =
        ToJSString(aCx, nsDependentCString(GetNameForMetricID(it.Key())));
    JS::RootedString rootStr(aCx, metricName);
    if (NS_WARN_IF(!JS_DefineProperty(aCx, rootObj, "encoding", rootStr,
                                      JSPROP_ENUMERATE))) {
      return NS_ERROR_FAILURE;
    }

    JS::RootedObject prioObj(aCx, JS_NewPlainObject(aCx));
    if (NS_WARN_IF(!prioObj)) {
      return NS_ERROR_FAILURE;
    }
    if (NS_WARN_IF(!JS_DefineProperty(aCx, rootObj, "prio", prioObj,
                                      JSPROP_ENUMERATE))) {
      return NS_ERROR_FAILURE;
    }

    JS::RootedString aRootStr(aCx, ToJSString(aCx, aBase64));
    if (NS_WARN_IF(!JS_DefineProperty(aCx, prioObj, "a", aRootStr,
                                      JSPROP_ENUMERATE))) {
      return NS_ERROR_FAILURE;
    }
    JS::RootedString bRootStr(aCx, ToJSString(aCx, bBase64));
    if (NS_WARN_IF(!JS_DefineProperty(aCx, prioObj, "b", bRootStr,
                                      JSPROP_ENUMERATE))) {
      return NS_ERROR_FAILURE;
    }

    if (NS_WARN_IF(!JS_DefineElement(aCx, prioDataArray, i++, rootObj,
                                     JSPROP_ENUMERATE))) {
      return NS_ERROR_FAILURE;
    }
  }

  // Step 4: If we need to clear, we'll need that lock again
  if (aClear) {
    StaticMutexAutoLock lock(gTelemetryOriginMutex);
    gMetricToOriginsMap->Clear();
  }

  aSnapshot.setObject(*prioDataArray);

  return NS_OK;
}

/**
 * Resets all the stored events. This is intended to be only used in tests.
 */
void TelemetryOrigin::ClearOrigins() {
  StaticMutexAutoLock lock(gTelemetryOriginMutex);

  if (!gInitDone) {
    return;
  }

  gMetricToOriginsMap->Clear();
}

size_t TelemetryOrigin::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) {
  StaticMutexAutoLock locker(gTelemetryOriginMutex);
  size_t n = 0;

  if (!gInitDone) {
    return 0;
  }

  n += gMetricToOriginsMap->ShallowSizeOfIncludingThis(aMallocSizeOf);
  auto iter = gMetricToOriginsMap->ConstIter();
  for (; !iter.Done(); iter.Next()) {
    n += iter.Data().ShallowSizeOfIncludingThis(aMallocSizeOf);
    for (const auto& origin : iter.Data()) {
      // nsTArray's shallow size should include the strings' `this`
      n += origin.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
    }
  }

  // The string hashkey and ID should both be contained within the hashtable.
  n += gOriginToIndexMap->ShallowSizeOfIncludingThis(aMallocSizeOf);

  return n;
}
