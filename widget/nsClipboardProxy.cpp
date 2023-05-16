/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsClipboardProxy.h"

#if defined(ACCESSIBILITY) && defined(XP_WIN)
#  include "mozilla/a11y/Compatibility.h"
#endif
#include "mozilla/ClipboardWriteRequestChild.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/net/CookieJarSettings.h"
#include "mozilla/Maybe.h"
#include "mozilla/Unused.h"
#include "nsArrayUtils.h"
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
    int32_t aWhichClipboard, nsIAsyncSetClipboardDataCallback* aCallback,
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

RefPtr<DataFlavorsPromise> nsClipboardProxy::AsyncHasDataMatchingFlavors(
    const nsTArray<nsCString>& aFlavorList, int32_t aWhichClipboard) {
  auto promise = MakeRefPtr<DataFlavorsPromise::Private>(__func__);
  ContentChild::GetSingleton()
      ->SendClipboardHasTypesAsync(aFlavorList, aWhichClipboard)
      ->Then(
          GetMainThreadSerialEventTarget(), __func__,
          /* resolve */
          [promise](nsTArray<nsCString> types) {
            promise->Resolve(std::move(types), __func__);
          },
          /* reject */
          [promise](mozilla::ipc::ResponseRejectReason aReason) {
            promise->Reject(NS_ERROR_FAILURE, __func__);
          });

  return promise.forget();
}

RefPtr<GenericPromise> nsClipboardProxy::AsyncGetData(
    nsITransferable* aTransferable, int32_t aWhichClipboard) {
  if (!aTransferable) {
    return GenericPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  // Get a list of flavors this transferable can import
  nsTArray<nsCString> flavors;
  nsresult rv = aTransferable->FlavorsTransferableCanImport(flavors);
  if (NS_FAILED(rv)) {
    return GenericPromise::CreateAndReject(rv, __func__);
  }

  nsCOMPtr<nsITransferable> transferable(aTransferable);
  auto promise = MakeRefPtr<GenericPromise::Private>(__func__);
  ContentChild::GetSingleton()
      ->SendGetClipboardAsync(flavors, aWhichClipboard)
      ->Then(
          GetMainThreadSerialEventTarget(), __func__,
          /* resolve */
          [promise, transferable](
              const IPCTransferableDataOrError& ipcTransferableDataOrError) {
            if (ipcTransferableDataOrError.type() ==
                IPCTransferableDataOrError::Tnsresult) {
              promise->Reject(ipcTransferableDataOrError.get_nsresult(),
                              __func__);
              return;
            }

            nsresult rv = nsContentUtils::IPCTransferableDataToTransferable(
                ipcTransferableDataOrError.get_IPCTransferableData(),
                false /* aAddDataFlavor */, transferable,
                false /* aFilterUnknownFlavors */);
            if (NS_FAILED(rv)) {
              promise->Reject(rv, __func__);
              return;
            }

            promise->Resolve(true, __func__);
          },
          /* reject */
          [promise](mozilla::ipc::ResponseRejectReason aReason) {
            promise->Reject(NS_ERROR_FAILURE, __func__);
          });

  return promise.forget();
}
