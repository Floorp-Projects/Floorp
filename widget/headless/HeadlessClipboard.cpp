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
  : mClipboard(MakeUnique<HeadlessClipboardData>())
{
}

NS_IMETHODIMP
HeadlessClipboard::SetData(nsITransferable *aTransferable,
                     nsIClipboardOwner *anOwner,
                     int32_t aWhichClipboard)
{
  if (aWhichClipboard != kGlobalClipboard) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // Clear out the clipboard in order to set the new data.
  EmptyClipboard(aWhichClipboard);

  // Only support plain text for now.
  nsCOMPtr<nsISupports> clip;
  uint32_t len;
  nsresult rv = aTransferable->GetTransferData(kUnicodeMime,
                                               getter_AddRefs(clip),
                                               &len);
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
HeadlessClipboard::GetData(nsITransferable *aTransferable,
                     int32_t aWhichClipboard)
{
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
  uint32_t len = mClipboard->GetText().Length() * sizeof(char16_t);
  rv = aTransferable->SetTransferData(kUnicodeMime, genericDataWrapper, len);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

NS_IMETHODIMP
HeadlessClipboard::EmptyClipboard(int32_t aWhichClipboard)
{
  if (aWhichClipboard != kGlobalClipboard) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  mClipboard->Clear();
  return NS_OK;
}

NS_IMETHODIMP
HeadlessClipboard::HasDataMatchingFlavors(const char **aFlavorList,
                                    uint32_t aLength, int32_t aWhichClipboard,
                                    bool *aHasType)
{
  *aHasType = false;
  if (aWhichClipboard != kGlobalClipboard) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  // Retrieve the union of all aHasType in aFlavorList
  for (uint32_t i = 0; i < aLength; ++i) {
    const char *flavor = aFlavorList[i];
    if (!flavor) {
      continue;
    }
    if (!strcmp(flavor, kUnicodeMime) && mClipboard->HasText()) {
      *aHasType = true;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
HeadlessClipboard::SupportsSelectionClipboard(bool *aIsSupported)
{
  *aIsSupported = false;
  return NS_OK;
}

NS_IMETHODIMP
HeadlessClipboard::SupportsFindClipboard(bool* _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  *_retval = false;
  return NS_OK;
}

} // namespace widget
} // namespace mozilla
