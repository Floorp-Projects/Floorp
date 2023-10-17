/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/java/ClipboardWrappers.h"
#include "mozilla/java/GeckoAppShellWrappers.h"
#include "nsClipboard.h"
#include "nsISupportsPrimitives.h"
#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsMemory.h"
#include "nsStringStream.h"
#include "nsPrimitiveHelpers.h"

using namespace mozilla;

NS_IMPL_ISUPPORTS_INHERITED0(nsClipboard, nsBaseClipboard)

/* The Android clipboard only supports text and doesn't support mime types
 * so we assume all clipboard data is text/plain for now. Documentation
 * indicates that support for other data types is planned for future
 * releases.
 */

nsClipboard::nsClipboard()
    : nsBaseClipboard(mozilla::dom::ClipboardCapabilities(
          false /* supportsSelectionClipboard */,
          false /* supportsFindClipboard */,
          false /* supportsSelectionCache */)) {
  java::Clipboard::StartTrackingClipboardData(
      java::GeckoAppShell::GetApplicationContext());
}

nsClipboard::~nsClipboard() {
  java::Clipboard::StopTrackingClipboardData(
      java::GeckoAppShell::GetApplicationContext());
}

NS_IMETHODIMP
nsClipboard::SetNativeClipboardData(nsITransferable* aTransferable,
                                    int32_t aWhichClipboard) {
  MOZ_DIAGNOSTIC_ASSERT(aTransferable);
  MOZ_DIAGNOSTIC_ASSERT(
      nsIClipboard::IsClipboardTypeSupported(aWhichClipboard));

  if (!jni::IsAvailable()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsTArray<nsCString> flavors;
  aTransferable->FlavorsTransferableCanImport(flavors);

  nsAutoString html;
  nsAutoString text;

  for (auto& flavorStr : flavors) {
    if (flavorStr.EqualsLiteral(kTextMime)) {
      nsCOMPtr<nsISupports> item;
      nsresult rv =
          aTransferable->GetTransferData(kTextMime, getter_AddRefs(item));
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
      java::Clipboard::SetHTML(java::GeckoAppShell::GetApplicationContext(),
                               text, html)) {
    return NS_OK;
  }
  if (!text.IsEmpty() &&
      java::Clipboard::SetText(java::GeckoAppShell::GetApplicationContext(),
                               text)) {
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsClipboard::GetNativeClipboardData(nsITransferable* aTransferable,
                                    int32_t aWhichClipboard) {
  MOZ_DIAGNOSTIC_ASSERT(aTransferable);
  MOZ_DIAGNOSTIC_ASSERT(
      nsIClipboard::IsClipboardTypeSupported(aWhichClipboard));

  if (!jni::IsAvailable()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsTArray<nsCString> flavors;
  aTransferable->FlavorsTransferableCanImport(flavors);

  for (auto& flavorStr : flavors) {
    if (flavorStr.EqualsLiteral(kTextMime) ||
        flavorStr.EqualsLiteral(kHTMLMime)) {
      auto text = java::Clipboard::GetTextData(
          java::GeckoAppShell::GetApplicationContext(), flavorStr);
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
      continue;
    }

    mozilla::jni::ByteArray::LocalRef bytes;
    nsresult rv = java::Clipboard::GetRawData(flavorStr, &bytes);
    if (NS_FAILED(rv) || !bytes) {
      continue;
    }
    nsCOMPtr<nsIInputStream> byteStream;
    rv = NS_NewByteInputStream(
        getter_AddRefs(byteStream),
        mozilla::Span(
            reinterpret_cast<const char*>(bytes->GetElements().Elements()),
            bytes->Length()),
        NS_ASSIGNMENT_COPY);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      continue;
    }
    rv = aTransferable->SetTransferData(flavorStr.get(), byteStream);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      continue;
    }
  }

  return NS_ERROR_FAILURE;
}

nsresult nsClipboard::EmptyNativeClipboardData(int32_t aWhichClipboard) {
  MOZ_DIAGNOSTIC_ASSERT(
      nsIClipboard::IsClipboardTypeSupported(aWhichClipboard));

  if (!jni::IsAvailable()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  java::Clipboard::Clear(java::GeckoAppShell::GetApplicationContext());

  return NS_OK;
}

mozilla::Result<int32_t, nsresult>
nsClipboard::GetNativeClipboardSequenceNumber(int32_t aWhichClipboard) {
  MOZ_DIAGNOSTIC_ASSERT(
      nsIClipboard::IsClipboardTypeSupported(aWhichClipboard));

  if (!jni::IsAvailable()) {
    return Err(NS_ERROR_NOT_AVAILABLE);
  }

  return java::Clipboard::GetSequenceNumber(
      java::GeckoAppShell::GetApplicationContext());
}

mozilla::Result<bool, nsresult>
nsClipboard::HasNativeClipboardDataMatchingFlavors(
    const nsTArray<nsCString>& aFlavorList, int32_t aWhichClipboard) {
  MOZ_DIAGNOSTIC_ASSERT(
      nsIClipboard::IsClipboardTypeSupported(aWhichClipboard));

  if (!jni::IsAvailable()) {
    return Err(NS_ERROR_NOT_AVAILABLE);
  }

  for (auto& flavor : aFlavorList) {
    if (java::Clipboard::HasData(java::GeckoAppShell::GetApplicationContext(),
                                 NS_ConvertASCIItoUTF16(flavor))) {
      return true;
    }
  }

  return false;
}
