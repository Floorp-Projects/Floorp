/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HeadlessClipboard.h"

#include "nsISupportsPrimitives.h"
#include "nsComponentManagerUtils.h"
#include "nsCOMPtr.h"

namespace mozilla {
namespace widget {

NS_IMPL_ISUPPORTS(HeadlessClipboard, nsIClipboard)

HeadlessClipboard::HeadlessClipboard()
    : mClipboard(MakeUnique<HeadlessClipboardData>()) {}

NS_IMETHODIMP
HeadlessClipboard::SetData(nsITransferable* aTransferable,
                           nsIClipboardOwner* anOwner,
                           int32_t aWhichClipboard) {
  if (aWhichClipboard != kGlobalClipboard) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // Clear out the clipboard in order to set the new data.
  EmptyClipboard(aWhichClipboard);

  // Only support plain text for now.
  nsCOMPtr<nsISupports> clip;
  nsresult rv =
      aTransferable->GetTransferData(kUnicodeMime, getter_AddRefs(clip));
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsCOMPtr<nsISupportsString> wideString = do_QueryInterface(clip);
  if (!wideString) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  nsAutoString utf16string;
  wideString->GetData(utf16string);
  mClipboard->SetText(utf16string);

  return NS_OK;
}

NS_IMETHODIMP
HeadlessClipboard::GetData(nsITransferable* aTransferable,
                           int32_t aWhichClipboard) {
  if (aWhichClipboard != kGlobalClipboard) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  nsresult rv;
  nsCOMPtr<nsISupportsString> dataWrapper =
      do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv);
  rv = dataWrapper->SetData(mClipboard->GetText());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  nsCOMPtr<nsISupports> genericDataWrapper = do_QueryInterface(dataWrapper);
  rv = aTransferable->SetTransferData(kUnicodeMime, genericDataWrapper);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

NS_IMETHODIMP
HeadlessClipboard::EmptyClipboard(int32_t aWhichClipboard) {
  if (aWhichClipboard != kGlobalClipboard) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  mClipboard->Clear();
  return NS_OK;
}

NS_IMETHODIMP
HeadlessClipboard::HasDataMatchingFlavors(
    const nsTArray<nsCString>& aFlavorList, int32_t aWhichClipboard,
    bool* aHasType) {
  *aHasType = false;
  if (aWhichClipboard != kGlobalClipboard) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  // Retrieve the union of all aHasType in aFlavorList
  for (auto& flavor : aFlavorList) {
    if (flavor.EqualsLiteral(kUnicodeMime) && mClipboard->HasText()) {
      *aHasType = true;
      break;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
HeadlessClipboard::SupportsSelectionClipboard(bool* aIsSupported) {
  *aIsSupported = false;
  return NS_OK;
}

NS_IMETHODIMP
HeadlessClipboard::SupportsFindClipboard(bool* _retval) {
  NS_ENSURE_ARG_POINTER(_retval);

  *_retval = false;
  return NS_OK;
}

RefPtr<GenericPromise> HeadlessClipboard::AsyncGetData(
    nsITransferable* aTransferable, int32_t aWhichClipboard) {
  nsresult rv = GetData(aTransferable, aWhichClipboard);
  if (NS_FAILED(rv)) {
    return GenericPromise::CreateAndReject(rv, __func__);
  }

  return GenericPromise::CreateAndResolve(true, __func__);
}

RefPtr<DataFlavorsPromise> HeadlessClipboard::AsyncHasDataMatchingFlavors(
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

}  // namespace widget
}  // namespace mozilla
