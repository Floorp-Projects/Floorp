/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HeadlessClipboard.h"

#include "nsISupportsPrimitives.h"
#include "nsComponentManagerUtils.h"
#include "nsCOMPtr.h"

namespace mozilla::widget {

NS_IMPL_ISUPPORTS_INHERITED0(HeadlessClipboard, nsBaseClipboard)

HeadlessClipboard::HeadlessClipboard()
    : nsBaseClipboard(mozilla::dom::ClipboardCapabilities(
          true /* supportsSelectionClipboard */,
          true /* supportsFindClipboard */,
          true /* supportsSelectionCache */)) {
  for (auto& clipboard : mClipboards) {
    clipboard = MakeUnique<HeadlessClipboardData>();
  }
}

NS_IMETHODIMP
HeadlessClipboard::SetNativeClipboardData(nsITransferable* aTransferable,
                                          nsIClipboardOwner* aOwner,
                                          int32_t aWhichClipboard) {
  MOZ_DIAGNOSTIC_ASSERT(aTransferable);
  MOZ_DIAGNOSTIC_ASSERT(
      nsIClipboard::IsClipboardTypeSupported(aWhichClipboard));

  // Clear out the clipboard in order to set the new data.
  EmptyNativeClipboardData(aWhichClipboard);

  // Only support plain text for now.
  nsCOMPtr<nsISupports> clip;
  nsresult rv = aTransferable->GetTransferData(kTextMime, getter_AddRefs(clip));
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsCOMPtr<nsISupportsString> wideString = do_QueryInterface(clip);
  if (!wideString) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  auto& clipboard = mClipboards[aWhichClipboard];
  MOZ_ASSERT(clipboard);

  nsAutoString utf16string;
  wideString->GetData(utf16string);
  clipboard->SetText(utf16string);

  return NS_OK;
}

NS_IMETHODIMP
HeadlessClipboard::GetNativeClipboardData(nsITransferable* aTransferable,
                                          int32_t aWhichClipboard) {
  MOZ_DIAGNOSTIC_ASSERT(aTransferable);
  MOZ_DIAGNOSTIC_ASSERT(
      nsIClipboard::IsClipboardTypeSupported(aWhichClipboard));

  auto& clipboard = mClipboards[aWhichClipboard];
  MOZ_ASSERT(clipboard);

  if (!clipboard->HasText()) {
    return NS_OK;
  }

  nsresult rv;
  nsCOMPtr<nsISupportsString> dataWrapper =
      do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv);
  rv = dataWrapper->SetData(clipboard->GetText());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  nsCOMPtr<nsISupports> genericDataWrapper = do_QueryInterface(dataWrapper);
  rv = aTransferable->SetTransferData(kTextMime, genericDataWrapper);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult HeadlessClipboard::EmptyNativeClipboardData(int32_t aWhichClipboard) {
  MOZ_DIAGNOSTIC_ASSERT(
      nsIClipboard::IsClipboardTypeSupported(aWhichClipboard));
  auto& clipboard = mClipboards[aWhichClipboard];
  MOZ_ASSERT(clipboard);
  clipboard->Clear();
  return NS_OK;
}

mozilla::Result<int32_t, nsresult>
HeadlessClipboard::GetNativeClipboardSequenceNumber(int32_t aWhichClipboard) {
  MOZ_DIAGNOSTIC_ASSERT(
      nsIClipboard::IsClipboardTypeSupported(aWhichClipboard));
  auto& clipboard = mClipboards[aWhichClipboard];
  MOZ_ASSERT(clipboard);
  return clipboard->GetChangeCount();
  ;
}

mozilla::Result<bool, nsresult>
HeadlessClipboard::HasNativeClipboardDataMatchingFlavors(
    const nsTArray<nsCString>& aFlavorList, int32_t aWhichClipboard) {
  MOZ_DIAGNOSTIC_ASSERT(
      nsIClipboard::IsClipboardTypeSupported(aWhichClipboard));

  auto& clipboard = mClipboards[aWhichClipboard];
  MOZ_ASSERT(clipboard);

  // Retrieve the union of all aHasType in aFlavorList
  for (auto& flavor : aFlavorList) {
    if (flavor.EqualsLiteral(kTextMime) && clipboard->HasText()) {
      return true;
    }
  }
  return false;
}

}  // namespace mozilla::widget
