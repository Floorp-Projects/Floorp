/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/glean/bindings/Timespan.h"

#include "Common.h"
#include "nsString.h"
#include "mozilla/Components.h"
#include "mozilla/ResultVariant.h"
#include "mozilla/glean/bindings/ScalarGIFFTMap.h"
#include "mozilla/glean/fog_ffi_generated.h"
#include "nsIClassInfoImpl.h"

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
  if (!fog_timespan_test_has_value(mId, &aPingName)) {
    return Maybe<uint64_t>();
  }
  return Some(fog_timespan_test_get_value(mId, &aPingName));
}

}  // namespace impl

NS_IMPL_CLASSINFO(GleanTimespan, nullptr, 0, {0})
NS_IMPL_ISUPPORTS_CI(GleanTimespan, nsIGleanTimespan)

NS_IMETHODIMP
GleanTimespan::Start() {
  mTimespan.Start();
  return NS_OK;
}

NS_IMETHODIMP
GleanTimespan::Stop() {
  mTimespan.Stop();
  return NS_OK;
}

NS_IMETHODIMP
GleanTimespan::Cancel() {
  mTimespan.Cancel();
  return NS_OK;
}

NS_IMETHODIMP
GleanTimespan::SetRaw(uint32_t aDuration) {
  mTimespan.SetRaw(aDuration);
  return NS_OK;
}

NS_IMETHODIMP
GleanTimespan::TestGetValue(const nsACString& aStorageName,
                            JS::MutableHandle<JS::Value> aResult) {
  auto result = mTimespan.TestGetValue(aStorageName);
  if (result.isErr()) {
    aResult.set(JS::UndefinedValue());
    LogToBrowserConsole(nsIScriptError::errorFlag,
                        NS_ConvertUTF8toUTF16(result.unwrapErr()));
    return NS_ERROR_LOSS_OF_SIGNIFICANT_DATA;
  }
  auto optresult = result.unwrap();
  if (optresult.isNothing()) {
    aResult.set(JS::UndefinedValue());
  } else {
    aResult.set(JS::DoubleValue(static_cast<double>(optresult.value())));
  }
  return NS_OK;
}

}  // namespace mozilla::glean
