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
                                          int32_t aWhichClipboard) {
  MOZ_DIAGNOSTIC_ASSERT(aTransferable);
  MOZ_DIAGNOSTIC_ASSERT(
      nsIClipboard::IsClipboardTypeSupported(aWhichClipboard));

  // Clear out the clipboard in order to set the new data.
  EmptyNativeClipboardData(aWhichClipboard);

  nsTArray<nsCString> flavors;
  nsresult rv = aTransferable->FlavorsTransferableCanExport(flavors);
  if (NS_FAILED(rv)) {
    return rv;
  }

  auto& clipboard = mClipboards[aWhichClipboard];
  MOZ_ASSERT(clipboard);

  for (const auto& flavor : flavors) {
    if (!flavor.EqualsLiteral(kTextMime) && !flavor.EqualsLiteral(kHTMLMime)) {
      continue;
    }

    nsCOMPtr<nsISupports> data;
    rv = aTransferable->GetTransferData(flavor.get(), getter_AddRefs(data));
    if (NS_FAILED(rv)) {
      continue;
    }

    nsCOMPtr<nsISupportsString> wideString = do_QueryInterface(data);
    if (!wideString) {
      continue;
    }

    nsAutoString utf16string;
    wideString->GetData(utf16string);
    flavor.EqualsLiteral(kTextMime) ? clipboard->SetText(utf16string)
                                    : clipboard->SetHTML(utf16string);
  }

  return NS_OK;
}

NS_IMETHODIMP
HeadlessClipboard::GetNativeClipboardData(nsITransferable* aTransferable,
                                          int32_t aWhichClipboard) {
  MOZ_DIAGNOSTIC_ASSERT(aTransferable);
  MOZ_DIAGNOSTIC_ASSERT(
      nsIClipboard::IsClipboardTypeSupported(aWhichClipboard));

  nsTArray<nsCString> flavors;
  nsresult rv = aTransferable->FlavorsTransferableCanImport(flavors);
  if (NS_FAILED(rv)) {
    return NS_ERROR_FAILURE;
  }

  auto& clipboard = mClipboards[aWhichClipboard];
  MOZ_ASSERT(clipboard);

  for (const auto& flavor : flavors) {
    if (!flavor.EqualsLiteral(kTextMime) && !flavor.EqualsLiteral(kHTMLMime)) {
      continue;
    }

    bool isText = flavor.EqualsLiteral(kTextMime);
    if (!(isText ? clipboard->HasText() : clipboard->HasHTML())) {
      continue;
    }

    nsCOMPtr<nsISupportsString> dataWrapper =
        do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv);
    rv = dataWrapper->SetData(isText ? clipboard->GetText()
                                     : clipboard->GetHTML());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      continue;
    }

    nsCOMPtr<nsISupports> genericDataWrapper = do_QueryInterface(dataWrapper);
    rv = aTransferable->SetTransferData(flavor.get(), genericDataWrapper);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      continue;
    }

    // XXX Other platforms only fill the first available type, too.
    break;
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
    if ((flavor.EqualsLiteral(kTextMime) && clipboard->HasText()) ||
        (flavor.EqualsLiteral(kHTMLMime) && clipboard->HasHTML())) {
      return true;
    }
  }
  return false;
}

}  // namespace mozilla::widget
