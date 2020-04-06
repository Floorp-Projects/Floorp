/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsOpenWindowInfo.h"
#include "mozilla/OriginAttributes.h"
#include "mozilla/dom/ToJSValue.h"

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

const OriginAttributes& nsOpenWindowInfo::GetOriginAttributes() {
  return mOriginAttributes;
}

NS_IMETHODIMP nsOpenWindowInfo::GetNextRemoteBrowserId(
    uint64_t* aNextRemoteBrowserId) {
  *aNextRemoteBrowserId = mNextRemoteBrowserId;
  return NS_OK;
}
