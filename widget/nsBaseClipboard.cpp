/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsBaseClipboard.h"

#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/StaticPrefs_widget.h"
#include "nsContentUtils.h"
#include "nsFocusManager.h"
#include "nsIClipboardOwner.h"
#include "nsIPromptService.h"
#include "nsError.h"
#include "nsXPCOM.h"

using mozilla::GenericPromise;
using mozilla::LogLevel;
using mozilla::UniquePtr;
using mozilla::dom::BrowsingContext;
using mozilla::dom::CanonicalBrowsingContext;
using mozilla::dom::ClipboardCapabilities;
using mozilla::dom::Document;

static const int32_t kGetAvailableFlavorsRetryCount = 5;

namespace {

struct ClipboardGetRequest {
  ClipboardGetRequest(const nsTArray<nsCString>& aFlavorList,
                      nsIAsyncClipboardGetCallback* aCallback)
      : mFlavorList(aFlavorList.Clone()), mCallback(aCallback) {}

  const nsTArray<nsCString> mFlavorList;
  const nsCOMPtr<nsIAsyncClipboardGetCallback> mCallback;
};

class UserConfirmationRequest final
    : public mozilla::dom::PromiseNativeHandler {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(UserConfirmationRequest)

  UserConfirmationRequest(int32_t aClipboardType,
                          Document* aRequestingChromeDocument,
                          nsIPrincipal* aRequestingPrincipal,
                          nsBaseClipboard* aClipboard)
      : mClipboardType(aClipboardType),
        mRequestingChromeDocument(aRequestingChromeDocument),
        mRequestingPrincipal(aRequestingPrincipal),
        mClipboard(aClipboard) {
    MOZ_ASSERT(
        mClipboard->nsIClipboard::IsClipboardTypeSupported(aClipboardType));
  }

  void ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                        mozilla::ErrorResult& aRv) override;

  void RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                        mozilla::ErrorResult& aRv) override;

  bool IsEqual(int32_t aClipboardType, Document* aRequestingChromeDocument,
               nsIPrincipal* aRequestingPrincipal) const {
    return ClipboardType() == aClipboardType &&
           RequestingChromeDocument() == aRequestingChromeDocument &&
           RequestingPrincipal()->Equals(aRequestingPrincipal);
  }

  int32_t ClipboardType() const { return mClipboardType; }

  Document* RequestingChromeDocument() const {
    return mRequestingChromeDocument;
  }

  nsIPrincipal* RequestingPrincipal() const { return mRequestingPrincipal; }

  void AddClipboardGetRequest(const nsTArray<nsCString>& aFlavorList,
                              nsIAsyncClipboardGetCallback* aCallback) {
    MOZ_ASSERT(!aFlavorList.IsEmpty());
    MOZ_ASSERT(aCallback);
    mPendingClipboardGetRequests.AppendElement(
        mozilla::MakeUnique<ClipboardGetRequest>(aFlavorList, aCallback));
  }

  void RejectPendingClipboardGetRequests(nsresult aError) {
    MOZ_ASSERT(NS_FAILED(aError));
    auto requests = std::move(mPendingClipboardGetRequests);
    for (const auto& request : requests) {
      MOZ_ASSERT(request);
      MOZ_ASSERT(request->mCallback);
      request->mCallback->OnError(aError);
    }
  }

  void ProcessPendingClipboardGetRequests() {
    auto requests = std::move(mPendingClipboardGetRequests);
    for (const auto& request : requests) {
      MOZ_ASSERT(request);
      MOZ_ASSERT(!request->mFlavorList.IsEmpty());
      MOZ_ASSERT(request->mCallback);
      mClipboard->AsyncGetDataInternal(request->mFlavorList, mClipboardType,
                                       request->mCallback);
    }
  }

  nsTArray<UniquePtr<ClipboardGetRequest>>& GetPendingClipboardGetRequests() {
    return mPendingClipboardGetRequests;
  }

 private:
  ~UserConfirmationRequest() = default;

  const int32_t mClipboardType;
  RefPtr<Document> mRequestingChromeDocument;
  const nsCOMPtr<nsIPrincipal> mRequestingPrincipal;
  const RefPtr<nsBaseClipboard> mClipboard;
  // Track the pending read requests that wait for user confirmation.
  nsTArray<UniquePtr<ClipboardGetRequest>> mPendingClipboardGetRequests;
};

NS_IMPL_CYCLE_COLLECTION(UserConfirmationRequest, mRequestingChromeDocument)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(UserConfirmationRequest)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(UserConfirmationRequest)
NS_IMPL_CYCLE_COLLECTING_RELEASE(UserConfirmationRequest)

static mozilla::StaticRefPtr<UserConfirmationRequest> sUserConfirmationRequest;

void UserConfirmationRequest::ResolvedCallback(JSContext* aCx,
                                               JS::Handle<JS::Value> aValue,
                                               mozilla::ErrorResult& aRv) {
  MOZ_DIAGNOSTIC_ASSERT(sUserConfirmationRequest == this);
  sUserConfirmationRequest = nullptr;

  JS::Rooted<JSObject*> detailObj(aCx, &aValue.toObject());
  nsCOMPtr<nsIPropertyBag2> propBag;
  nsresult rv = mozilla::dom::UnwrapArg<nsIPropertyBag2>(
      aCx, detailObj, getter_AddRefs(propBag));
  if (NS_FAILED(rv)) {
    RejectPendingClipboardGetRequests(rv);
    return;
  }

  bool result = false;
  rv = propBag->GetPropertyAsBool(u"ok"_ns, &result);
  if (NS_FAILED(rv)) {
    RejectPendingClipboardGetRequests(rv);
    return;
  }

  if (!result) {
    RejectPendingClipboardGetRequests(NS_ERROR_DOM_NOT_ALLOWED_ERR);
    return;
  }

  ProcessPendingClipboardGetRequests();
}

void UserConfirmationRequest::RejectedCallback(JSContext* aCx,
                                               JS::Handle<JS::Value> aValue,
                                               mozilla::ErrorResult& aRv) {
  MOZ_DIAGNOSTIC_ASSERT(sUserConfirmationRequest == this);
  sUserConfirmationRequest = nullptr;
  RejectPendingClipboardGetRequests(NS_ERROR_FAILURE);
}

}  // namespace

NS_IMPL_ISUPPORTS(nsBaseClipboard::AsyncSetClipboardData,
                  nsIAsyncSetClipboardData)

nsBaseClipboard::AsyncSetClipboardData::AsyncSetClipboardData(
    int32_t aClipboardType, nsBaseClipboard* aClipboard,
    nsIAsyncClipboardRequestCallback* aCallback)
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
  if (nsCOMPtr<nsIAsyncClipboardRequestCallback> callback =
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
    int32_t aWhichClipboard, nsIAsyncClipboardRequestCallback* aCallback,
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
    rv = SetNativeClipboardData(aTransferable, aWhichClipboard);
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

  return clipboardCache->GetData(aTransferable);
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

  if (!nsIClipboard::IsClipboardTypeSupported(aWhichClipboard)) {
    MOZ_CLIPBOARD_LOG("%s: clipboard %d is not supported.", __FUNCTION__,
                      aWhichClipboard);
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

void nsBaseClipboard::MaybeRetryGetAvailableFlavors(
    const nsTArray<nsCString>& aFlavorList, int32_t aWhichClipboard,
    nsIAsyncClipboardGetCallback* aCallback, int32_t aRetryCount) {
  // Note we have to get the clipboard sequence number first before the actual
  // read. This is to use it to verify the clipboard data is still the one we
  // try to read, instead of the later state.
  auto sequenceNumberOrError =
      GetNativeClipboardSequenceNumber(aWhichClipboard);
  if (sequenceNumberOrError.isErr()) {
    MOZ_CLIPBOARD_LOG("%s: unable to get sequence number for clipboard %d.",
                      __FUNCTION__, aWhichClipboard);
    aCallback->OnError(sequenceNumberOrError.unwrapErr());
    return;
  }

  int32_t sequenceNumber = sequenceNumberOrError.unwrap();
  AsyncHasNativeClipboardDataMatchingFlavors(
      aFlavorList, aWhichClipboard,
      [self = RefPtr{this}, callback = nsCOMPtr{aCallback}, aWhichClipboard,
       aRetryCount, flavorList = aFlavorList.Clone(),
       sequenceNumber](auto aFlavorsOrError) {
        if (aFlavorsOrError.isErr()) {
          MOZ_CLIPBOARD_LOG(
              "%s: unable to get available flavors for clipboard %d.",
              __FUNCTION__, aWhichClipboard);
          callback->OnError(aFlavorsOrError.unwrapErr());
          return;
        }

        auto sequenceNumberOrError =
            self->GetNativeClipboardSequenceNumber(aWhichClipboard);
        if (sequenceNumberOrError.isErr()) {
          MOZ_CLIPBOARD_LOG(
              "%s: unable to get sequence number for clipboard %d.",
              __FUNCTION__, aWhichClipboard);
          callback->OnError(sequenceNumberOrError.unwrapErr());
          return;
        }

        if (sequenceNumber == sequenceNumberOrError.unwrap()) {
          auto asyncGetClipboardData =
              mozilla::MakeRefPtr<AsyncGetClipboardData>(
                  aWhichClipboard, sequenceNumber,
                  std::move(aFlavorsOrError.unwrap()), false, self);
          callback->OnSuccess(asyncGetClipboardData);
          return;
        }

        if (aRetryCount > 0) {
          MOZ_CLIPBOARD_LOG(
              "%s: clipboard=%d, ignore the data due to the sequence number "
              "doesn't match, retry (%d) ..",
              __FUNCTION__, aWhichClipboard, aRetryCount);
          self->MaybeRetryGetAvailableFlavors(flavorList, aWhichClipboard,
                                              callback, aRetryCount - 1);
          return;
        }

        MOZ_DIAGNOSTIC_ASSERT(false, "How can this happen?!?");
        callback->OnError(NS_ERROR_FAILURE);
      });
}

NS_IMETHODIMP nsBaseClipboard::AsyncGetData(
    const nsTArray<nsCString>& aFlavorList, int32_t aWhichClipboard,
    mozilla::dom::WindowContext* aRequestingWindowContext,
    nsIPrincipal* aRequestingPrincipal,
    nsIAsyncClipboardGetCallback* aCallback) {
  MOZ_CLIPBOARD_LOG("%s: clipboard=%d", __FUNCTION__, aWhichClipboard);

  if (!aCallback || !aRequestingPrincipal || aFlavorList.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  if (!nsIClipboard::IsClipboardTypeSupported(aWhichClipboard)) {
    MOZ_CLIPBOARD_LOG("%s: clipboard %d is not supported.", __FUNCTION__,
                      aWhichClipboard);
    return NS_ERROR_FAILURE;
  }

  // We want to disable security check for automated tests that have the pref
  // set to true, or extension that have clipboard read permission.
  if (mozilla::StaticPrefs::
          dom_events_testing_asyncClipboard_DoNotUseDirectly() ||
      nsContentUtils::PrincipalHasPermission(*aRequestingPrincipal,
                                             nsGkAtoms::clipboardRead)) {
    AsyncGetDataInternal(aFlavorList, aWhichClipboard, aCallback);
    return NS_OK;
  }

  // If cache data is valid, we are the last ones to put something on the native
  // clipboard, then check if the data is from the same-origin page,
  if (auto* clipboardCache = GetClipboardCacheIfValid(aWhichClipboard)) {
    nsCOMPtr<nsITransferable> trans = clipboardCache->GetTransferable();
    MOZ_ASSERT(trans);

    if (nsCOMPtr<nsIPrincipal> principal = trans->GetRequestingPrincipal()) {
      if (aRequestingPrincipal->Subsumes(principal)) {
        MOZ_CLIPBOARD_LOG("%s: native clipboard data is from same-origin page.",
                          __FUNCTION__);
        AsyncGetDataInternal(aFlavorList, aWhichClipboard, aCallback);
        return NS_OK;
      }
    }
  }

  // TODO: enable showing the "Paste" button in this case; see bug 1773681.
  if (aRequestingPrincipal->GetIsAddonOrExpandedAddonPrincipal()) {
    MOZ_CLIPBOARD_LOG("%s: Addon without read permission.", __FUNCTION__);
    return aCallback->OnError(NS_ERROR_FAILURE);
  }

  RequestUserConfirmation(aWhichClipboard, aFlavorList,
                          aRequestingWindowContext, aRequestingPrincipal,
                          aCallback);
  return NS_OK;
}

void nsBaseClipboard::AsyncGetDataInternal(
    const nsTArray<nsCString>& aFlavorList, int32_t aClipboardType,
    nsIAsyncClipboardGetCallback* aCallback) {
  MOZ_ASSERT(nsIClipboard::IsClipboardTypeSupported(aClipboardType));

  if (mozilla::StaticPrefs::widget_clipboard_use_cached_data_enabled()) {
    // If we were the last ones to put something on the native clipboard, then
    // just use the cached transferable. Otherwise clear it because it isn't
    // relevant any more.
    if (auto* clipboardCache = GetClipboardCacheIfValid(aClipboardType)) {
      nsITransferable* cachedTransferable = clipboardCache->GetTransferable();
      MOZ_ASSERT(cachedTransferable);

      nsTArray<nsCString> transferableFlavors;
      if (NS_SUCCEEDED(cachedTransferable->FlavorsTransferableCanExport(
              transferableFlavors))) {
        nsTArray<nsCString> results;
        for (const auto& transferableFlavor : transferableFlavors) {
          for (const auto& flavor : aFlavorList) {
            // XXX We need special check for image as we always put the
            // image as "native" on the clipboard.
            if (transferableFlavor.Equals(flavor) ||
                (transferableFlavor.Equals(kNativeImageMime) &&
                 nsContentUtils::IsFlavorImage(flavor))) {
              MOZ_CLIPBOARD_LOG("    has %s", flavor.get());
              results.AppendElement(flavor);
            }
          }
        }

        // XXX Do we need to check system clipboard for the flavors that cannot
        // be found in cache?
        auto asyncGetClipboardData = mozilla::MakeRefPtr<AsyncGetClipboardData>(
            aClipboardType, clipboardCache->GetSequenceNumber(),
            std::move(results), true, this);
        aCallback->OnSuccess(asyncGetClipboardData);
        return;
      }
    }

    // At this point we can't satisfy the request from cache data so let's look
    // for things other people put on the system clipboard.
  }

  MaybeRetryGetAvailableFlavors(aFlavorList, aClipboardType, aCallback,
                                kGetAvailableFlavorsRetryCount);
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
  nsresult rv = cachedTransferable->FlavorsTransferableCanExport(flavors);
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

void nsBaseClipboard::RequestUserConfirmation(
    int32_t aClipboardType, const nsTArray<nsCString>& aFlavorList,
    mozilla::dom::WindowContext* aWindowContext,
    nsIPrincipal* aRequestingPrincipal,
    nsIAsyncClipboardGetCallback* aCallback) {
  MOZ_ASSERT(nsIClipboard::IsClipboardTypeSupported(aClipboardType));
  MOZ_ASSERT(aCallback);

  if (!aWindowContext) {
    aCallback->OnError(NS_ERROR_FAILURE);
    return;
  }

  CanonicalBrowsingContext* cbc =
      CanonicalBrowsingContext::Cast(aWindowContext->GetBrowsingContext());
  MOZ_ASSERT(
      cbc->IsContent(),
      "Should not require user confirmation when access from chrome window");

  RefPtr<CanonicalBrowsingContext> chromeTop = cbc->TopCrossChromeBoundary();
  Document* chromeDoc = chromeTop ? chromeTop->GetDocument() : nullptr;
  if (!chromeDoc || !chromeDoc->HasFocus(mozilla::IgnoreErrors())) {
    MOZ_CLIPBOARD_LOG("%s: reject due to not in the focused window",
                      __FUNCTION__);
    aCallback->OnError(NS_ERROR_FAILURE);
    return;
  }

  mozilla::dom::Element* activeElementInChromeDoc =
      chromeDoc->GetActiveElement();
  if (activeElementInChromeDoc != cbc->Top()->GetEmbedderElement()) {
    // Reject if the request is not from web content that is in the focused tab.
    MOZ_CLIPBOARD_LOG("%s: reject due to not in the focused tab", __FUNCTION__);
    aCallback->OnError(NS_ERROR_FAILURE);
    return;
  }

  // If there is a pending user confirmation request, check if we could reuse
  // it. If not, reject the request.
  if (sUserConfirmationRequest) {
    if (sUserConfirmationRequest->IsEqual(aClipboardType, chromeDoc,
                                          aRequestingPrincipal)) {
      sUserConfirmationRequest->AddClipboardGetRequest(aFlavorList, aCallback);
      return;
    }

    aCallback->OnError(NS_ERROR_DOM_NOT_ALLOWED_ERR);
    return;
  }

  nsresult rv = NS_ERROR_FAILURE;
  nsCOMPtr<nsIPromptService> promptService =
      do_GetService("@mozilla.org/prompter;1", &rv);
  if (NS_FAILED(rv)) {
    aCallback->OnError(NS_ERROR_DOM_NOT_ALLOWED_ERR);
    return;
  }

  RefPtr<mozilla::dom::Promise> promise;
  if (NS_FAILED(promptService->ConfirmUserPaste(aWindowContext->Canonical(),
                                                getter_AddRefs(promise)))) {
    aCallback->OnError(NS_ERROR_DOM_NOT_ALLOWED_ERR);
    return;
  }

  sUserConfirmationRequest = new UserConfirmationRequest(
      aClipboardType, chromeDoc, aRequestingPrincipal, this);
  sUserConfirmationRequest->AddClipboardGetRequest(aFlavorList, aCallback);
  promise->AppendNativeHandler(sUserConfirmationRequest);
}

NS_IMPL_ISUPPORTS(nsBaseClipboard::AsyncGetClipboardData,
                  nsIAsyncGetClipboardData)

nsBaseClipboard::AsyncGetClipboardData::AsyncGetClipboardData(
    int32_t aClipboardType, int32_t aSequenceNumber,
    nsTArray<nsCString>&& aFlavors, bool aFromCache,
    nsBaseClipboard* aClipboard)
    : mClipboardType(aClipboardType),
      mSequenceNumber(aSequenceNumber),
      mFlavors(std::move(aFlavors)),
      mFromCache(aFromCache),
      mClipboard(aClipboard) {
  MOZ_ASSERT(mClipboard);
  MOZ_ASSERT(
      mClipboard->nsIClipboard::IsClipboardTypeSupported(mClipboardType));
}

NS_IMETHODIMP nsBaseClipboard::AsyncGetClipboardData::GetValid(
    bool* aOutResult) {
  *aOutResult = IsValid();
  return NS_OK;
}

NS_IMETHODIMP nsBaseClipboard::AsyncGetClipboardData::GetFlavorList(
    nsTArray<nsCString>& aFlavors) {
  aFlavors.AppendElements(mFlavors);
  return NS_OK;
}

NS_IMETHODIMP nsBaseClipboard::AsyncGetClipboardData::GetData(
    nsITransferable* aTransferable,
    nsIAsyncClipboardRequestCallback* aCallback) {
  MOZ_CLIPBOARD_LOG("AsyncGetClipboardData::GetData: %p", this);

  if (!aTransferable || !aCallback) {
    return NS_ERROR_INVALID_ARG;
  }

  nsTArray<nsCString> flavors;
  nsresult rv = aTransferable->FlavorsTransferableCanImport(flavors);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // If the requested flavor is not in the list, throw an error.
  for (const auto& flavor : flavors) {
    if (!mFlavors.Contains(flavor)) {
      return NS_ERROR_FAILURE;
    }
  }

  if (!IsValid()) {
    aCallback->OnComplete(NS_ERROR_FAILURE);
    return NS_OK;
  }

  MOZ_ASSERT(mClipboard);

  if (mFromCache) {
    const auto* clipboardCache =
        mClipboard->GetClipboardCacheIfValid(mClipboardType);
    // `IsValid()` above ensures we should get a valid cache and matched
    // sequence number here.
    MOZ_DIAGNOSTIC_ASSERT(clipboardCache);
    MOZ_DIAGNOSTIC_ASSERT(clipboardCache->GetSequenceNumber() ==
                          mSequenceNumber);
    if (NS_SUCCEEDED(clipboardCache->GetData(aTransferable))) {
      aCallback->OnComplete(NS_OK);
      return NS_OK;
    }

    // At this point we can't satisfy the request from cache data so let's look
    // for things other people put on the system clipboard.
  }

  // Since this is an async operation, we need to check if the data is still
  // valid after we get the result.
  mClipboard->AsyncGetNativeClipboardData(
      aTransferable, mClipboardType,
      [callback = nsCOMPtr{aCallback}, self = RefPtr{this}](nsresult aResult) {
        // `IsValid()` checks the clipboard sequence number to ensure the data
        // we are requesting is still valid.
        callback->OnComplete(self->IsValid() ? aResult : NS_ERROR_FAILURE);
      });
  return NS_OK;
}

bool nsBaseClipboard::AsyncGetClipboardData::IsValid() {
  if (!mClipboard) {
    return false;
  }

  // If the data should from cache, check if cache is still valid or the
  // sequence numbers are matched.
  if (mFromCache) {
    const auto* clipboardCache =
        mClipboard->GetClipboardCacheIfValid(mClipboardType);
    if (!clipboardCache) {
      mClipboard = nullptr;
      return false;
    }

    return mSequenceNumber == clipboardCache->GetSequenceNumber();
  }

  auto resultOrError =
      mClipboard->GetNativeClipboardSequenceNumber(mClipboardType);
  if (resultOrError.isErr()) {
    mClipboard = nullptr;
    return false;
  }

  if (mSequenceNumber != resultOrError.unwrap()) {
    mClipboard = nullptr;
    return false;
  }

  return true;
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

nsresult nsBaseClipboard::ClipboardCache::GetData(
    nsITransferable* aTransferable) const {
  MOZ_ASSERT(aTransferable);
  MOZ_ASSERT(mozilla::StaticPrefs::widget_clipboard_use_cached_data_enabled());

  // get flavor list that includes all acceptable flavors (including ones
  // obtained through conversion)
  nsTArray<nsCString> flavors;
  if (NS_FAILED(aTransferable->FlavorsTransferableCanImport(flavors))) {
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(mTransferable);
  for (const auto& flavor : flavors) {
    nsCOMPtr<nsISupports> dataSupports;
    // XXX Maybe we need special check for image as we always put the image as
    // "native" on the clipboard.
    if (NS_SUCCEEDED(mTransferable->GetTransferData(
            flavor.get(), getter_AddRefs(dataSupports)))) {
      MOZ_CLIPBOARD_LOG("%s: getting %s from cache.", __FUNCTION__,
                        flavor.get());
      aTransferable->SetTransferData(flavor.get(), dataSupports);
      // XXX we only read the first available type from native clipboard, so
      // make cache behave the same.
      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
}
