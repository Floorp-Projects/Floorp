/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsOpenWindowInfo.h"
#include "mozilla/OriginAttributes.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/dom/BrowserParent.h"
#include "nsThreadUtils.h"

NS_IMPL_ISUPPORTS(nsOpenWindowInfo, nsIOpenWindowInfo)

NS_IMETHODIMP nsOpenWindowInfo::GetParent(
    mozilla::dom::BrowsingContext** aParent) {
  *aParent = do_AddRef(mParent).take();
  return NS_OK;
}

NS_IMETHODIMP nsOpenWindowInfo::GetIsRemote(bool* aIsRemote) {
  *aIsRemote = mIsRemote;
  return NS_OK;
}

NS_IMETHODIMP nsOpenWindowInfo::GetIsForWindowDotPrint(bool* aResult) {
  *aResult = mIsForWindowDotPrint;
  return NS_OK;
}

NS_IMETHODIMP nsOpenWindowInfo::GetIsForPrinting(bool* aIsForPrinting) {
  *aIsForPrinting = mIsForPrinting;
  return NS_OK;
}

NS_IMETHODIMP nsOpenWindowInfo::GetForceNoOpener(bool* aForceNoOpener) {
  *aForceNoOpener = mForceNoOpener;
  return NS_OK;
}

NS_IMETHODIMP nsOpenWindowInfo::GetScriptableOriginAttributes(
    JSContext* aCx, JS::MutableHandle<JS::Value> aAttrs) {
  bool ok = ToJSValue(aCx, mOriginAttributes, aAttrs);
  NS_ENSURE_TRUE(ok, NS_ERROR_FAILURE);
  return NS_OK;
}

const mozilla::OriginAttributes& nsOpenWindowInfo::GetOriginAttributes() {
  return mOriginAttributes;
}

mozilla::dom::BrowserParent* nsOpenWindowInfo::GetNextRemoteBrowser() {
  return mNextRemoteBrowser;
}

nsIBrowsingContextReadyCallback*
nsOpenWindowInfo::BrowsingContextReadyCallback() {
  return mBrowsingContextReadyCallback;
}

namespace {

class nsBrowsingContextReadyCallback final
    : public nsIBrowsingContextReadyCallback {
 public:
  NS_DECL_ISUPPORTS

  explicit nsBrowsingContextReadyCallback(
      std::function<void(mozilla::dom::BrowsingContext*)>&& aCallback)
      : mCallback(aCallback) {}

  NS_IMETHOD BrowsingContextReady(mozilla::dom::BrowsingContext* aBC) override {
    if (!mCallback) {
      return NS_OK;
    }
    // Spin the event loop to make sure everything's been initialized before we
    // invoke the callback.
    NS_DispatchToMainThread(NS_NewRunnableFunction(
        "BrowsingContextReadyCallback",
        [callback = std::move(mCallback), bc = RefPtr{aBC}] { callback(bc); }));
    mCallback = nullptr;
    return NS_OK;
  }

 private:
  ~nsBrowsingContextReadyCallback() { BrowsingContextReady(nullptr); }

  std::function<void(mozilla::dom::BrowsingContext*)> mCallback;
};

NS_IMPL_ISUPPORTS(nsBrowsingContextReadyCallback,
                  nsIBrowsingContextReadyCallback)

}  // namespace

nsresult nsOpenWindowInfo::OnBrowsingContextReady(
    std::function<void(mozilla::dom::BrowsingContext*)>&& aCallback) {
  if (mBrowsingContextReadyCallback) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }
  mBrowsingContextReadyCallback =
      mozilla::MakeAndAddRef<nsBrowsingContextReadyCallback>(
          std::move(aCallback));
  return NS_OK;
}
