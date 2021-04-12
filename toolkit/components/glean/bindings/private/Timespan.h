/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_glean_GleanTimespan_h
#define mozilla_glean_GleanTimespan_h

#include "mozilla/Maybe.h"
#include "nsIGleanMetrics.h"
#include "mozilla/glean/fog_ffi_generated.h"

namespace mozilla::glean {

namespace impl {

class TimespanMetric {
 public:
  constexpr explicit TimespanMetric(uint32_t aId) : mId(aId) {}

  /**
   * Start tracking time for the provided metric.
   *
   * This records an error if it’s already tracking time (i.e. start was already
   * called with no corresponding [stop]): in that case the original
   * start time will be preserved.
   */
  void Start() const {
    auto optScalarId = ScalarIdForMetric(mId);
    if (optScalarId) {
      auto scalarId = optScalarId.extract();
      auto lock = GetTimesToStartsLock();
      (void)NS_WARN_IF(lock.ref()->Remove(scalarId));
      lock.ref()->InsertOrUpdate(scalarId, TimeStamp::Now());
    }
#ifndef MOZ_GLEAN_ANDROID
    fog_timespan_start(mId);
#endif
  }

  /**
   * Stop tracking time for the provided metric.
   *
   * Sets the metric to the elapsed time, but does not overwrite an already
   * existing value.
   * This will record an error if no [start] was called or there is an already
   * existing value.
   */
  void Stop() const {
    auto optScalarId = ScalarIdForMetric(mId);
    if (optScalarId) {
      auto scalarId = optScalarId.extract();
      auto lock = GetTimesToStartsLock();
      auto optStart = lock.ref()->Extract(scalarId);
      if (!NS_WARN_IF(!optStart)) {
        uint32_t delta = static_cast<uint32_t>(
            (TimeStamp::Now() - optStart.extract()).ToMilliseconds());
        Telemetry::ScalarSet(scalarId, delta);
      }
    }
#ifndef MOZ_GLEAN_ANDROID
    fog_timespan_stop(mId);
#endif
  }

  /**
   * Abort a previous Start.
   *
   * No error will be recorded if no Start was called.
   */
  void Cancel() const {
    auto optScalarId = ScalarIdForMetric(mId);
    if (optScalarId) {
      auto scalarId = optScalarId.extract();
      auto lock = GetTimesToStartsLock();
      lock.ref()->Remove(scalarId);
    }
#ifndef MOZ_GLEAN_ANDROID
    fog_timespan_cancel(mId);
#endif
  }

  /**
   * Explicitly sets the timespan value
   *
   * This API should only be used if you cannot make use of
   * `start`/`stop`/`cancel`.
   *
   * @param aDuration The duration of this timespan, in units matching the
   *        `time_unit` of this metric's definition.
   */
  void SetRaw(uint32_t aDuration) const {
    auto optScalarId = ScalarIdForMetric(mId);
    if (optScalarId) {
      auto scalarId = optScalarId.extract();
      Telemetry::ScalarSet(scalarId, aDuration);
    }
#ifndef MOZ_GLEAN_ANDROID
    fog_timespan_set_raw(mId, aDuration);
#endif
  }

  /**
   * **Test-only API**
   *
   * Gets the currently stored value as an integer.
   *
   * This function will attempt to await the last parent-process task (if any)
   * writing to the the metric's storage engine before returning a value.
   * This function will not wait for data from child processes.
   *
   * This doesn't clear the stored value.
   * Parent process only. Panics in child processes.
   *
   * @param aPingName The (optional) name of the ping to retrieve the metric
   *        for. Defaults to the first value in `send_in_pings`.
   *
   * @return value of the stored metric, or Nothing() if there is no value.
   */
  Maybe<int64_t> TestGetValue(const nsACString& aPingName = nsCString()) const {
#ifdef MOZ_GLEAN_ANDROID
    Unused << mId;
    return Nothing();
#else
    if (!fog_timespan_test_has_value(mId, &aPingName)) {
      return Nothing();
    }
    return Some(fog_timespan_test_get_value(mId, &aPingName));
#endif
  }

 private:
  const uint32_t mId;
};
}  // namespace impl

class GleanTimespan final : public nsIGleanTimespan {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIGLEANTIMESPAN

  explicit GleanTimespan(uint32_t aId) : mTimespan(aId){};

 private:
  virtual ~GleanTimespan() = default;

  const impl::TimespanMetric mTimespan;
};

}  // namespace mozilla::glean

#endif /* mozilla_glean_GleanTimespan_h */
