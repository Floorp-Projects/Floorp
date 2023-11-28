/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsClipboardProxy.h"

#if defined(ACCESSIBILITY) && defined(XP_WIN)
#  include "mozilla/a11y/Compatibility.h"
#endif
#include "mozilla/ClipboardReadRequestChild.h"
#include "mozilla/ClipboardWriteRequestChild.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/net/CookieJarSettings.h"
#include "mozilla/Maybe.h"
#include "mozilla/Unused.h"
#include "nsArrayUtils.h"
#include "nsBaseClipboard.h"
#include "nsISupportsPrimitives.h"
#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsXULAppAPI.h"
#include "nsContentUtils.h"
#include "PermissionMessageUtils.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_ISUPPORTS(nsClipboardProxy, nsIClipboard, nsIClipboardProxy)

nsClipboardProxy::nsClipboardProxy() : mClipboardCaps(false, false, false) {}

NS_IMETHODIMP
nsClipboardProxy::SetData(nsITransferable* aTransferable,
                          nsIClipboardOwner* anOwner, int32_t aWhichClipboard) {
#if defined(ACCESSIBILITY) && defined(XP_WIN)
  a11y::Compatibility::SuppressA11yForClipboardCopy();
#endif

  ContentChild* child = ContentChild::GetSingleton();
  IPCTransferable ipcTransferable;
  nsContentUtils::TransferableToIPCTransferable(aTransferable, &ipcTransferable,
                                                false, nullptr);
  child->SendSetClipboard(std::move(ipcTransferable), aWhichClipboard);
  return NS_OK;
}

NS_IMETHODIMP nsClipboardProxy::AsyncSetData(
    int32_t aWhichClipboard, nsIAsyncClipboardRequestCallback* aCallback,
    nsIAsyncSetClipboardData** _retval) {
  RefPtr<ClipboardWriteRequestChild> request =
      MakeRefPtr<ClipboardWriteRequestChild>(aCallback);
  ContentChild::GetSingleton()->SendPClipboardWriteRequestConstructor(
      request, aWhichClipboard);
  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
nsClipboardProxy::GetData(nsITransferable* aTransferable,
                          int32_t aWhichClipboard) {
  nsTArray<nsCString> types;
  aTransferable->FlavorsTransferableCanImport(types);

  IPCTransferableData transferable;
  ContentChild::GetSingleton()->SendGetClipboard(types, aWhichClipboard,
                                                 &transferable);
  return nsContentUtils::IPCTransferableDataToTransferable(
      transferable, false /* aAddDataFlavor */, aTransferable,
      false /* aFilterUnknownFlavors */);
}

namespace {

class AsyncGetClipboardDataProxy final : public nsIAsyncGetClipboardData {
 public:
  explicit AsyncGetClipboardDataProxy(ClipboardReadRequestChild* aActor)
      : mActor(aActor) {
    MOZ_ASSERT(mActor);
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIASYNCGETCLIPBOARDDATA

 private:
  virtual ~AsyncGetClipboardDataProxy() {
    MOZ_ASSERT(mActor);
    if (mActor->CanSend()) {
      PClipboardReadRequestChild::Send__delete__(mActor);
    }
  };

  RefPtr<ClipboardReadRequestChild> mActor;
};

NS_IMPL_ISUPPORTS(AsyncGetClipboardDataProxy, nsIAsyncGetClipboardData)

NS_IMETHODIMP AsyncGetClipboardDataProxy::GetValid(bool* aOutResult) {
  MOZ_ASSERT(mActor);
  *aOutResult = mActor->CanSend();
  return NS_OK;
}

NS_IMETHODIMP AsyncGetClipboardDataProxy::GetFlavorList(
    nsTArray<nsCString>& aFlavorList) {
  MOZ_ASSERT(mActor);
  aFlavorList.AppendElements(mActor->FlavorList());
  return NS_OK;
}

NS_IMETHODIMP AsyncGetClipboardDataProxy::GetData(
    nsITransferable* aTransferable,
    nsIAsyncClipboardRequestCallback* aCallback) {
  if (!aTransferable || !aCallback) {
    return NS_ERROR_INVALID_ARG;
  }

  // Get a list of flavors this transferable can import
  nsTArray<nsCString> flavors;
  nsresult rv = aTransferable->FlavorsTransferableCanImport(flavors);
  if (NS_FAILED(rv)) {
    return rv;
  }

  MOZ_ASSERT(mActor);
  // If the requested flavor is not in the list, throw an error.
  for (const auto& flavor : flavors) {
    if (!mActor->FlavorList().Contains(flavor)) {
      return NS_ERROR_FAILURE;
    }
  }

  if (!mActor->CanSend()) {
    return aCallback->OnComplete(NS_ERROR_FAILURE);
  }

  mActor->SendGetData(flavors)->Then(
      GetMainThreadSerialEventTarget(), __func__,
      /* resolve */
      [self = RefPtr{this}, callback = nsCOMPtr{aCallback},
       transferable = nsCOMPtr{aTransferable}](
          const IPCTransferableDataOrError& aIpcTransferableDataOrError) {
        if (aIpcTransferableDataOrError.type() ==
            IPCTransferableDataOrError::Tnsresult) {
          MOZ_ASSERT(NS_FAILED(aIpcTransferableDataOrError.get_nsresult()));
          callback->OnComplete(aIpcTransferableDataOrError.get_nsresult());
          return;
        }

        nsresult rv = nsContentUtils::IPCTransferableDataToTransferable(
            aIpcTransferableDataOrError.get_IPCTransferableData(),
            false /* aAddDataFlavor */, transferable,
            false /* aFilterUnknownFlavors */);
        if (NS_FAILED(rv)) {
          callback->OnComplete(rv);
          return;
        }

        callback->OnComplete(NS_OK);
      },
      /* reject */
      [callback =
           nsCOMPtr{aCallback}](mozilla::ipc::ResponseRejectReason aReason) {
        callback->OnComplete(NS_ERROR_FAILURE);
      });

  return NS_OK;
}

}  // namespace

NS_IMETHODIMP nsClipboardProxy::AsyncGetData(
    const nsTArray<nsCString>& aFlavorList, int32_t aWhichClipboard,
    mozilla::dom::WindowContext* aRequestingWindowContext,
    nsIPrincipal* aRequestingPrincipal,
    nsIAsyncClipboardGetCallback* aCallback) {
  if (!aCallback || !aRequestingPrincipal || aFlavorList.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  if (!nsIClipboard::IsClipboardTypeSupported(aWhichClipboard)) {
    MOZ_CLIPBOARD_LOG("%s: clipboard %d is not supported.", __FUNCTION__,
                      aWhichClipboard);
    return NS_ERROR_FAILURE;
  }

  ContentChild::GetSingleton()
      ->SendGetClipboardAsync(aFlavorList, aWhichClipboard,
                              aRequestingWindowContext,
                              WrapNotNull(aRequestingPrincipal))
      ->Then(
          GetMainThreadSerialEventTarget(), __func__,
          /* resolve */
          [callback = nsCOMPtr{aCallback}](const PClipboardReadRequestOrError&
                                               aClipboardReadRequestOrError) {
            if (aClipboardReadRequestOrError.type() ==
                PClipboardReadRequestOrError::Tnsresult) {
              MOZ_ASSERT(
                  NS_FAILED(aClipboardReadRequestOrError.get_nsresult()));
              callback->OnError(aClipboardReadRequestOrError.get_nsresult());
              return;
            }

            auto asyncGetClipboardData = MakeRefPtr<AsyncGetClipboardDataProxy>(
                static_cast<ClipboardReadRequestChild*>(
                    aClipboardReadRequestOrError.get_PClipboardReadRequest()
                        .AsChild()
                        .get()));

            callback->OnSuccess(asyncGetClipboardData);
          },
          /* reject */
          [callback = nsCOMPtr{aCallback}](
              mozilla::ipc::ResponseRejectReason aReason) {
            callback->OnError(NS_ERROR_FAILURE);
          });
  return NS_OK;
}

NS_IMETHODIMP
nsClipboardProxy::EmptyClipboard(int32_t aWhichClipboard) {
  ContentChild::GetSingleton()->SendEmptyClipboard(aWhichClipboard);
  return NS_OK;
}

NS_IMETHODIMP
nsClipboardProxy::HasDataMatchingFlavors(const nsTArray<nsCString>& aFlavorList,
                                         int32_t aWhichClipboard,
                                         bool* aHasType) {
  *aHasType = false;

  ContentChild::GetSingleton()->SendClipboardHasType(aFlavorList,
                                                     aWhichClipboard, aHasType);

  return NS_OK;
}

NS_IMETHODIMP
nsClipboardProxy::IsClipboardTypeSupported(int32_t aWhichClipboard,
                                           bool* aIsSupported) {
  switch (aWhichClipboard) {
    case kGlobalClipboard:
      // We always support the global clipboard.
      *aIsSupported = true;
      return NS_OK;
    case kSelectionClipboard:
      *aIsSupported = mClipboardCaps.supportsSelectionClipboard();
      return NS_OK;
    case kFindClipboard:
      *aIsSupported = mClipboardCaps.supportsFindClipboard();
      return NS_OK;
    case kSelectionCache:
      *aIsSupported = mClipboardCaps.supportsSelectionCache();
      return NS_OK;
  }

  *aIsSupported = false;
  return NS_OK;
}

void nsClipboardProxy::SetCapabilities(
    const ClipboardCapabilities& aClipboardCaps) {
  mClipboardCaps = aClipboardCaps;
}
