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
#include "nsStringStream.h"

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
  child->SendSetClipboard(ipcDataTransfer, isPrivateData,
                          IPC::Principal(requestingPrincipal),
                          contentPolicyType, aWhichClipboard);

  return NS_OK;
}

NS_IMETHODIMP
nsClipboardProxy::GetData(nsITransferable* aTransferable,
                          int32_t aWhichClipboard) {
  nsTArray<nsCString> types;
  aTransferable->FlavorsTransferableCanImport(types);

  nsresult rv;
  IPCDataTransfer dataTransfer;
  ContentChild::GetSingleton()->SendGetClipboard(types, aWhichClipboard,
                                                 &dataTransfer);

  auto& items = dataTransfer.items();
  for (uint32_t j = 0; j < items.Length(); ++j) {
    const IPCDataTransferItem& item = items[j];

    if (item.data().type() == IPCDataTransferData::TnsString) {
      nsCOMPtr<nsISupportsString> dataWrapper =
          do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv);
      NS_ENSURE_SUCCESS(rv, rv);

      const nsString& data = item.data().get_nsString();
      rv = dataWrapper->SetData(data);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = aTransferable->SetTransferData(item.flavor().get(), dataWrapper);
      NS_ENSURE_SUCCESS(rv, rv);
    } else if (item.data().type() == IPCDataTransferData::TShmem) {
      // If this is an image, convert it into an nsIInputStream.
      const nsCString& flavor = item.flavor();
      mozilla::ipc::Shmem data = item.data().get_Shmem();
      if (flavor.EqualsLiteral(kJPEGImageMime) ||
          flavor.EqualsLiteral(kJPGImageMime) ||
          flavor.EqualsLiteral(kPNGImageMime) ||
          flavor.EqualsLiteral(kGIFImageMime)) {
        nsCOMPtr<nsIInputStream> stream;

        NS_NewCStringInputStream(
            getter_AddRefs(stream),
            nsDependentCSubstring(data.get<char>(), data.Size<char>()));

        rv = aTransferable->SetTransferData(flavor.get(), stream);
        NS_ENSURE_SUCCESS(rv, rv);
      } else if (flavor.EqualsLiteral(kNativeHTMLMime) ||
                 flavor.EqualsLiteral(kRTFMime) ||
                 flavor.EqualsLiteral(kCustomTypesMime)) {
        nsCOMPtr<nsISupportsCString> dataWrapper =
            do_CreateInstance(NS_SUPPORTS_CSTRING_CONTRACTID, &rv);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = dataWrapper->SetData(
            nsDependentCSubstring(data.get<char>(), data.Size<char>()));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = aTransferable->SetTransferData(item.flavor().get(), dataWrapper);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      mozilla::Unused << ContentChild::GetSingleton()->DeallocShmem(data);
    }
  }

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
