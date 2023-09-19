/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_glean_GleanRate_h
#define mozilla_glean_GleanRate_h

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/glean/bindings/GleanMetric.h"
#include "mozilla/Maybe.h"
#include "mozilla/Result.h"
#include "nsString.h"

namespace mozilla::dom {
struct GleanRateData;
}  // namespace mozilla::dom

namespace mozilla::glean {

namespace impl {

class RateMetric {
 public:
  constexpr explicit RateMetric(uint32_t aId) : mId(aId) {}

  /*
   * Increases the numerator by `amount`.
   *
   * @param aAmount The amount to increase by. Should be positive.
   */
  void AddToNumerator(int32_t aAmount = 1) const;

  /*
   * Increases the denominator by `amount`.
   *
   * @param aAmount The amount to increase by. Should be positive.
   */
  void AddToDenominator(int32_t aAmount = 1) const;

  /**
   * **Test-only API**
   *
   * Gets the currently stored value as a pair of integers.
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
  Result<Maybe<std::pair<int32_t, int32_t>>, nsCString> TestGetValue(
      const nsACString& aPingName = nsCString()) const;

 private:
  const uint32_t mId;
};
}  // namespace impl

class GleanRate final : public GleanMetric {
 public:
  explicit GleanRate(uint32_t id, nsISupports* aParent)
      : GleanMetric(aParent), mRate(id) {}

  virtual JSObject* WrapObject(
      JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override final;

  void AddToNumerator(int32_t aAmount);
  void AddToDenominator(int32_t aAmount);

  void TestGetValue(const nsACString& aPingName,
                    dom::Nullable<dom::GleanRateData>& aResult,
                    ErrorResult& aRv);

 private:
  virtual ~GleanRate() = default;

  const impl::RateMetric mRate;
};

}  // namespace mozilla::glean

#endif /* mozilla_glean_GleanRate_h */
