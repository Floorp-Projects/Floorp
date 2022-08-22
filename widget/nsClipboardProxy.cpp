/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ContentChild.h"
#include "mozilla/Unused.h"
#include "nsArrayUtils.h"
#include "nsClipboardProxy.h"
#include "nsISupportsPrimitives.h"
#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsXULAppAPI.h"
#include "nsContentUtils.h"
#include "PermissionMessageUtils.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_ISUPPORTS(nsClipboardProxy, nsIClipboard, nsIClipboardProxy)

nsClipboardProxy::nsClipboardProxy() : mClipboardCaps(false, false) {}

NS_IMETHODIMP
nsClipboardProxy::SetData(nsITransferable* aTransferable,
                          nsIClipboardOwner* anOwner, int32_t aWhichClipboard) {
  ContentChild* child = ContentChild::GetSingleton();

  IPCDataTransfer ipcDataTransfer;
  nsContentUtils::TransferableToIPCTransferable(aTransferable, &ipcDataTransfer,
                                                false, child, nullptr);

  bool isPrivateData = aTransferable->GetIsPrivateData();
  nsCOMPtr<nsIPrincipal> requestingPrincipal =
      aTransferable->GetRequestingPrincipal();
  nsContentPolicyType contentPolicyType = aTransferable->GetContentPolicyType();
  child->SendSetClipboard(std::move(ipcDataTransfer), isPrivateData,
                          requestingPrincipal, contentPolicyType,
                          aWhichClipboard);

  return NS_OK;
}

NS_IMETHODIMP
nsClipboardProxy::GetData(nsITransferable* aTransferable,
                          int32_t aWhichClipboard) {
  nsTArray<nsCString> types;
  aTransferable->FlavorsTransferableCanImport(types);

  IPCDataTransfer dataTransfer;
  ContentChild::GetSingleton()->SendGetClipboard(types, aWhichClipboard,
                                                 &dataTransfer);
  return nsContentUtils::IPCTransferableToTransferable(
      dataTransfer, false /* aAddDataFlavor */, aTransferable);
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
nsClipboardProxy::SupportsSelectionClipboard(bool* aIsSupported) {
  *aIsSupported = mClipboardCaps.supportsSelectionClipboard();
  return NS_OK;
}

NS_IMETHODIMP
nsClipboardProxy::SupportsFindClipboard(bool* aIsSupported) {
  *aIsSupported = mClipboardCaps.supportsFindClipboard();
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
          [promise, transferable](const IPCDataTransfer& ipcDataTransfer) {
            nsresult rv = nsContentUtils::IPCTransferableToTransferable(
                ipcDataTransfer, false /* aAddDataFlavor */, transferable);
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
