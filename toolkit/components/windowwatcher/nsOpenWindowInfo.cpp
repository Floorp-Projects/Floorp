/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsOpenWindowInfo.h"
#include "mozilla/OriginAttributes.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/dom/BrowserParent.h"

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

NS_IMETHODIMP nsOpenWindowInfo::GetIsTopLevelCreatedByWebContent(
    bool* aIsTopLevelCreatedByWebContent) {
  *aIsTopLevelCreatedByWebContent = mIsTopLevelCreatedByWebContent;
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

NS_IMETHODIMP nsOpenWindowInfo::Cancel() {
  if (mBrowsingContextReadyCallback) {
    mBrowsingContextReadyCallback->BrowsingContextReady(nullptr);
    mBrowsingContextReadyCallback = nullptr;
  }
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsBrowsingContextReadyCallback,
                  nsIBrowsingContextReadyCallback)

nsBrowsingContextReadyCallback::nsBrowsingContextReadyCallback(
    RefPtr<mozilla::dom::BrowsingContextCallbackReceivedPromise::Private>
        aPromise)
    : mPromise(std::move(aPromise)) {}

nsBrowsingContextReadyCallback::~nsBrowsingContextReadyCallback() {
  if (mPromise) {
    mPromise->Reject(NS_ERROR_FAILURE, __func__);
  }
  mPromise = nullptr;
}

NS_IMETHODIMP nsBrowsingContextReadyCallback::BrowsingContextReady(
    mozilla::dom::BrowsingContext* aBC) {
  MOZ_DIAGNOSTIC_ASSERT(mPromise,
                        "The 'browsing context ready' callback is null");
  if (!mPromise) {
    return NS_OK;
  }
  if (aBC) {
    mPromise->Resolve(aBC, __func__);
  } else {
    mPromise->Reject(NS_ERROR_FAILURE, __func__);
  }
  mPromise = nullptr;
  return NS_OK;
}
