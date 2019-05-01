/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsClipboard.h"
#include "FennecJNIWrappers.h"
#include "nsISupportsPrimitives.h"
#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsPrimitiveHelpers.h"

using namespace mozilla;

NS_IMPL_ISUPPORTS(nsClipboard, nsIClipboard)

/* The Android clipboard only supports text and doesn't support mime types
 * so we assume all clipboard data is text/unicode for now. Documentation
 * indicates that support for other data types is planned for future
 * releases.
 */

nsClipboard::nsClipboard() {}

NS_IMETHODIMP
nsClipboard::SetData(nsITransferable* aTransferable, nsIClipboardOwner* anOwner,
                     int32_t aWhichClipboard) {
  if (aWhichClipboard != kGlobalClipboard) return NS_ERROR_NOT_IMPLEMENTED;

  if (!jni::IsAvailable()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsTArray<nsCString> flavors;
  aTransferable->FlavorsTransferableCanImport(flavors);

  nsAutoString html;
  nsAutoString text;

  for (auto& flavorStr : flavors) {
    if (flavorStr.EqualsLiteral(kUnicodeMime)) {
      nsCOMPtr<nsISupports> item;
      nsresult rv =
          aTransferable->GetTransferData(kUnicodeMime, getter_AddRefs(item));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        continue;
      }
      nsCOMPtr<nsISupportsString> supportsString = do_QueryInterface(item);
      if (supportsString) {
        supportsString->GetData(text);
      }
    } else if (flavorStr.EqualsLiteral(kHTMLMime)) {
      nsCOMPtr<nsISupports> item;
      nsresult rv =
          aTransferable->GetTransferData(kHTMLMime, getter_AddRefs(item));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        continue;
      }
      nsCOMPtr<nsISupportsString> supportsString = do_QueryInterface(item);
      if (supportsString) {
        supportsString->GetData(html);
      }
    }
  }

  if (!html.IsEmpty() &&
      java::Clipboard::SetHTML(GeckoAppShell::GetApplicationContext(), text,
                               html)) {
    return NS_OK;
  }
  if (!text.IsEmpty() &&
      java::Clipboard::SetText(GeckoAppShell::GetApplicationContext(), text)) {
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsClipboard::GetData(nsITransferable* aTransferable, int32_t aWhichClipboard) {
  if (aWhichClipboard != kGlobalClipboard) return NS_ERROR_NOT_IMPLEMENTED;

  if (!jni::IsAvailable()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsTArray<nsCString> flavors;
  aTransferable->FlavorsTransferableCanImport(flavors);

  for (auto& flavorStr : flavors) {
    if (flavorStr.EqualsLiteral(kUnicodeMime) ||
        flavorStr.EqualsLiteral(kHTMLMime)) {
      auto text =
          Clipboard::GetData(GeckoAppShell::GetApplicationContext(), flavorStr);
      if (!text) {
        continue;
      }
      nsString buffer = text->ToString();
      if (buffer.IsEmpty()) {
        continue;
      }
      nsCOMPtr<nsISupports> wrapper;
      nsPrimitiveHelpers::CreatePrimitiveForData(flavorStr, buffer.get(),
                                                 buffer.Length() * 2,
                                                 getter_AddRefs(wrapper));
      if (wrapper) {
        aTransferable->SetTransferData(flavorStr.get(), wrapper);
        return NS_OK;
      }
    }
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsClipboard::EmptyClipboard(int32_t aWhichClipboard) {
  if (aWhichClipboard != kGlobalClipboard) return NS_ERROR_NOT_IMPLEMENTED;

  if (!jni::IsAvailable()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  java::Clipboard::ClearText(java::GeckoAppShell::GetApplicationContext());

  return NS_OK;
}

NS_IMETHODIMP
nsClipboard::HasDataMatchingFlavors(const char** aFlavorList, uint32_t aLength,
                                    int32_t aWhichClipboard, bool* aHasText) {
  *aHasText = false;
  if (aWhichClipboard != kGlobalClipboard) return NS_ERROR_NOT_IMPLEMENTED;

  if (!jni::IsAvailable()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  for (uint32_t k = 0; k < aLength; k++) {
    bool hasData =
        java::Clipboard::HasData(java::GeckoAppShell::GetApplicationContext(),
                                 NS_ConvertASCIItoUTF16(aFlavorList[k]));
    if (hasData) {
      *aHasText = true;
      return NS_OK;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsClipboard::SupportsSelectionClipboard(bool* aIsSupported) {
  *aIsSupported = false;
  return NS_OK;
}

NS_IMETHODIMP
nsClipboard::SupportsFindClipboard(bool* _retval) {
  *_retval = false;
  return NS_OK;
}
