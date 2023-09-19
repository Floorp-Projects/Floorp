/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_glean_GleanText_h
#define mozilla_glean_GleanText_h

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/glean/bindings/GleanMetric.h"
#include "mozilla/Maybe.h"
#include "mozilla/Result.h"
#include "nsString.h"

namespace mozilla::glean {

namespace impl {

class TextMetric {
 public:
  constexpr explicit TextMetric(uint32_t aId) : mId(aId) {}

  void Set(const nsACString& aValue) const;

  Result<Maybe<nsCString>, nsCString> TestGetValue(
      const nsACString& aPingName = nsCString()) const;

 private:
  const uint32_t mId;
};

}  // namespace impl

class GleanText final : public GleanMetric {
 public:
  explicit GleanText(uint32_t aId, nsISupports* aParent)
      : GleanMetric(aParent), mText(aId) {}

  virtual JSObject* WrapObject(
      JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override final;

  void Set(const nsACString& aValue);

  void TestGetValue(const nsACString& aPingName, nsCString& aResult,
                    ErrorResult& aRv);

 private:
  virtual ~GleanText() = default;

  const impl::TextMetric mText;
};
}  // namespace mozilla::glean
#endif
