/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ContentChild.h"
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

nsClipboardProxy::nsClipboardProxy()
  : mClipboardCaps(false, false)
{
}

NS_IMETHODIMP
nsClipboardProxy::SetData(nsITransferable *aTransferable,
                          nsIClipboardOwner *anOwner, int32_t aWhichClipboard)
{
  ContentChild* child = ContentChild::GetSingleton();

  IPCDataTransfer ipcDataTransfer;
  nsContentUtils::TransferableToIPCTransferable(aTransferable, &ipcDataTransfer,
                                                child, nullptr);

  bool isPrivateData = false;
  aTransferable->GetIsPrivateData(&isPrivateData);
  child->SendSetClipboard(ipcDataTransfer, isPrivateData, aWhichClipboard);

  return NS_OK;
}

NS_IMETHODIMP
nsClipboardProxy::GetData(nsITransferable *aTransferable, int32_t aWhichClipboard)
{
   nsTArray<nsCString> types;
  
  nsCOMPtr<nsISupportsArray> flavorList;
  aTransferable->FlavorsTransferableCanImport(getter_AddRefs(flavorList));
  if (flavorList) {
    uint32_t flavorCount = 0;
    flavorList->Count(&flavorCount);
    for (uint32_t j = 0; j < flavorCount; ++j) {
      nsCOMPtr<nsISupportsCString> flavor = do_QueryElementAt(flavorList, j);
      if (flavor) {
        nsAutoCString flavorStr;
        flavor->GetData(flavorStr);
        if (flavorStr.Length()) {
          types.AppendElement(flavorStr);
        }
      }
    }
  }

  nsresult rv;
  IPCDataTransfer dataTransfer;
  ContentChild::GetSingleton()->SendGetClipboard(types, aWhichClipboard, &dataTransfer);

  auto& items = dataTransfer.items();
  for (uint32_t j = 0; j < items.Length(); ++j) {
    const IPCDataTransferItem& item = items[j];

    if (item.data().type() == IPCDataTransferData::TnsString) {
      nsCOMPtr<nsISupportsString> dataWrapper =
        do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv);
      NS_ENSURE_SUCCESS(rv, rv);

      nsString data = item.data().get_nsString();
      rv = dataWrapper->SetData(data);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = aTransferable->SetTransferData(item.flavor().get(), dataWrapper,
                                          data.Length() * sizeof(char16_t));
      NS_ENSURE_SUCCESS(rv, rv);
    } else if (item.data().type() == IPCDataTransferData::TnsCString) {
      // If this is an image, convert it into an nsIInputStream.
      nsCString flavor = item.flavor();
      if (flavor.EqualsLiteral(kJPEGImageMime) ||
          flavor.EqualsLiteral(kJPGImageMime) ||
          flavor.EqualsLiteral(kPNGImageMime) ||
          flavor.EqualsLiteral(kGIFImageMime)) {
        nsCOMPtr<nsIInputStream> stream;
        NS_NewCStringInputStream(getter_AddRefs(stream), item.data().get_nsCString());

        rv = aTransferable->SetTransferData(flavor.get(), stream, sizeof(nsISupports*));
        NS_ENSURE_SUCCESS(rv, rv);
      } else if (flavor.EqualsLiteral(kNativeHTMLMime)) {
        nsCOMPtr<nsISupportsCString> dataWrapper =
          do_CreateInstance(NS_SUPPORTS_CSTRING_CONTRACTID, &rv);
        NS_ENSURE_SUCCESS(rv, rv);

        nsCString data = item.data().get_nsCString();
        rv = dataWrapper->SetData(data);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = aTransferable->SetTransferData(item.flavor().get(), dataWrapper,
                                            data.Length());
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsClipboardProxy::EmptyClipboard(int32_t aWhichClipboard)
{
  ContentChild::GetSingleton()->SendEmptyClipboard(aWhichClipboard);
  return NS_OK;
}

NS_IMETHODIMP
nsClipboardProxy::HasDataMatchingFlavors(const char **aFlavorList,
                                         uint32_t aLength, int32_t aWhichClipboard,
                                         bool *aHasType)
{
  *aHasType = false;

  nsTArray<nsCString> types;
  for (uint32_t j = 0; j < aLength; ++j) {
    types.AppendElement(nsDependentCString(aFlavorList[j]));
  }

  ContentChild::GetSingleton()->SendClipboardHasType(types, aWhichClipboard, aHasType);

  return NS_OK;
}

NS_IMETHODIMP
nsClipboardProxy::SupportsSelectionClipboard(bool *aIsSupported)
{
  *aIsSupported = mClipboardCaps.supportsSelectionClipboard();
  return NS_OK;
}


NS_IMETHODIMP
nsClipboardProxy::SupportsFindClipboard(bool *aIsSupported)
{
  *aIsSupported = mClipboardCaps.supportsFindClipboard();
  return NS_OK;
}

void
nsClipboardProxy::SetCapabilities(const ClipboardCapabilities& aClipboardCaps)
{
  mClipboardCaps = aClipboardCaps;
}
