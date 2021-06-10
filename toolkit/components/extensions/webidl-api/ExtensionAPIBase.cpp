/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ExtensionAPIBase.h"

namespace mozilla {
namespace extensions {

void ExtensionAPIBase::CallWebExtMethodNotImplementedNoReturn(
    JSContext* aCx, const nsAString& aApiMethod,
    const dom::Sequence<JS::Value>& aArgs, ErrorResult& aRv) {
  aRv.ThrowNotSupportedError("Not implemented");
}

void ExtensionAPIBase::CallWebExtMethodNotImplementedAsync(
    JSContext* aCx, const nsAString& aApiMethod,
    const dom::Sequence<JS::Value>& aArgs,
    const dom::Optional<OwningNonNull<dom::Function>>& aCallback,
    JS::MutableHandle<JS::Value> aRetval, ErrorResult& aRv) {
  CallWebExtMethodNotImplementedNoReturn(aCx, aApiMethod, aArgs, aRv);
}

void ExtensionAPIBase::CallWebExtMethodNotImplemented(
    JSContext* aCx, const nsAString& aApiMethod,
    const dom::Sequence<JS::Value>& aArgs, JS::MutableHandle<JS::Value> aRetval,
    ErrorResult& aRv) {
  CallWebExtMethodNotImplementedNoReturn(aCx, aApiMethod, aArgs, aRv);
}

}  // namespace extensions
}  // namespace mozilla
