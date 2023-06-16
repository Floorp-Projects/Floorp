/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsBaseClipboard.h"

#include "nsIClipboardOwner.h"
#include "nsError.h"
#include "nsXPCOM.h"

using mozilla::GenericPromise;
using mozilla::LogLevel;
using mozilla::UniquePtr;
using mozilla::dom::ClipboardCapabilities;

NS_IMPL_ISUPPORTS(ClipboardSetDataHelper::AsyncSetClipboardData,
                  nsIAsyncSetClipboardData)

ClipboardSetDataHelper::AsyncSetClipboardData::AsyncSetClipboardData(
    int32_t aClipboardType, ClipboardSetDataHelper* aClipboard,
    nsIAsyncSetClipboardDataCallback* aCallback)
    : mClipboardType(aClipboardType),
      mClipboard(aClipboard),
      mCallback(aCallback) {
  MOZ_ASSERT(mClipboard);
  MOZ_ASSERT(mClipboard->IsClipboardTypeSupported(mClipboardType));
}

NS_IMETHODIMP
ClipboardSetDataHelper::AsyncSetClipboardData::SetData(
    nsITransferable* aTransferable, nsIClipboardOwner* aOwner) {
  if (!IsValid()) {
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(mClipboard);
  MOZ_ASSERT(mClipboard->IsClipboardTypeSupported(mClipboardType));
  MOZ_DIAGNOSTIC_ASSERT(mClipboard->mPendingWriteRequests[mClipboardType] ==
                        this);

  RefPtr<AsyncSetClipboardData> request =
      std::move(mClipboard->mPendingWriteRequests[mClipboardType]);
  nsresult rv = mClipboard->SetData(aTransferable, aOwner, mClipboardType);
  MaybeNotifyCallback(rv);

  return rv;
}

NS_IMETHODIMP
ClipboardSetDataHelper::AsyncSetClipboardData::Abort(nsresult aReason) {
  // Note: This may be called during destructor, so it should not attempt to
  // take a reference to mClipboard.

  if (!IsValid() || !NS_FAILED(aReason)) {
    return NS_ERROR_FAILURE;
  }

  MaybeNotifyCallback(aReason);
  return NS_OK;
}

void ClipboardSetDataHelper::AsyncSetClipboardData::MaybeNotifyCallback(
    nsresult aResult) {
  // Note: This may be called during destructor, so it should not attempt to
  // take a reference to mClipboard.

  MOZ_ASSERT(IsValid());
  if (nsCOMPtr<nsIAsyncSetClipboardDataCallback> callback =
          mCallback.forget()) {
    callback->OnComplete(aResult);
  }
  // Once the callback is notified, setData should not be allowed, so invalidate
  // this request.
  mClipboard = nullptr;
}

NS_IMPL_ISUPPORTS(ClipboardSetDataHelper, nsIClipboard)

ClipboardSetDataHelper::~ClipboardSetDataHelper() {
  for (auto& request : mPendingWriteRequests) {
    if (request) {
      request->Abort(NS_ERROR_ABORT);
      request = nullptr;
    }
  }
}

void ClipboardSetDataHelper::RejectPendingAsyncSetDataRequestIfAny(
    int32_t aClipboardType) {
  MOZ_ASSERT(nsIClipboard::IsClipboardTypeSupported(aClipboardType));
  auto& request = mPendingWriteRequests[aClipboardType];
  if (request) {
    request->Abort(NS_ERROR_ABORT);
    request = nullptr;
  }
}

NS_IMETHODIMP
ClipboardSetDataHelper::SetData(nsITransferable* aTransferable,
                                nsIClipboardOwner* aOwner,
                                int32_t aWhichClipboard) {
  NS_ENSURE_ARG(aTransferable);
  if (!nsIClipboard::IsClipboardTypeSupported(aWhichClipboard)) {
    return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
  }

  // Reject existing pending asyncSetData request if any.
  RejectPendingAsyncSetDataRequestIfAny(aWhichClipboard);

  return SetNativeClipboardData(aTransferable, aOwner, aWhichClipboard);
}

NS_IMETHODIMP ClipboardSetDataHelper::AsyncSetData(
    int32_t aWhichClipboard, nsIAsyncSetClipboardDataCallback* aCallback,
    nsIAsyncSetClipboardData** _retval) {
  *_retval = nullptr;
  if (!nsIClipboard::IsClipboardTypeSupported(aWhichClipboard)) {
    return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
  }

  // Reject existing pending AsyncSetData request if any.
  RejectPendingAsyncSetDataRequestIfAny(aWhichClipboard);

  // Create a new AsyncSetClipboardData.
  RefPtr<AsyncSetClipboardData> request =
      mozilla::MakeRefPtr<AsyncSetClipboardData>(aWhichClipboard, this,
                                                 aCallback);
  mPendingWriteRequests[aWhichClipboard] = request;
  request.forget(_retval);
  return NS_OK;
}

nsBaseClipboard::nsBaseClipboard(const ClipboardCapabilities& aClipboardCaps)
    : mClipboardCaps(aClipboardCaps) {
  using mozilla::MakeUnique;
  // Initialize clipboard cache.
  mCaches[kGlobalClipboard] = MakeUnique<ClipboardCache>();
  if (mClipboardCaps.supportsSelectionClipboard()) {
    mCaches[kSelectionClipboard] = MakeUnique<ClipboardCache>();
  }
  if (mClipboardCaps.supportsFindClipboard()) {
    mCaches[kFindClipboard] = MakeUnique<ClipboardCache>();
  }
  if (mClipboardCaps.supportsSelectionCache()) {
    mCaches[kSelectionCache] = MakeUnique<ClipboardCache>();
  }
}

NS_IMPL_ISUPPORTS_INHERITED0(nsBaseClipboard, ClipboardSetDataHelper)

/**
 * Sets the transferable object
 *
 */
NS_IMETHODIMP nsBaseClipboard::SetData(nsITransferable* aTransferable,
                                       nsIClipboardOwner* anOwner,
                                       int32_t aWhichClipboard) {
  NS_ASSERTION(aTransferable, "clipboard given a null transferable");

  CLIPBOARD_LOG("%s", __FUNCTION__);

  if (!nsIClipboard::IsClipboardTypeSupported(aWhichClipboard)) {
    return NS_ERROR_FAILURE;
  }

  const auto& clipboardCache = mCaches[aWhichClipboard];
  MOZ_ASSERT(clipboardCache);
  if (aTransferable == clipboardCache->GetTransferable() &&
      anOwner == clipboardCache->GetClipboardOwner()) {
    CLIPBOARD_LOG("%s: skipping update.", __FUNCTION__);
    return NS_OK;
  }

  mEmptyingForSetData = true;
  if (NS_FAILED(EmptyClipboard(aWhichClipboard))) {
    CLIPBOARD_LOG("%s: emptying clipboard failed.", __FUNCTION__);
  }
  mEmptyingForSetData = false;

  clipboardCache->Update(aTransferable, anOwner);

  nsresult rv = NS_ERROR_FAILURE;
  if (aTransferable) {
    mIgnoreEmptyNotification = true;
    rv = ClipboardSetDataHelper::SetData(aTransferable, anOwner,
                                         aWhichClipboard);
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

  if (!nsIClipboard::IsClipboardTypeSupported(kSelectionClipboard) &&
      !nsIClipboard::IsClipboardTypeSupported(kFindClipboard) &&
      aWhichClipboard != kGlobalClipboard) {
    return NS_ERROR_FAILURE;
  }

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

  if (!nsIClipboard::IsClipboardTypeSupported(aWhichClipboard)) {
    return NS_ERROR_FAILURE;
  }

  if (mIgnoreEmptyNotification) {
    MOZ_DIAGNOSTIC_ASSERT(false, "How did we get here?");
    return NS_OK;
  }

  const auto& clipboardCache = mCaches[aWhichClipboard];
  MOZ_ASSERT(clipboardCache);
  clipboardCache->Clear();

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
nsBaseClipboard::IsClipboardTypeSupported(int32_t aWhichClipboard,
                                          bool* aRetval) {
  NS_ENSURE_ARG_POINTER(aRetval);
  switch (aWhichClipboard) {
    case kGlobalClipboard:
      // We always support the global clipboard.
      *aRetval = true;
      return NS_OK;
    case kSelectionClipboard:
      *aRetval = mClipboardCaps.supportsSelectionClipboard();
      return NS_OK;
    case kFindClipboard:
      *aRetval = mClipboardCaps.supportsFindClipboard();
      return NS_OK;
    case kSelectionCache:
      *aRetval = mClipboardCaps.supportsSelectionCache();
      return NS_OK;
    default:
      *aRetval = false;
      return NS_OK;
  }
}

void nsBaseClipboard::ClipboardCache::Clear() {
  if (mClipboardOwner) {
    mClipboardOwner->LosingOwnership(mTransferable);
    mClipboardOwner = nullptr;
  }
  mTransferable = nullptr;
}
