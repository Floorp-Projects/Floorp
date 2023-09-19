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
