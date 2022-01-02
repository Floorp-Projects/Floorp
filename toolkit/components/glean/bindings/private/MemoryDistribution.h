/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_glean_GleanMemoryDistribution_h
#define mozilla_glean_GleanMemoryDistribution_h

#include "mozilla/glean/bindings/DistributionData.h"
#include "mozilla/Maybe.h"
#include "nsIGleanMetrics.h"
#include "nsTArray.h"

namespace mozilla::glean {

namespace impl {

class MemoryDistributionMetric {
 public:
  constexpr explicit MemoryDistributionMetric(uint32_t aId) : mId(aId) {}

  /*
   * Accumulates the provided sample in the metric.
   *
   * @param aSample The sample to be recorded by the metric. The sample is
   *                assumed to be in the confgured memory unit of the metric.
   *
   * Notes: Values bigger than 1 Terabyte (2^40 bytes) are truncated and an
   * InvalidValue error is recorded.
   */
  void Accumulate(uint64_t aSample) const;

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

class GleanMemoryDistribution final : public nsIGleanMemoryDistribution {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIGLEANMEMORYDISTRIBUTION

  explicit GleanMemoryDistribution(uint64_t aId) : mMemoryDist(aId){};

 private:
  virtual ~GleanMemoryDistribution() = default;

  const impl::MemoryDistributionMetric mMemoryDist;
};

}  // namespace mozilla::glean

#endif /* mozilla_glean_GleanMemoryDistribution_h */
