/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_extensions_ExtensionAPIBase_h
#define mozilla_extensions_ExtensionAPIBase_h

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/ErrorResult.h"

namespace mozilla {

namespace dom {
class Function;
}

namespace extensions {

class ExtensionAPIBase {
 public:
  virtual void CallWebExtMethodNotImplementedNoReturn(
      JSContext* aCx, const nsAString& aApiMethod,
      const dom::Sequence<JS::Value>& aArgs, ErrorResult& aRv);

  virtual void CallWebExtMethodNotImplementedAsync(
      JSContext* aCx, const nsAString& aApiMethod,
      const dom::Sequence<JS::Value>& aArgs,
      const dom::Optional<OwningNonNull<dom::Function>>& aCallback,
      JS::MutableHandle<JS::Value> aRetval, ErrorResult& aRv);

  virtual void CallWebExtMethodNotImplemented(
      JSContext* aCx, const nsAString& aApiMethod,
      const dom::Sequence<JS::Value>& aArgs,
      JS::MutableHandle<JS::Value> aRetval, ErrorResult& aRv);
};

}  // namespace extensions
}  // namespace mozilla

#endif  // mozilla_extensions_ExtensionAPIBase_h
