/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include "nsClipboardPrivacyHandler.h"
#include "nsITransferable.h"
#include "nsISupportsPrimitives.h"
#include "nsIObserverService.h"
#include "nsIClipboard.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsLiteralString.h"
#include "nsNetCID.h"
#include "nsXPCOM.h"
#include "mozilla/Services.h"

#if defined(XP_WIN)
#include <ole2.h>
#endif

using namespace mozilla;

#define NS_MOZ_DATA_FROM_PRIVATEBROWSING "application/x-moz-private-browsing"

NS_IMPL_ISUPPORTS(nsClipboardPrivacyHandler, nsIObserver, nsISupportsWeakReference)

nsresult
nsClipboardPrivacyHandler::Init()
{
  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (!observerService)
    return NS_ERROR_FAILURE;
  return observerService->AddObserver(this, "last-pb-context-exited", true);
}

/**
  * Prepare the transferable object to be inserted into the clipboard
  *
  */
nsresult
nsClipboardPrivacyHandler::PrepareDataForClipboard(nsITransferable * aTransferable)
{
  NS_ASSERTION(aTransferable, "clipboard given a null transferable");

  bool isPrivateData = false;
  aTransferable->GetIsPrivateData(&isPrivateData);

  nsresult rv = NS_OK;
  if (isPrivateData) {
    nsCOMPtr<nsISupportsPRBool> data = do_CreateInstance(NS_SUPPORTS_PRBOOL_CONTRACTID);
    if (data) {
      rv = data->SetData(true);
      NS_ENSURE_SUCCESS(rv, rv);

      // Ignore the error code of AddDataFlavor, since we might have added
      // this flavor before.  If this call really fails, so will the next
      // one (SetTransferData).
      aTransferable->AddDataFlavor(NS_MOZ_DATA_FROM_PRIVATEBROWSING);

      rv = aTransferable->SetTransferData(NS_MOZ_DATA_FROM_PRIVATEBROWSING, data, sizeof(bool));
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return rv;
}

NS_IMETHODIMP
nsClipboardPrivacyHandler::Observe(nsISupports *aSubject, char const *aTopic, char16_t const *aData)
{
  nsresult rv;
  nsCOMPtr<nsIClipboard> clipboard =
    do_GetService("@mozilla.org/widget/clipboard;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  const char * flavors[] = { NS_MOZ_DATA_FROM_PRIVATEBROWSING };
  bool haveFlavors;
  rv = clipboard->HasDataMatchingFlavors(flavors,
                                         ArrayLength(flavors),
                                         nsIClipboard::kGlobalClipboard,
                                         &haveFlavors);
  if (NS_SUCCEEDED(rv) && haveFlavors) {
#if defined(XP_WIN)
    // Workaround for bug 518412.  On Windows 7 x64, there is a bug
    // in handling clipboard data without any formats between
    // 32-bit/64-bit boundaries, which could lead Explorer to crash.
    // We work around the problem by clearing the clipboard using
    // the usual Win32 API.
    NS_ENSURE_TRUE(SUCCEEDED(::OleSetClipboard(nullptr)), NS_ERROR_FAILURE);
#else
    // Empty the native clipboard by copying an empty transferable
    nsCOMPtr<nsITransferable> nullData =
      do_CreateInstance("@mozilla.org/widget/transferable;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    nullData->Init(nullptr);
    rv = clipboard->SetData(nullData, nullptr,
                            nsIClipboard::kGlobalClipboard);
    NS_ENSURE_SUCCESS(rv, rv);
#endif
  }

  return NS_OK;
}

nsresult
NS_NewClipboardPrivacyHandler(nsClipboardPrivacyHandler ** aHandler)
{
  NS_PRECONDITION(aHandler != nullptr, "null ptr");
  if (!aHandler)
    return NS_ERROR_NULL_POINTER;

  *aHandler = new nsClipboardPrivacyHandler();

  NS_ADDREF(*aHandler);
  nsresult rv = (*aHandler)->Init();
  if (NS_FAILED(rv))
    NS_RELEASE(*aHandler);

  return rv;
}
