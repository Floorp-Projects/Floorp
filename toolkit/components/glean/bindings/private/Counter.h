/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_glean_GleanCounter_h
#define mozilla_glean_GleanCounter_h

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/glean/bindings/GleanMetric.h"
#include "mozilla/Maybe.h"
#include "mozilla/Result.h"
#include "nsString.h"

namespace mozilla::glean {

namespace impl {

class CounterMetric {
 public:
  constexpr explicit CounterMetric(uint32_t aId) : mId(aId) {}

  /*
   * Increases the counter by `amount`.
   *
   * @param aAmount The amount to increase by. Should be positive.
   */
  void Add(int32_t aAmount = 1) const;

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
  Result<Maybe<int32_t>, nsCString> TestGetValue(
      const nsACString& aPingName = nsCString()) const;

 private:
  const uint32_t mId;
};
}  // namespace impl

class GleanCounter final : public GleanMetric {
 public:
  explicit GleanCounter(uint32_t id, nsISupports* aParent)
      : GleanMetric(aParent), mCounter(id) {}

  virtual JSObject* WrapObject(
      JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override final;

  void Add(int32_t aAmount);

  dom::Nullable<int32_t> TestGetValue(const nsACString& aPingName,
                                      ErrorResult& aRv);

 private:
  virtual ~GleanCounter() = default;

  const impl::CounterMetric mCounter;
};

}  // namespace mozilla::glean

#endif /* mozilla_glean_GleanCounter_h */
