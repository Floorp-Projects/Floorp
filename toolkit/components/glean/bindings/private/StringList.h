/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_glean_GleanStringList_h
#define mozilla_glean_GleanStringList_h

#include "mozilla/Maybe.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/glean/bindings/GleanMetric.h"
#include "nsString.h"
#include "nsTArray.h"

namespace mozilla::glean {

namespace impl {

class StringListMetric {
 public:
  constexpr explicit StringListMetric(uint32_t aId) : mId(aId) {}

  /*
   * Adds a new string to the list.
   *
   * Truncates the value if it is longer than 50 bytes and logs an error.
   *
   * @param aValue The string to add.
   */
  void Add(const nsACString& aValue) const;

  /*
   * Set to a specific list of strings.
   *
   * Truncates any values longer than 50 bytes and logs an error.
   * Truncates the list if it is over 20 items long.
   * See
   * https://mozilla.github.io/glean/book/user/metrics/string_list.html#limits.
   *
   * @param aValue The list of strings to set the metric to.
   */
  void Set(const nsTArray<nsCString>& aValue) const;

  /**
   * **Test-only API**
   *
   * Gets the currently stored value.
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
  Result<Maybe<nsTArray<nsCString>>, nsCString> TestGetValue(
      const nsACString& aPingName = nsCString()) const;

 private:
  const uint32_t mId;
};
}  // namespace impl

class GleanStringList final : public GleanMetric {
 public:
  explicit GleanStringList(uint32_t aId, nsISupports* aParent)
      : GleanMetric(aParent), mStringList(aId) {}

  virtual JSObject* WrapObject(
      JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override final;

  void Add(const nsACString& aValue);
  void Set(const dom::Sequence<nsCString>& aValue);

  void TestGetValue(const nsACString& aPingName,
                    dom::Nullable<nsTArray<nsCString>>& aResult,
                    ErrorResult& aRv);

 private:
  virtual ~GleanStringList() = default;

  const impl::StringListMetric mStringList;
};

}  // namespace mozilla::glean

#endif /* mozilla_glean_GleanStringList_h */
