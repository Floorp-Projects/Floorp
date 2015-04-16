/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ContentChild.h"
#include "nsClipboard.h"
#include "nsClipboardProxy.h"
#include "nsISupportsPrimitives.h"
#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsXULAppAPI.h"

using namespace mozilla;
using mozilla::dom::ContentChild;

#define LOG_TAG "Clipboard"
#define LOGI(args...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, ## args)
#define LOGE(args...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, ## args)

NS_IMPL_ISUPPORTS(nsClipboard, nsIClipboard)

nsClipboard::nsClipboard()
{
}

NS_IMETHODIMP
nsClipboard::SetData(nsITransferable *aTransferable,
                     nsIClipboardOwner *anOwner, int32_t aWhichClipboard)
{
  if (aWhichClipboard != kGlobalClipboard) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    // Re-direct to the clipboard proxy.
    nsRefPtr<nsClipboardProxy> clipboardProxy = new nsClipboardProxy();
    return clipboardProxy->SetData(aTransferable, anOwner, aWhichClipboard);
  }

  nsCOMPtr<nsISupports> tmp;
  uint32_t len;
  nsresult rv  = aTransferable->GetTransferData(kUnicodeMime, getter_AddRefs(tmp),
                                                &len);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  nsCOMPtr<nsISupportsString> supportsString = do_QueryInterface(tmp);
  // No support for non-text data
  if (NS_WARN_IF(!supportsString)) {
    LOGE("No support for non-text data. See bug 952456.");
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  nsAutoString buffer;
  supportsString->GetData(buffer);

  mClipboard = buffer;
  return NS_OK;
}

NS_IMETHODIMP
nsClipboard::GetData(nsITransferable *aTransferable, int32_t aWhichClipboard)
{
  if (aWhichClipboard != kGlobalClipboard) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    // Re-direct to the clipboard proxy.
    nsRefPtr<nsClipboardProxy> clipboardProxy = new nsClipboardProxy();
    return clipboardProxy->GetData(aTransferable, aWhichClipboard);
  }

  nsAutoString buffer(mClipboard);

  nsresult rv;
  nsCOMPtr<nsISupportsString> dataWrapper =
    do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = dataWrapper->SetData(buffer);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // If our data flavor has already been added, this will fail. But we don't care
  aTransferable->AddDataFlavor(kUnicodeMime);

  nsCOMPtr<nsISupports> nsisupportsDataWrapper =
    do_QueryInterface(dataWrapper);
  rv = aTransferable->SetTransferData(kUnicodeMime, nsisupportsDataWrapper,
                                      buffer.Length() * sizeof(PRUnichar));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsClipboard::EmptyClipboard(int32_t aWhichClipboard)
{
  if (aWhichClipboard != kGlobalClipboard) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  if (XRE_GetProcessType() == GeckoProcessType_Default) {
    mClipboard.Truncate(0);
  } else {
    ContentChild::GetSingleton()->SendEmptyClipboard(aWhichClipboard);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsClipboard::HasDataMatchingFlavors(const char **aFlavorList,
                                    uint32_t aLength, int32_t aWhichClipboard,
                                    bool *aHasType)
{
  *aHasType = false;
  if (aWhichClipboard != kGlobalClipboard) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  if (XRE_GetProcessType() == GeckoProcessType_Default) {
    *aHasType = !mClipboard.IsEmpty();
  } else {
    nsRefPtr<nsClipboardProxy> clipboardProxy = new nsClipboardProxy();
    return clipboardProxy->HasDataMatchingFlavors(aFlavorList, aLength, aWhichClipboard, aHasType);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsClipboard::SupportsSelectionClipboard(bool *aIsSupported)
{
  *aIsSupported = false;
  return NS_OK;
}

NS_IMETHODIMP
nsClipboard::SupportsFindClipboard(bool* _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  *_retval = false;
  return NS_OK;
}

