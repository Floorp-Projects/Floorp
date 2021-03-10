/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Telemetry.h"
#include "TelemetryOrigin.h"

#include "nsTHashMap.h"
#include "nsIObserverService.h"
#include "nsPrintfCString.h"
#include "nsTArray.h"
#include "TelemetryCommon.h"
#include "TelemetryOriginEnums.h"

#include "js/Array.h"  // JS::NewArrayObject
#include "mozilla/Atomics.h"
#include "mozilla/Base64.h"
#include "mozilla/dom/PrioEncoder.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/Tuple.h"
#include "mozilla/UniquePtr.h"

#include <cmath>
#include <type_traits>

using mozilla::Get;
using mozilla::MakeTuple;
using mozilla::MakeUnique;
using mozilla::MallocSizeOf;
using mozilla::StaticMutex;
using mozilla::StaticMutexAutoLock;
using mozilla::Tuple;
using mozilla::UniquePtr;
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
  ~OriginMetricIDHashKey() = default;

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

typedef nsTArray<Tuple<const char*, const char*>> OriginHashesList;
UniquePtr<OriginHashesList> gOriginHashesList;

typedef nsTHashMap<nsCStringHashKey, size_t> OriginToIndexMap;
UniquePtr<OriginToIndexMap> gOriginToIndexMap;

typedef nsTHashMap<nsCStringHashKey, size_t> HashToIndexMap;
UniquePtr<HashToIndexMap> gHashToIndexMap;

typedef nsTHashMap<nsCStringHashKey, uint32_t> OriginBag;
typedef nsTHashMap<OriginMetricIDHashKey, OriginBag> IdToOriginBag;

UniquePtr<IdToOriginBag> gMetricToOriginBag;

mozilla::Atomic<bool, mozilla::Relaxed> gInitDone(false);

// Useful for app-encoded data
typedef nsTArray<std::pair<OriginMetricID, nsTArray<nsTArray<bool>>>>
    IdBoolsPairArray;

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

constexpr auto kUnknownOrigin = "__UNKNOWN__"_ns;

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

// Calculates the number of `prioData` elements we'd need if we were asked for
// an encoded snapshot right now.
uint32_t PrioDataCount(const StaticMutexAutoLock& lock) {
  uint32_t count = 0;
  auto iter = gMetricToOriginBag->ConstIter();
  for (; !iter.Done(); iter.Next()) {
    auto originIt = iter.Data().ConstIter();
    uint32_t maxOriginCount = 0;
    for (; !originIt.Done(); originIt.Next()) {
      maxOriginCount = std::max(maxOriginCount, originIt.Data());
    }
    count += gPrioDatasPerMetric * maxOriginCount;
  }
  return count;
}

// Takes the storage and turns it into bool arrays for Prio to encode, turning
// { metric1: [origin1, origin2, ...], ...}
// into
// [(metric1, [[shard1], [shard2], ...]), ...]
// Note: if an origin is present multiple times for a given metric, we must
// generate multiple (id, boolvectors) pairs so that they are all reported.
// Meaning
// { metric1: [origin1, origin2, origin2] }
// must turn into (with a pretend gNumBooleans of 1)
// [(metric1, [[1], [1]]), (metric1, [[0], [1]])]
nsresult AppEncodeTo(const StaticMutexAutoLock& lock,
                     IdBoolsPairArray& aResult) {
  auto iter = gMetricToOriginBag->ConstIter();
  for (; !iter.Done(); iter.Next()) {
    OriginMetricID id = iter.Key();
    const OriginBag& bag = iter.Data();

    uint32_t generation = 1;
    uint32_t maxGeneration = 1;
    do {
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
      lastArray.SetLength((gOriginHashesList->Length() + kNumMetaOrigins) %
                          PrioEncoder::gNumBooleans);
      for (auto& metricDatum : lastArray) {
        metricDatum = false;
      }

      auto originIt = bag.ConstIter();
      for (; !originIt.Done(); originIt.Next()) {
        uint32_t originCount = originIt.Data();
        if (originCount >= generation) {
          maxGeneration = std::max(maxGeneration, originCount);

          const nsACString& origin = originIt.Key();
          size_t index;
          if (!gOriginToIndexMap->Get(origin, &index)) {
            return NS_ERROR_FAILURE;
          }
          MOZ_ASSERT(index < (gOriginHashesList->Length() + kNumMetaOrigins));
          size_t shardIndex = index / PrioEncoder::gNumBooleans;
          MOZ_ASSERT(shardIndex < metricData.Length());
          MOZ_ASSERT(index % PrioEncoder::gNumBooleans <
                     metricData[shardIndex].Length());
          metricData[shardIndex][index % PrioEncoder::gNumBooleans] = true;
        }
      }
      aResult.EmplaceBack(id, std::move(metricData));
    } while (generation++ < maxGeneration);
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

  // The contents and order of the arrays that follow matter.
  // Both ensure a consistent app-encoding.
  static const char sOriginStrings[] = {
#define ORIGIN(origin, hash) origin "\0"
#include "TelemetryOriginData.inc"
#undef ORIGIN
  };
  static const char sHashStrings[] = {
#define ORIGIN(origin, hash) hash "\0"
#include "TelemetryOriginData.inc"
#undef ORIGIN
  };

  struct OriginHashLengths {
    uint8_t originLength;
    uint8_t hashLength;
  };
  static const OriginHashLengths sOriginHashLengths[] = {
#define ORIGIN(origin, hash) {sizeof(origin), sizeof(hash)},
#include "TelemetryOriginData.inc"
#undef ORIGIN
  };

  static const size_t kNumOrigins = MOZ_ARRAY_LENGTH(sOriginHashLengths);

  gOriginHashesList = MakeUnique<OriginHashesList>(kNumOrigins);

  gPrioDatasPerMetric =
      ceil(static_cast<double>(kNumOrigins + kNumMetaOrigins) /
           PrioEncoder::gNumBooleans);

  gOriginToIndexMap =
      MakeUnique<OriginToIndexMap>(kNumOrigins + kNumMetaOrigins);
  gHashToIndexMap = MakeUnique<HashToIndexMap>(kNumOrigins);
  size_t originOffset = 0;
  size_t hashOffset = 0;
  for (size_t i = 0; i < kNumOrigins; ++i) {
    const char* origin = &sOriginStrings[originOffset];
    const char* hash = &sHashStrings[hashOffset];
    MOZ_ASSERT(!kUnknownOrigin.Equals(origin),
               "Unknown origin literal is reserved in Origin Telemetry");

    gOriginHashesList->AppendElement(MakeTuple(origin, hash));

    const size_t originLength = sOriginHashLengths[i].originLength;
    const size_t hashLength = sOriginHashLengths[i].hashLength;

    originOffset += originLength;
    hashOffset += hashLength;

    // -1 to leave off the null terminators.
    gOriginToIndexMap->InsertOrUpdate(
        nsDependentCString(origin, originLength - 1), i);
    gHashToIndexMap->InsertOrUpdate(nsDependentCString(hash, hashLength - 1),
                                    i);
  }

  // Add the meta-origin for tracking recordings to untracked origins.
  gOriginToIndexMap->InsertOrUpdate(kUnknownOrigin,
                                    gOriginHashesList->Length());

  gMetricToOriginBag = MakeUnique<IdToOriginBag>();

  // This map shouldn't change at runtime, so make debug builds complain
  // if it tries.
  gOriginToIndexMap->MarkImmutable();
  gHashToIndexMap->MarkImmutable();

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

  gOriginHashesList = nullptr;

  gOriginToIndexMap = nullptr;

  gHashToIndexMap = nullptr;

  gMetricToOriginBag = nullptr;

  gInitDone = false;
}

nsresult TelemetryOrigin::RecordOrigin(OriginMetricID aId,
                                       const nsACString& aOrigin) {
  if (!XRE_IsParentProcess()) {
    return NS_ERROR_FAILURE;
  }

  uint32_t prioDataCount;
  {
    StaticMutexAutoLock locker(gTelemetryOriginMutex);

    // Common Telemetry error-handling practices for recording functions:
    // only illegal calls return errors whereas merely incorrect ones are mutely
    // ignored.
    if (!gInitDone) {
      return NS_OK;
    }

    size_t index;
    nsCString origin(aOrigin);
    if (gHashToIndexMap->Get(aOrigin, &index)) {
      MOZ_ASSERT(aOrigin.Equals(Get<1>((*gOriginHashesList)[index])));
      origin = Get<0>((*gOriginHashesList)[index]);
    }

    if (!gOriginToIndexMap->Contains(origin)) {
      // Only record one unknown origin per metric per snapshot.
      // (otherwise we may get swamped and blow our data budget.)
      if (gMetricToOriginBag->Contains(aId) &&
          gMetricToOriginBag->LookupOrInsert(aId).Contains(kUnknownOrigin)) {
        return NS_OK;
      }
      origin = kUnknownOrigin;
    }

    auto& originBag = gMetricToOriginBag->LookupOrInsert(aId);
    originBag.LookupOrInsert(origin)++;

    prioDataCount = PrioDataCount(locker);
  }

  static uint32_t sPrioPingLimit =
      mozilla::Preferences::GetUint("toolkit.telemetry.prioping.dataLimit", 10);
  if (prioDataCount >= sPrioPingLimit) {
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os) {
      // Ensure we don't notify while holding the lock in case of synchronous
      // dispatch. May deadlock ourselves if we then trigger a snapshot.
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
  IdToOriginBag copy;
  {
    StaticMutexAutoLock locker(gTelemetryOriginMutex);

    if (aClear) {
      // I'd really prefer to clear after we're sure the snapshot didn't go
      // awry, but we can't hold a lock preventing recording while using JS
      // APIs. And replaying any interleaving recording sounds like too much
      // squeeze for not enough juice.

      gMetricToOriginBag->SwapElements(copy);
    } else {
      auto iter = gMetricToOriginBag->ConstIter();
      for (; !iter.Done(); iter.Next()) {
        OriginBag& bag = copy.LookupOrInsert(iter.Key());
        auto originIt = iter.Data().ConstIter();
        for (; !originIt.Done(); originIt.Next()) {
          bag.InsertOrUpdate(originIt.Key(), originIt.Data());
        }
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
    JS::RootedObject originsObj(aCx, JS_NewPlainObject(aCx));
    if (NS_WARN_IF(!originsObj)) {
      return NS_ERROR_FAILURE;
    }
    if (!JS_DefineProperty(aCx, rootObj, GetNameForMetricID(iter.Key()),
                           originsObj, JSPROP_ENUMERATE)) {
      NS_WARNING("Failed to define property in origin snapshot.");
      return NS_ERROR_FAILURE;
    }

    auto originIt = iter.Data().ConstIter();
    for (; !originIt.Done(); originIt.Next()) {
      if (!JS_DefineProperty(aCx, originsObj,
                             nsPromiseFlatCString(originIt.Key()).get(),
                             originIt.Data(), JSPROP_ENUMERATE)) {
        NS_WARNING("Failed to define origin and count in snapshot.");
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

      gMetricToOriginBag->Clear();
    }
  }

  // Step 2: Don't need the lock to prio-encode and base64-encode
  nsTArray<std::pair<nsCString, std::pair<nsCString, nsCString>>> prioData;
  for (auto& metricData : appEncodedMetricData) {
    auto& boolVectors = metricData.second;
    for (uint32_t i = 0; i < boolVectors.Length(); ++i) {
      // "encoding" is of the form `metricName-X` where X is the shard index.
      nsCString encodingName =
          nsPrintfCString("%s-%u", GetNameForMetricID(metricData.first), i);
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

      prioData.EmplaceBack(std::move(encodingName),
                           std::pair(std::move(aBase64), std::move(bBase64)));
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
                                 JS::NewArrayObject(aCx, prioData.Length()));
  if (NS_WARN_IF(!prioDataArray)) {
    return NS_ERROR_FAILURE;
  }
  uint32_t i = 0;
  for (auto& prioDatum : prioData) {
    JS::RootedObject prioDatumObj(aCx, JS_NewPlainObject(aCx));
    if (NS_WARN_IF(!prioDatumObj)) {
      return NS_ERROR_FAILURE;
    }
    JSString* encoding = ToJSString(aCx, prioDatum.first);
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

    JS::RootedString aRootStr(aCx, ToJSString(aCx, prioDatum.second.first));
    if (NS_WARN_IF(!JS_DefineProperty(aCx, prioObj, "a", aRootStr,
                                      JSPROP_ENUMERATE))) {
      return NS_ERROR_FAILURE;
    }
    JS::RootedString bRootStr(aCx, ToJSString(aCx, prioDatum.second.second));
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

  gMetricToOriginBag->Clear();
}

size_t TelemetryOrigin::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) {
  StaticMutexAutoLock locker(gTelemetryOriginMutex);
  size_t n = 0;

  if (!gInitDone) {
    return 0;
  }

  n += gMetricToOriginBag->ShallowSizeOfIncludingThis(aMallocSizeOf);
  auto iter = gMetricToOriginBag->ConstIter();
  for (; !iter.Done(); iter.Next()) {
    // The string hashkey and count should both be contained by the hashtable.
    n += iter.Data().ShallowSizeOfIncludingThis(aMallocSizeOf);
  }

  // The string hashkey and ID should both be contained within the hashtable.
  n += gOriginToIndexMap->ShallowSizeOfIncludingThis(aMallocSizeOf);

  return n;
}

size_t TelemetryOrigin::SizeOfPrioDatasPerMetric() {
  return gPrioDatasPerMetric;
}
