/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_glean_GleanCustomDistribution_h
#define mozilla_glean_GleanCustomDistribution_h

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/glean/bindings/GleanMetric.h"
#include "mozilla/glean/bindings/DistributionData.h"
#include "mozilla/Maybe.h"
#include "mozilla/Result.h"
#include "nsTArray.h"

namespace mozilla::dom {
struct GleanDistributionData;
}  // namespace mozilla::dom

namespace mozilla::glean {

namespace impl {

class CustomDistributionMetric {
 public:
  constexpr explicit CustomDistributionMetric(uint32_t aId) : mId(aId) {}

  /**
   * Accumulates the provided samples in the metric.
   *
   * @param aSamples The vector holding the samples to be recorded by the
   *                 metric.
   */
  void AccumulateSamples(const nsTArray<uint64_t>& aSamples) const;

  /**
   * Accumulates the provided samples in the metric.
   *
   * @param aSamples The vector holding the samples to be recorded by the
   *                 metric.
   *
   * Notes: Discards any negative value in `samples`
   * and reports an `InvalidValue` error for each of them.
   */
  void AccumulateSamplesSigned(const nsTArray<int64_t>& aSamples) const;

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

class GleanCustomDistribution final : public GleanMetric {
 public:
  explicit GleanCustomDistribution(uint64_t aId, nsISupports* aParent)
      : GleanMetric(aParent), mCustomDist(aId) {}

  virtual JSObject* WrapObject(
      JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override final;

  void AccumulateSamples(const dom::Sequence<int64_t>& aSamples);

  void TestGetValue(const nsACString& aPingName,
                    dom::Nullable<dom::GleanDistributionData>& aRetval,
                    ErrorResult& aRv);

 private:
  virtual ~GleanCustomDistribution() = default;

  const impl::CustomDistributionMetric mCustomDist;
};

}  // namespace mozilla::glean

#endif /* mozilla_glean_GleanCustomDistribution_h */
