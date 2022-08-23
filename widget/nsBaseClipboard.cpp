/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsBaseClipboard.h"

#include "mozilla/Logging.h"

#include "nsIClipboardOwner.h"
#include "nsCOMPtr.h"
#include "nsError.h"
#include "nsXPCOM.h"

using mozilla::GenericPromise;
using mozilla::LogLevel;

nsBaseClipboard::nsBaseClipboard() : mEmptyingForSetData(false) {}

nsBaseClipboard::~nsBaseClipboard() {
  EmptyClipboard(kSelectionClipboard);
  EmptyClipboard(kGlobalClipboard);
  EmptyClipboard(kFindClipboard);
}

NS_IMPL_ISUPPORTS(nsBaseClipboard, nsIClipboard)

/**
 * Sets the transferable object
 *
 */
NS_IMETHODIMP nsBaseClipboard::SetData(nsITransferable* aTransferable,
                                       nsIClipboardOwner* anOwner,
                                       int32_t aWhichClipboard) {
  NS_ASSERTION(aTransferable, "clipboard given a null transferable");

  CLIPBOARD_LOG("%s", __FUNCTION__);

  if (aTransferable == mTransferable && anOwner == mClipboardOwner) {
    CLIPBOARD_LOG("%s: skipping update.", __FUNCTION__);
    return NS_OK;
  }
  bool selectClipPresent;
  SupportsSelectionClipboard(&selectClipPresent);
  bool findClipPresent;
  SupportsFindClipboard(&findClipPresent);
  if (!selectClipPresent && !findClipPresent &&
      aWhichClipboard != kGlobalClipboard)
    return NS_ERROR_FAILURE;

  mEmptyingForSetData = true;
  if (NS_FAILED(EmptyClipboard(aWhichClipboard))) {
    CLIPBOARD_LOG("%s: emptying clipboard failed.", __FUNCTION__);
  }
  mEmptyingForSetData = false;

  mClipboardOwner = anOwner;
  mTransferable = aTransferable;

  nsresult rv = NS_ERROR_FAILURE;
  if (mTransferable) {
    mIgnoreEmptyNotification = true;
    rv = SetNativeClipboardData(aWhichClipboard);
    mIgnoreEmptyNotification = false;
  }
  if (NS_FAILED(rv)) {
    CLIPBOARD_LOG("%s: setting native clipboard data failed.", __FUNCTION__);
  }

  return rv;
}

/**
 * Gets the transferable object
 *
 */
NS_IMETHODIMP nsBaseClipboard::GetData(nsITransferable* aTransferable,
                                       int32_t aWhichClipboard) {
  NS_ASSERTION(aTransferable, "clipboard given a null transferable");

  CLIPBOARD_LOG("%s", __FUNCTION__);

  bool selectClipPresent;
  SupportsSelectionClipboard(&selectClipPresent);
  bool findClipPresent;
  SupportsFindClipboard(&findClipPresent);
  if (!selectClipPresent && !findClipPresent &&
      aWhichClipboard != kGlobalClipboard)
    return NS_ERROR_FAILURE;

  if (aTransferable)
    return GetNativeClipboardData(aTransferable, aWhichClipboard);

  return NS_ERROR_FAILURE;
}

RefPtr<GenericPromise> nsBaseClipboard::AsyncGetData(
    nsITransferable* aTransferable, int32_t aWhichClipboard) {
  nsresult rv = GetData(aTransferable, aWhichClipboard);
  if (NS_FAILED(rv)) {
    return GenericPromise::CreateAndReject(rv, __func__);
  }

  return GenericPromise::CreateAndResolve(true, __func__);
}

NS_IMETHODIMP nsBaseClipboard::EmptyClipboard(int32_t aWhichClipboard) {
  CLIPBOARD_LOG("%s: clipboard=%i", __FUNCTION__, aWhichClipboard);

  bool selectClipPresent;
  SupportsSelectionClipboard(&selectClipPresent);
  bool findClipPresent;
  SupportsFindClipboard(&findClipPresent);
  if (!selectClipPresent && !findClipPresent &&
      aWhichClipboard != kGlobalClipboard)
    return NS_ERROR_FAILURE;

  if (mIgnoreEmptyNotification) {
    return NS_OK;
  }

  if (mClipboardOwner) {
    mClipboardOwner->LosingOwnership(mTransferable);
    mClipboardOwner = nullptr;
  }

  mTransferable = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseClipboard::HasDataMatchingFlavors(const nsTArray<nsCString>& aFlavorList,
                                        int32_t aWhichClipboard,
                                        bool* outResult) {
  *outResult = true;  // say we always do.
  return NS_OK;
}

RefPtr<DataFlavorsPromise> nsBaseClipboard::AsyncHasDataMatchingFlavors(
    const nsTArray<nsCString>& aFlavorList, int32_t aWhichClipboard) {
  nsTArray<nsCString> results;
  for (const auto& flavor : aFlavorList) {
    bool hasMatchingFlavor = false;
    nsresult rv = HasDataMatchingFlavors(AutoTArray<nsCString, 1>{flavor},
                                         aWhichClipboard, &hasMatchingFlavor);
    if (NS_SUCCEEDED(rv) && hasMatchingFlavor) {
      results.AppendElement(flavor);
    }
  }

  return DataFlavorsPromise::CreateAndResolve(std::move(results), __func__);
}

NS_IMETHODIMP
nsBaseClipboard::SupportsSelectionClipboard(bool* _retval) {
  *_retval = false;  // we don't support the selection clipboard by default.
  return NS_OK;
}

NS_IMETHODIMP
nsBaseClipboard::SupportsFindClipboard(bool* _retval) {
  *_retval = false;  // we don't support the find clipboard by default.
  return NS_OK;
}
