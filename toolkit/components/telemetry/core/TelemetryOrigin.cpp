/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Telemetry.h"
#include "TelemetryOrigin.h"

#include "nsDataHashtable.h"
#include "TelemetryCommon.h"
#include "TelemetryOriginEnums.h"
#include "TelemetryOriginData.h"

#include "mozilla/Atomics.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/StaticMutex.h"

#include <type_traits>

using mozilla::ErrorResult;
using mozilla::MallocSizeOf;
using mozilla::StaticMutex;
using mozilla::StaticMutexAutoLock;
using mozilla::dom::Promise;
using mozilla::Telemetry::OriginMetricID;
using mozilla::Telemetry::Common::ToJSString;

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

nsresult TelemetryOrigin::GetEncodedOriginSnapshot(bool aClear, JSContext* aCx,
                                                   Promise** aResult) {
  if (!XRE_IsParentProcess()) {
    return NS_ERROR_FAILURE;
  }
  NS_ENSURE_ARG_POINTER(aResult);
  if (!gInitDone) {
    *aResult = nullptr;
    return NS_OK;
  }

  nsIGlobalObject* global = xpc::CurrentNativeGlobal(aCx);
  if (NS_WARN_IF(!global)) {
    return NS_ERROR_FAILURE;
  }

  ErrorResult rv;
  RefPtr<Promise> promise = Promise::Create(global, rv);
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }

  // TODO: implement
  // We'll be wanting a worker thread
  // It'll need to take gMetricToOriginsMap and perform app-encoding
  // Then pass it to PrioEncoder for prio-encoding
  // Then base64 encoding
  // And don't forget to clear if aClear

  // For now, just reject the Promise.
  promise->MaybeReject(NS_ERROR_FAILURE);

  promise.forget(aResult);
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
