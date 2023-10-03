/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsBaseClipboard.h"

#include "mozilla/StaticPrefs_widget.h"
#include "nsIClipboardOwner.h"
#include "nsError.h"
#include "nsXPCOM.h"

using mozilla::GenericPromise;
using mozilla::LogLevel;
using mozilla::UniquePtr;
using mozilla::dom::ClipboardCapabilities;

NS_IMPL_ISUPPORTS(nsBaseClipboard::AsyncSetClipboardData,
                  nsIAsyncSetClipboardData)

nsBaseClipboard::AsyncSetClipboardData::AsyncSetClipboardData(
    int32_t aClipboardType, nsBaseClipboard* aClipboard,
    nsIAsyncSetClipboardDataCallback* aCallback)
    : mClipboardType(aClipboardType),
      mClipboard(aClipboard),
      mCallback(aCallback) {
  MOZ_ASSERT(mClipboard);
  MOZ_ASSERT(
      mClipboard->nsIClipboard::IsClipboardTypeSupported(mClipboardType));
}

NS_IMETHODIMP
nsBaseClipboard::AsyncSetClipboardData::SetData(nsITransferable* aTransferable,
                                                nsIClipboardOwner* aOwner) {
  MOZ_CLIPBOARD_LOG("AsyncSetClipboardData::SetData (%p): clipboard=%d", this,
                    mClipboardType);

  if (!IsValid()) {
    return NS_ERROR_FAILURE;
  }

  if (MOZ_CLIPBOARD_LOG_ENABLED()) {
    nsTArray<nsCString> flavors;
    if (NS_SUCCEEDED(aTransferable->FlavorsTransferableCanImport(flavors))) {
      for (const auto& flavor : flavors) {
        MOZ_CLIPBOARD_LOG("    MIME %s", flavor.get());
      }
    }
  }

  MOZ_ASSERT(mClipboard);
  MOZ_ASSERT(
      mClipboard->nsIClipboard::IsClipboardTypeSupported(mClipboardType));
  MOZ_DIAGNOSTIC_ASSERT(mClipboard->mPendingWriteRequests[mClipboardType] ==
                        this);

  RefPtr<AsyncSetClipboardData> request =
      std::move(mClipboard->mPendingWriteRequests[mClipboardType]);
  nsresult rv = mClipboard->SetData(aTransferable, aOwner, mClipboardType);
  MaybeNotifyCallback(rv);

  return rv;
}

NS_IMETHODIMP
nsBaseClipboard::AsyncSetClipboardData::Abort(nsresult aReason) {
  // Note: This may be called during destructor, so it should not attempt to
  // take a reference to mClipboard.

  if (!IsValid() || !NS_FAILED(aReason)) {
    return NS_ERROR_FAILURE;
  }

  MaybeNotifyCallback(aReason);
  return NS_OK;
}

void nsBaseClipboard::AsyncSetClipboardData::MaybeNotifyCallback(
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

void nsBaseClipboard::RejectPendingAsyncSetDataRequestIfAny(
    int32_t aClipboardType) {
  MOZ_ASSERT(nsIClipboard::IsClipboardTypeSupported(aClipboardType));
  auto& request = mPendingWriteRequests[aClipboardType];
  if (request) {
    request->Abort(NS_ERROR_ABORT);
    request = nullptr;
  }
}

NS_IMETHODIMP nsBaseClipboard::AsyncSetData(
    int32_t aWhichClipboard, nsIAsyncSetClipboardDataCallback* aCallback,
    nsIAsyncSetClipboardData** _retval) {
  MOZ_CLIPBOARD_LOG("%s: clipboard=%d", __FUNCTION__, aWhichClipboard);

  *_retval = nullptr;
  if (!nsIClipboard::IsClipboardTypeSupported(aWhichClipboard)) {
    MOZ_CLIPBOARD_LOG("%s: clipboard %d is not supported.", __FUNCTION__,
                      aWhichClipboard);
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

nsBaseClipboard::~nsBaseClipboard() {
  for (auto& request : mPendingWriteRequests) {
    if (request) {
      request->Abort(NS_ERROR_ABORT);
      request = nullptr;
    }
  }
}

NS_IMPL_ISUPPORTS(nsBaseClipboard, nsIClipboard)

/**
 * Sets the transferable object
 *
 */
NS_IMETHODIMP nsBaseClipboard::SetData(nsITransferable* aTransferable,
                                       nsIClipboardOwner* aOwner,
                                       int32_t aWhichClipboard) {
  NS_ASSERTION(aTransferable, "clipboard given a null transferable");

  MOZ_CLIPBOARD_LOG("%s: clipboard=%d", __FUNCTION__, aWhichClipboard);

  if (!nsIClipboard::IsClipboardTypeSupported(aWhichClipboard)) {
    MOZ_CLIPBOARD_LOG("%s: clipboard %d is not supported.", __FUNCTION__,
                      aWhichClipboard);
    return NS_ERROR_FAILURE;
  }

  if (MOZ_CLIPBOARD_LOG_ENABLED()) {
    nsTArray<nsCString> flavors;
    if (NS_SUCCEEDED(aTransferable->FlavorsTransferableCanImport(flavors))) {
      for (const auto& flavor : flavors) {
        MOZ_CLIPBOARD_LOG("    MIME %s", flavor.get());
      }
    }
  }

  const auto& clipboardCache = mCaches[aWhichClipboard];
  MOZ_ASSERT(clipboardCache);
  if (aTransferable == clipboardCache->GetTransferable() &&
      aOwner == clipboardCache->GetClipboardOwner()) {
    MOZ_CLIPBOARD_LOG("%s: skipping update.", __FUNCTION__);
    return NS_OK;
  }

  clipboardCache->Clear();

  nsresult rv = NS_ERROR_FAILURE;
  if (aTransferable) {
    mIgnoreEmptyNotification = true;
    // Reject existing pending asyncSetData request if any.
    RejectPendingAsyncSetDataRequestIfAny(aWhichClipboard);
    rv = SetNativeClipboardData(aTransferable, aOwner, aWhichClipboard);
    mIgnoreEmptyNotification = false;
  }
  if (NS_FAILED(rv)) {
    MOZ_CLIPBOARD_LOG("%s: setting native clipboard data failed.",
                      __FUNCTION__);
    return rv;
  }

  auto result = GetNativeClipboardSequenceNumber(aWhichClipboard);
  if (result.isErr()) {
    MOZ_CLIPBOARD_LOG("%s: getting native clipboard change count failed.",
                      __FUNCTION__);
    return result.unwrapErr();
  }

  clipboardCache->Update(aTransferable, aOwner, result.unwrap());
  return NS_OK;
}

nsresult nsBaseClipboard::GetDataFromClipboardCache(
    nsITransferable* aTransferable, int32_t aClipboardType) {
  MOZ_ASSERT(aTransferable);
  MOZ_ASSERT(nsIClipboard::IsClipboardTypeSupported(aClipboardType));
  MOZ_ASSERT(mozilla::StaticPrefs::widget_clipboard_use_cached_data_enabled());

  const auto* clipboardCache = GetClipboardCacheIfValid(aClipboardType);
  if (!clipboardCache) {
    return NS_ERROR_FAILURE;
  }

  nsITransferable* cachedTransferable = clipboardCache->GetTransferable();
  MOZ_ASSERT(cachedTransferable);

  // get flavor list that includes all acceptable flavors (including ones
  // obtained through conversion)
  nsTArray<nsCString> flavors;
  if (NS_FAILED(aTransferable->FlavorsTransferableCanImport(flavors))) {
    return NS_ERROR_FAILURE;
  }

  for (const auto& flavor : flavors) {
    nsCOMPtr<nsISupports> dataSupports;
    if (NS_SUCCEEDED(cachedTransferable->GetTransferData(
            flavor.get(), getter_AddRefs(dataSupports)))) {
      MOZ_CLIPBOARD_LOG("%s: getting %s from cache.", __FUNCTION__,
                        flavor.get());
      aTransferable->SetTransferData(flavor.get(), dataSupports);
      // maybe try to fill in more types? Is there a point?
      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
}

/**
 * Gets the transferable object from system clipboard.
 */
NS_IMETHODIMP nsBaseClipboard::GetData(nsITransferable* aTransferable,
                                       int32_t aWhichClipboard) {
  MOZ_CLIPBOARD_LOG("%s: clipboard=%d", __FUNCTION__, aWhichClipboard);

  if (!aTransferable) {
    NS_ASSERTION(false, "clipboard given a null transferable");
    return NS_ERROR_FAILURE;
  }

  if (mozilla::StaticPrefs::widget_clipboard_use_cached_data_enabled()) {
    // If we were the last ones to put something on the native clipboard, then
    // just use the cached transferable. Otherwise clear it because it isn't
    // relevant any more.
    if (NS_SUCCEEDED(
            GetDataFromClipboardCache(aTransferable, aWhichClipboard))) {
      // maybe try to fill in more types? Is there a point?
      return NS_OK;
    }

    // at this point we can't satisfy the request from cache data so let's look
    // for things other people put on the system clipboard
  }

  return GetNativeClipboardData(aTransferable, aWhichClipboard);
}

RefPtr<GenericPromise> nsBaseClipboard::AsyncGetData(
    nsITransferable* aTransferable, int32_t aWhichClipboard) {
  MOZ_CLIPBOARD_LOG("%s: clipboard=%d", __FUNCTION__, aWhichClipboard);

  if (!aTransferable) {
    NS_ASSERTION(false, "clipboard given a null transferable");
    return GenericPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  if (mozilla::StaticPrefs::widget_clipboard_use_cached_data_enabled()) {
    // If we were the last ones to put something on the native clipboard, then
    // just use the cached transferable. Otherwise clear it because it isn't
    // relevant any more.
    if (NS_SUCCEEDED(
            GetDataFromClipboardCache(aTransferable, aWhichClipboard))) {
      // maybe try to fill in more types? Is there a point?
      return GenericPromise::CreateAndResolve(true, __func__);
    }

    // at this point we can't satisfy the request from cache data so let's look
    // for things other people put on the system clipboard
  }

  RefPtr<GenericPromise::Private> dataPromise =
      mozilla::MakeRefPtr<GenericPromise::Private>(__func__);
  AsyncGetNativeClipboardData(aTransferable, aWhichClipboard,
                              [dataPromise](nsresult aResult) {
                                if (NS_FAILED(aResult)) {
                                  dataPromise->Reject(aResult, __func__);
                                  return;
                                }

                                dataPromise->Resolve(true, __func__);
                              });
  return dataPromise.forget();
}

NS_IMETHODIMP nsBaseClipboard::EmptyClipboard(int32_t aWhichClipboard) {
  MOZ_CLIPBOARD_LOG("%s: clipboard=%d", __FUNCTION__, aWhichClipboard);

  if (!nsIClipboard::IsClipboardTypeSupported(aWhichClipboard)) {
    MOZ_CLIPBOARD_LOG("%s: clipboard %d is not supported.", __FUNCTION__,
                      aWhichClipboard);
    return NS_ERROR_FAILURE;
  }

  EmptyNativeClipboardData(aWhichClipboard);

  const auto& clipboardCache = mCaches[aWhichClipboard];
  MOZ_ASSERT(clipboardCache);

  if (mIgnoreEmptyNotification) {
    MOZ_DIAGNOSTIC_ASSERT(!clipboardCache->GetTransferable() &&
                              !clipboardCache->GetClipboardOwner() &&
                              clipboardCache->GetSequenceNumber() == -1,
                          "How did we have data in clipboard cache here?");
    return NS_OK;
  }

  clipboardCache->Clear();

  return NS_OK;
}

mozilla::Result<nsTArray<nsCString>, nsresult>
nsBaseClipboard::GetFlavorsFromClipboardCache(int32_t aClipboardType) {
  MOZ_ASSERT(mozilla::StaticPrefs::widget_clipboard_use_cached_data_enabled());
  MOZ_ASSERT(nsIClipboard::IsClipboardTypeSupported(aClipboardType));

  const auto* clipboardCache = GetClipboardCacheIfValid(aClipboardType);
  if (!clipboardCache) {
    return mozilla::Err(NS_ERROR_FAILURE);
  }

  nsITransferable* cachedTransferable = clipboardCache->GetTransferable();
  MOZ_ASSERT(cachedTransferable);

  nsTArray<nsCString> flavors;
  nsresult rv = cachedTransferable->FlavorsTransferableCanImport(flavors);
  if (NS_FAILED(rv)) {
    return mozilla::Err(rv);
  }

  if (MOZ_CLIPBOARD_LOG_ENABLED()) {
    MOZ_CLIPBOARD_LOG("    Cached transferable types (nums %zu)\n",
                      flavors.Length());
    for (const auto& flavor : flavors) {
      MOZ_CLIPBOARD_LOG("        MIME %s", flavor.get());
    }
  }

  return std::move(flavors);
}

NS_IMETHODIMP
nsBaseClipboard::HasDataMatchingFlavors(const nsTArray<nsCString>& aFlavorList,
                                        int32_t aWhichClipboard,
                                        bool* aOutResult) {
  MOZ_CLIPBOARD_LOG("%s: clipboard=%d", __FUNCTION__, aWhichClipboard);
  if (MOZ_CLIPBOARD_LOG_ENABLED()) {
    MOZ_CLIPBOARD_LOG("    Asking for content clipboard=%i:\n",
                      aWhichClipboard);
    for (const auto& flavor : aFlavorList) {
      MOZ_CLIPBOARD_LOG("        MIME %s", flavor.get());
    }
  }

  *aOutResult = false;

  if (mozilla::StaticPrefs::widget_clipboard_use_cached_data_enabled()) {
    // First, check if we have valid data in our cached transferable.
    auto flavorsOrError = GetFlavorsFromClipboardCache(aWhichClipboard);
    if (flavorsOrError.isOk()) {
      for (const auto& transferableFlavor : flavorsOrError.unwrap()) {
        for (const auto& flavor : aFlavorList) {
          if (transferableFlavor.Equals(flavor)) {
            MOZ_CLIPBOARD_LOG("    has %s", flavor.get());
            *aOutResult = true;
            return NS_OK;
          }
        }
      }
    }
  }

  auto resultOrError =
      HasNativeClipboardDataMatchingFlavors(aFlavorList, aWhichClipboard);
  if (resultOrError.isErr()) {
    MOZ_CLIPBOARD_LOG(
        "%s: checking native clipboard data matching flavors falied.",
        __FUNCTION__);
    return resultOrError.unwrapErr();
  }

  *aOutResult = resultOrError.unwrap();
  return NS_OK;
}

RefPtr<DataFlavorsPromise> nsBaseClipboard::AsyncHasDataMatchingFlavors(
    const nsTArray<nsCString>& aFlavorList, int32_t aWhichClipboard) {
  MOZ_CLIPBOARD_LOG("%s: clipboard=%d", __FUNCTION__, aWhichClipboard);
  if (MOZ_CLIPBOARD_LOG_ENABLED()) {
    MOZ_CLIPBOARD_LOG("    Asking for content clipboard=%i:\n",
                      aWhichClipboard);
    for (const auto& flavor : aFlavorList) {
      MOZ_CLIPBOARD_LOG("        MIME %s", flavor.get());
    }
  }

  if (mozilla::StaticPrefs::widget_clipboard_use_cached_data_enabled()) {
    // First, check if we have valid data in our cached transferable.
    auto flavorsOrError = GetFlavorsFromClipboardCache(aWhichClipboard);
    if (flavorsOrError.isOk()) {
      nsTArray<nsCString> results;
      for (const auto& transferableFlavor : flavorsOrError.unwrap()) {
        for (const auto& flavor : aFlavorList) {
          if (transferableFlavor.Equals(flavor)) {
            MOZ_CLIPBOARD_LOG("    has %s", flavor.get());
            results.AppendElement(flavor);
          }
        }
      }
      return DataFlavorsPromise::CreateAndResolve(std::move(results), __func__);
    }
  }

  RefPtr<DataFlavorsPromise::Private> flavorPromise =
      mozilla::MakeRefPtr<DataFlavorsPromise::Private>(__func__);
  AsyncHasNativeClipboardDataMatchingFlavors(
      aFlavorList, aWhichClipboard, [flavorPromise](auto aResultOrError) {
        if (aResultOrError.isErr()) {
          flavorPromise->Reject(aResultOrError.unwrapErr(), __func__);
          return;
        }

        flavorPromise->Resolve(std::move(aResultOrError.unwrap()), __func__);
      });
  return flavorPromise.forget();
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

void nsBaseClipboard::AsyncHasNativeClipboardDataMatchingFlavors(
    const nsTArray<nsCString>& aFlavorList, int32_t aWhichClipboard,
    HasMatchingFlavorsCallback&& aCallback) {
  MOZ_DIAGNOSTIC_ASSERT(
      nsIClipboard::IsClipboardTypeSupported(aWhichClipboard));

  MOZ_CLIPBOARD_LOG(
      "nsBaseClipboard::AsyncHasNativeClipboardDataMatchingFlavors: "
      "clipboard=%d",
      aWhichClipboard);

  nsTArray<nsCString> results;
  for (const auto& flavor : aFlavorList) {
    auto resultOrError = HasNativeClipboardDataMatchingFlavors(
        AutoTArray<nsCString, 1>{flavor}, aWhichClipboard);
    if (resultOrError.isOk() && resultOrError.unwrap()) {
      results.AppendElement(flavor);
    }
  }
  aCallback(std::move(results));
}

void nsBaseClipboard::AsyncGetNativeClipboardData(
    nsITransferable* aTransferable, int32_t aWhichClipboard,
    GetDataCallback&& aCallback) {
  aCallback(GetNativeClipboardData(aTransferable, aWhichClipboard));
}

void nsBaseClipboard::ClearClipboardCache(int32_t aClipboardType) {
  MOZ_ASSERT(nsIClipboard::IsClipboardTypeSupported(aClipboardType));
  const mozilla::UniquePtr<ClipboardCache>& cache = mCaches[aClipboardType];
  MOZ_ASSERT(cache);
  cache->Clear();
}

nsBaseClipboard::ClipboardCache* nsBaseClipboard::GetClipboardCacheIfValid(
    int32_t aClipboardType) {
  MOZ_ASSERT(nsIClipboard::IsClipboardTypeSupported(aClipboardType));

  const mozilla::UniquePtr<ClipboardCache>& cache = mCaches[aClipboardType];
  MOZ_ASSERT(cache);

  if (!cache->GetTransferable()) {
    MOZ_ASSERT(cache->GetSequenceNumber() == -1);
    return nullptr;
  }

  auto changeCountOrError = GetNativeClipboardSequenceNumber(aClipboardType);
  if (changeCountOrError.isErr()) {
    return nullptr;
  }

  if (changeCountOrError.unwrap() != cache->GetSequenceNumber()) {
    // Clipboard cache is invalid, clear it.
    cache->Clear();
    return nullptr;
  }

  return cache.get();
}

void nsBaseClipboard::ClipboardCache::Clear() {
  if (mClipboardOwner) {
    mClipboardOwner->LosingOwnership(mTransferable);
    mClipboardOwner = nullptr;
  }
  mTransferable = nullptr;
  mSequenceNumber = -1;
}
