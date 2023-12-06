/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/glean/bindings/Timespan.h"

#include "nsString.h"
#include "mozilla/Components.h"
#include "mozilla/ResultVariant.h"
#include "mozilla/dom/GleanMetricsBinding.h"
#include "mozilla/glean/bindings/ScalarGIFFTMap.h"
#include "mozilla/glean/fog_ffi_generated.h"

namespace mozilla::glean {

namespace impl {

namespace {
class ScalarIDHashKey : public PLDHashEntryHdr {
 public:
  using KeyType = const ScalarID&;
  using KeyTypePointer = const ScalarID*;

  explicit ScalarIDHashKey(KeyTypePointer aKey) : mValue(*aKey) {}
  ScalarIDHashKey(ScalarIDHashKey&& aOther)
      : PLDHashEntryHdr(std::move(aOther)), mValue(std::move(aOther.mValue)) {}
  ~ScalarIDHashKey() = default;

  KeyType GetKey() const { return mValue; }
  bool KeyEquals(KeyTypePointer aKey) const { return *aKey == mValue; }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }
  static PLDHashNumber HashKey(KeyTypePointer aKey) {
    return static_cast<std::underlying_type<ScalarID>::type>(*aKey);
  }
  enum { ALLOW_MEMMOVE = true };

 private:
  const ScalarID mValue;
};
}  // namespace

using TimesToStartsMutex =
    StaticDataMutex<UniquePtr<nsTHashMap<ScalarIDHashKey, TimeStamp>>>;
static Maybe<TimesToStartsMutex::AutoLock> GetTimesToStartsLock() {
  static TimesToStartsMutex sTimespanStarts("sTimespanStarts");
  auto lock = sTimespanStarts.Lock();
  // GIFFT will work up to the end of AppShutdownTelemetry.
  if (AppShutdown::IsInOrBeyond(ShutdownPhase::XPCOMWillShutdown)) {
    return Nothing();
  }
  if (!*lock) {
    *lock = MakeUnique<nsTHashMap<ScalarIDHashKey, TimeStamp>>();
    RefPtr<nsIRunnable> cleanupFn = NS_NewRunnableFunction(__func__, [&] {
      if (AppShutdown::IsInOrBeyond(ShutdownPhase::XPCOMWillShutdown)) {
        auto lock = sTimespanStarts.Lock();
        *lock = nullptr;  // deletes, see UniquePtr.h
        return;
      }
      RunOnShutdown(
          [&] {
            auto lock = sTimespanStarts.Lock();
            *lock = nullptr;  // deletes, see UniquePtr.h
          },
          ShutdownPhase::XPCOMWillShutdown);
    });
    // Both getting the main thread and dispatching to it can fail.
    // In that event we leak. Grab a pointer so we have something to NS_RELEASE
    // in that case.
    nsIRunnable* temp = cleanupFn.get();
    nsCOMPtr<nsIThread> mainThread;
    if (NS_FAILED(NS_GetMainThread(getter_AddRefs(mainThread))) ||
        NS_FAILED(mainThread->Dispatch(cleanupFn.forget(),
                                       nsIThread::DISPATCH_NORMAL))) {
      // Failed to dispatch cleanup routine.
      // First, un-leak the runnable (but only if we actually attempted
      // dispatch)
      if (!cleanupFn) {
        NS_RELEASE(temp);
      }
      // Next, cleanup immediately, and allow metrics to try again later.
      *lock = nullptr;
      return Nothing();
    }
  }
  return Some(std::move(lock));
}

void TimespanMetric::Start() const {
  auto optScalarId = ScalarIdForMetric(mId);
  if (optScalarId) {
    auto scalarId = optScalarId.extract();
    GetTimesToStartsLock().apply([&](auto& lock) {
      (void)NS_WARN_IF(lock.ref()->Remove(scalarId));
      lock.ref()->InsertOrUpdate(scalarId, TimeStamp::Now());
    });
  }
  fog_timespan_start(mId);
}

void TimespanMetric::Stop() const {
  auto optScalarId = ScalarIdForMetric(mId);
  if (optScalarId) {
    auto scalarId = optScalarId.extract();
    GetTimesToStartsLock().apply([&](auto& lock) {
      auto optStart = lock.ref()->Extract(scalarId);
      if (!NS_WARN_IF(!optStart)) {
        double delta = (TimeStamp::Now() - optStart.extract()).ToMilliseconds();
        uint32_t theDelta = static_cast<uint32_t>(delta);
        if (delta > std::numeric_limits<uint32_t>::max()) {
          theDelta = std::numeric_limits<uint32_t>::max();
        } else if (MOZ_UNLIKELY(delta < 0)) {
          theDelta = 0;
        }
        Telemetry::ScalarSet(scalarId, theDelta);
      }
    });
  }
  fog_timespan_stop(mId);
}

void TimespanMetric::Cancel() const {
  auto optScalarId = ScalarIdForMetric(mId);
  if (optScalarId) {
    auto scalarId = optScalarId.extract();
    GetTimesToStartsLock().apply(
        [&](auto& lock) { lock.ref()->Remove(scalarId); });
  }
  fog_timespan_cancel(mId);
}

void TimespanMetric::SetRaw(uint32_t aDuration) const {
  auto optScalarId = ScalarIdForMetric(mId);
  if (optScalarId) {
    auto scalarId = optScalarId.extract();
    Telemetry::ScalarSet(scalarId, aDuration);
  }
  fog_timespan_set_raw(mId, aDuration);
}

Result<Maybe<uint64_t>, nsCString> TimespanMetric::TestGetValue(
    const nsACString& aPingName) const {
  nsCString err;
  if (fog_timespan_test_get_error(mId, &err)) {
    return Err(err);
  }
  if (!fog_timespan_test_has_value(mId, &aPingName)) {
    return Maybe<uint64_t>();
  }
  return Some(fog_timespan_test_get_value(mId, &aPingName));
}

}  // namespace impl

/* virtual */
JSObject* GleanTimespan::WrapObject(JSContext* aCx,
                                    JS::Handle<JSObject*> aGivenProto) {
  return dom::GleanTimespan_Binding::Wrap(aCx, this, aGivenProto);
}

void GleanTimespan::Start() { mTimespan.Start(); }

void GleanTimespan::Stop() { mTimespan.Stop(); }

void GleanTimespan::Cancel() { mTimespan.Cancel(); }

void GleanTimespan::SetRaw(uint32_t aDuration) { mTimespan.SetRaw(aDuration); }

dom::Nullable<uint64_t> GleanTimespan::TestGetValue(const nsACString& aPingName,
                                                    ErrorResult& aRv) {
  dom::Nullable<uint64_t> ret;
  auto result = mTimespan.TestGetValue(aPingName);
  if (result.isErr()) {
    aRv.ThrowDataError(result.unwrapErr());
    return ret;
  }
  auto optresult = result.unwrap();
  if (!optresult.isNothing()) {
    ret.SetValue(optresult.value());
  }
  return ret;
}

}  // namespace mozilla::glean
