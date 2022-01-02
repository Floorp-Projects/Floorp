/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_glean_GleanTimingDistribution_h
#define mozilla_glean_GleanTimingDistribution_h

#include "mozilla/glean/bindings/DistributionData.h"
#include "mozilla/Maybe.h"
#include "mozilla/Result.h"
#include "nsIGleanMetrics.h"
#include "nsTArray.h"

namespace mozilla::glean {

typedef uint64_t TimerId;

namespace impl {
class TimingDistributionMetric {
 public:
  constexpr explicit TimingDistributionMetric(uint32_t aId) : mId(aId) {}

  /*
   * Starts tracking time for the provided metric.
   *
   * @returns A unique TimerId for the new timer
   */
  TimerId Start() const;

  /*
   * Stops tracking time for the provided metric and associated timer id.
   *
   * Adds a count to the corresponding bucket in the timing distribution.
   * This will record an error if no `Start` was called on this TimerId or
   * if this TimerId was used to call `Cancel`.
   *
   * @param aId The TimerId to associate with this timing. This allows for
   *            concurrent timing of events associated with different ids.
   */
  void StopAndAccumulate(const TimerId&& aId) const;

  /*
   * Aborts a previous `Start` call. No error is recorded if no `Start` was
   * called.
   *
   * @param aId The TimerId whose `Start` you wish to abort.
   */
  void Cancel(const TimerId&& aId) const;

  /**
   * **Test-only API**
   *
   * Gets the currently stored value as a DistributionData.
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
  Result<Maybe<DistributionData>, nsCString> TestGetValue(
      const nsACString& aPingName = nsCString()) const;

 private:
  const uint32_t mId;
};
}  // namespace impl

class GleanTimingDistribution final : public nsIGleanTimingDistribution {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIGLEANTIMINGDISTRIBUTION

  explicit GleanTimingDistribution(uint64_t aId) : mTimingDist(aId){};

 private:
  virtual ~GleanTimingDistribution() = default;

  const impl::TimingDistributionMetric mTimingDist;
};

}  // namespace mozilla::glean

#endif /* mozilla_glean_GleanTimingDistribution_h */
