/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSharePicker.h"

#include "nsString.h"
#include "nsThreadUtils.h"
#include "WindowsUIUtils.h"
#include "nsPIDOMWindow.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/Unused.h"

using mozilla::dom::Promise;

///////////////////////////////////////////////////////////////////////////////
// nsISharePicker

NS_IMPL_ISUPPORTS(nsSharePicker, nsISharePicker)

namespace {
inline NS_ConvertUTF8toUTF16 NS_ConvertUTF8toUTF16_MaybeVoid(
    const nsACString& aStr) {
  auto str = NS_ConvertUTF8toUTF16(aStr);
  str.SetIsVoid(aStr.IsVoid());
  return str;
}
inline nsIGlobalObject* GetGlobalObject() {
  return xpc::NativeGlobal(xpc::PrivilegedJunkScope());
}
}  // namespace

NS_IMETHODIMP
nsSharePicker::Init(mozIDOMWindowProxy* aOpenerWindow) {
  if (mInited) {
    return NS_ERROR_FAILURE;
  }
  mOpenerWindow = aOpenerWindow;
  mInited = true;
  return NS_OK;
}

NS_IMETHODIMP
nsSharePicker::GetOpenerWindow(mozIDOMWindowProxy** aOpenerWindow) {
  *aOpenerWindow = mOpenerWindow;
  return NS_OK;
}

NS_IMETHODIMP
nsSharePicker::Share(const nsACString& aTitle, const nsACString& aText,
                     nsIURI* aUrl, Promise** aPromise) {
  mozilla::ErrorResult result;
  RefPtr<Promise> promise = Promise::Create(GetGlobalObject(), result);
  if (NS_WARN_IF(result.Failed())) {
    return result.StealNSResult();
  }
  nsAutoCString urlString;
  if (aUrl) {
    nsresult rv = aUrl->GetSpec(urlString);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    mozilla::Unused << rv;
  } else {
    urlString.SetIsVoid(true);
  }

  auto mozPromise =
      WindowsUIUtils::Share(NS_ConvertUTF8toUTF16_MaybeVoid(aTitle),
                            NS_ConvertUTF8toUTF16_MaybeVoid(aText),
                            NS_ConvertUTF8toUTF16_MaybeVoid(urlString));
  mozPromise->Then(
      mozilla::GetCurrentSerialEventTarget(), __func__,
      [promise]() { promise->MaybeResolveWithUndefined(); },
      [promise]() { promise->MaybeReject(NS_ERROR_DOM_ABORT_ERR); });

  promise.forget(aPromise);

  return NS_OK;
}
