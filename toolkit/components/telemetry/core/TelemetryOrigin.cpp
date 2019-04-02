/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Telemetry.h"
#include "TelemetryOrigin.h"

#include "nsDataHashtable.h"
#include "nsIObserverService.h"
#include "nsPrintfCString.h"
#include "TelemetryCommon.h"
#include "TelemetryOriginEnums.h"

#include "mozilla/Atomics.h"
#include "mozilla/Base64.h"
#include "mozilla/dom/PrioEncoder.h"
#include "mozilla/Preferences.h"
#include "mozilla/Pair.h"
#include "mozilla/Services.h"
#include "mozilla/StaticMutex.h"

#include <cmath>
#include <type_traits>

using mozilla::ErrorResult;
using mozilla::MakePair;
using mozilla::MallocSizeOf;
using mozilla::Pair;
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
//
// Access for all of this state (except gInitDone) must be guarded by
// gTelemetryOriginMutex.

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

nsTArray<const char*>* gOriginsList = nullptr;

typedef nsDataHashtable<nsCStringHashKey, size_t> OriginToIndexMap;
OriginToIndexMap* gOriginToIndexMap;

typedef nsDataHashtable<OriginMetricIDHashKey, nsTArray<nsCString>>
    IdToOriginsMap;

IdToOriginsMap* gMetricToOriginsMap;

mozilla::Atomic<bool, mozilla::Relaxed> gInitDone(false);

// Useful for app-encoded data
typedef nsTArray<Pair<OriginMetricID, nsTArray<nsTArray<bool>>>>
    IdBoolsPairArray;

// The number of prioData elements needed to encode the contents of storage.
// Will be some whole multiple of gPrioDatasPerMetric.
static uint32_t gPrioDataCount = 0;

// Prio has a maximum supported number of bools it can encode at a time.
// This means a single metric may result in several encoded payloads if the
// number of origins exceeds the number of bools.
// Each encoded payload corresponds to an element in the `prioData` array in the
// "prio" ping.
// This number is the number of encoded payloads needed per metric, and is
// equivalent to "how many bitvectors do we need to encode this whole list of
// origins?"
static uint32_t gPrioDatasPerMetric;

// The number of "meta-origins": in-band metadata about origin telemetry.
// Currently 1: the "unknown origin recorded" meta-origin.
static uint32_t kNumMetaOrigins = 1;

NS_NAMED_LITERAL_CSTRING(kUnknownOrigin, "__UNKNOWN__");

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

nsresult AppEncodeTo(const StaticMutexAutoLock& lock,
                     IdBoolsPairArray& aResult) {
  auto iter = gMetricToOriginsMap->ConstIter();
  for (; !iter.Done(); iter.Next()) {
    OriginMetricID id = iter.Key();

    // Fill in the result bool vectors with `false`s.
    nsTArray<nsTArray<bool>> metricData(gPrioDatasPerMetric);
    metricData.SetLength(gPrioDatasPerMetric);
    for (size_t i = 0; i < metricData.Length() - 1; ++i) {
      metricData[i].SetLength(PrioEncoder::gNumBooleans);
      for (auto& metricDatum : metricData[i]) {
        metricDatum = false;
      }
    }
    auto& lastArray = metricData[metricData.Length() - 1];
    lastArray.SetLength(gOriginsList->Length() % PrioEncoder::gNumBooleans);
    for (auto& metricDatum : lastArray) {
      metricDatum = false;
    }

    for (const auto& origin : iter.Data()) {
      size_t index;
      if (!gOriginToIndexMap->Get(origin, &index)) {
        return NS_ERROR_FAILURE;
      }
      MOZ_ASSERT(index < gOriginsList->Length());
      size_t shardIndex =
          ceil(static_cast<double>(index) / PrioEncoder::gNumBooleans);
      MOZ_ASSERT(shardIndex < metricData.Length());
      MOZ_ASSERT(index % PrioEncoder::gNumBooleans <
                 metricData[shardIndex].Length());
      metricData[shardIndex][index % PrioEncoder::gNumBooleans] = true;
    }
    aResult.AppendElement(MakePair(id, metricData));
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

  // The contents and order of this array matters.
  // Both ensure a consistent app-encoding.
  gOriginsList = new nsTArray<const char*>({
      "doubleclick.de",
      "fb.com",
  });

  gPrioDatasPerMetric =
      ceil(static_cast<double>(gOriginsList->Length() + kNumMetaOrigins) /
           PrioEncoder::gNumBooleans);

  gOriginToIndexMap = new OriginToIndexMap(gOriginsList->Length());
  for (size_t i = 0; i < gOriginsList->Length(); ++i) {
    MOZ_ASSERT(!kUnknownOrigin.Equals((*gOriginsList)[i]),
               "Unknown origin literal is reserved in Origin Telemetry");
    gOriginToIndexMap->Put(nsDependentCString((*gOriginsList)[i]), i);
  }

  // Add the meta-origin for tracking recordings to untracked origins.
  gOriginToIndexMap->Put(kUnknownOrigin, gOriginsList->Length());

  gMetricToOriginsMap = new IdToOriginsMap();

  // This map shouldn't change at runtime, so make debug builds complain
  // if it tries.
#ifdef DEBUG
  gOriginToIndexMap->MarkImmutable();
#endif  // DEBUG

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

  delete gOriginsList;

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

  nsCString origin(aOrigin);
  if (!gOriginToIndexMap->Contains(aOrigin)) {
    // Only record one unknown origin per metric per snapshot.
    // (otherwise we may get swamped and blow our data budget.)
    if (gMetricToOriginsMap->Contains(aId) &&
        gMetricToOriginsMap->GetOrInsert(aId).Contains(kUnknownOrigin)) {
      return NS_OK;
    }
    origin = kUnknownOrigin;
  }

  if (!gMetricToOriginsMap->Contains(aId)) {
    // If we haven't recorded anything for this metric yet, we're adding some
    // prioDatas.
    gPrioDataCount += gPrioDatasPerMetric;
  }

  auto& originArray = gMetricToOriginsMap->GetOrInsert(aId);

  if (originArray.Contains(origin)) {
    // If we've already recorded this metric for this origin, then we're going
    // to need more prioDatas to encode that it happened again.
    gPrioDataCount += gPrioDatasPerMetric;
  }

  originArray.AppendElement(origin);

  static uint32_t sPrioPingLimit =
      mozilla::Preferences::GetUint("toolkit.telemetry.prioping.dataLimit", 10);
  if (gPrioDataCount >= sPrioPingLimit) {
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os) {
      os->NotifyObservers(nullptr, "origin-telemetry-storage-limit-reached",
                          nullptr);
    }
  }

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
      gPrioDataCount = 0;
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

  // Step 1: Take the lock and app-encode. Optionally clear.
  nsresult rv;
  IdBoolsPairArray appEncodedMetricData;
  {
    StaticMutexAutoLock lock(gTelemetryOriginMutex);

    rv = AppEncodeTo(lock, appEncodedMetricData);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (aClear) {
      // I'd really prefer to clear after we're sure the snapshot didn't go
      // awry, but we can't hold a lock preventing recording while using JS
      // APIs. And replaying any interleaving recording sounds like too much
      // squeeze for not enough juice.

      gMetricToOriginsMap->Clear();
    }
  }

  // Step 2: Don't need the lock to prio-encode and base64-encode
  nsTArray<Pair<nsCString, Pair<nsCString, nsCString>>> prioData;
  for (auto& metricData : appEncodedMetricData) {
    auto& boolVectors = metricData.second();
    for (uint32_t i = 0; i < boolVectors.Length(); ++i) {
      // "encoding" is of the form `metricName-X` where X is the shard index.
      nsCString encodingName =
          nsPrintfCString("%s-%u", GetNameForMetricID(metricData.first()), i);
      nsCString aResult;
      nsCString bResult;
      rv = PrioEncoder::EncodeNative(encodingName, boolVectors[i], aResult,
                                     bResult);
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

      prioData.AppendElement(
          MakePair(encodingName, MakePair(aBase64, bBase64)));
    }
  }

  // Step 3: Still don't need the lock to translate to JS
  // The resulting data structure is:
  // [{
  //   encoding: <encoding name>,
  //   prio: {
  //     a: <base64 string>,
  //     b: <base64 string>,
  //   },
  // }, ...]

  JS::RootedObject prioDataArray(aCx,
                                 JS_NewArrayObject(aCx, prioData.Length()));
  if (NS_WARN_IF(!prioDataArray)) {
    return NS_ERROR_FAILURE;
  }
  uint32_t i = 0;
  for (auto& prioDatum : prioData) {
    JS::RootedObject prioDatumObj(aCx, JS_NewPlainObject(aCx));
    if (NS_WARN_IF(!prioDatumObj)) {
      return NS_ERROR_FAILURE;
    }
    JSString* encoding = ToJSString(aCx, prioDatum.first());
    JS::RootedString rootedEncoding(aCx, encoding);
    if (NS_WARN_IF(!JS_DefineProperty(aCx, prioDatumObj, "encoding",
                                      rootedEncoding, JSPROP_ENUMERATE))) {
      return NS_ERROR_FAILURE;
    }

    JS::RootedObject prioObj(aCx, JS_NewPlainObject(aCx));
    if (NS_WARN_IF(!prioObj)) {
      return NS_ERROR_FAILURE;
    }
    if (NS_WARN_IF(!JS_DefineProperty(aCx, prioDatumObj, "prio", prioObj,
                                      JSPROP_ENUMERATE))) {
      return NS_ERROR_FAILURE;
    }

    JS::RootedString aRootStr(aCx, ToJSString(aCx, prioDatum.second().first()));
    if (NS_WARN_IF(!JS_DefineProperty(aCx, prioObj, "a", aRootStr,
                                      JSPROP_ENUMERATE))) {
      return NS_ERROR_FAILURE;
    }
    JS::RootedString bRootStr(aCx,
                              ToJSString(aCx, prioDatum.second().second()));
    if (NS_WARN_IF(!JS_DefineProperty(aCx, prioObj, "b", bRootStr,
                                      JSPROP_ENUMERATE))) {
      return NS_ERROR_FAILURE;
    }

    if (NS_WARN_IF(!JS_DefineElement(aCx, prioDataArray, i++, prioDatumObj,
                                     JSPROP_ENUMERATE))) {
      return NS_ERROR_FAILURE;
    }
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
