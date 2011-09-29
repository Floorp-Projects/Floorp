/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Ehsan Akhgari.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ehsan Akhgari <ehsan.akhgari@gmail.com> (Original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

#define NS_MOZ_DATA_FROM_PRIVATEBROWSING "application/x-moz-private-browsing"

NS_IMPL_ISUPPORTS2(nsClipboardPrivacyHandler, nsIObserver, nsISupportsWeakReference)

nsresult
nsClipboardPrivacyHandler::Init()
{
  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (!observerService)
    return NS_ERROR_FAILURE;
  return observerService->AddObserver(this, NS_PRIVATE_BROWSING_SWITCH_TOPIC,
                                      PR_TRUE);
}

/**
  * Prepare the transferable object to be inserted into the clipboard
  *
  */
nsresult
nsClipboardPrivacyHandler::PrepareDataForClipboard(nsITransferable * aTransferable)
{
  NS_ASSERTION(aTransferable, "clipboard given a null transferable");

  nsresult rv = NS_OK;
  if (InPrivateBrowsing()) {
    nsCOMPtr<nsISupportsPRBool> data = do_CreateInstance(NS_SUPPORTS_PRBOOL_CONTRACTID);
    if (data) {
      rv = data->SetData(PR_TRUE);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = aTransferable->AddDataFlavor(NS_MOZ_DATA_FROM_PRIVATEBROWSING);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = aTransferable->SetTransferData(NS_MOZ_DATA_FROM_PRIVATEBROWSING, data, sizeof(bool));
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return rv;
}

NS_IMETHODIMP
nsClipboardPrivacyHandler::Observe(nsISupports *aSubject, char const *aTopic, PRUnichar const *aData)
{
  if (NS_LITERAL_STRING(NS_PRIVATE_BROWSING_LEAVE).Equals(aData)) {
    nsresult rv;
    nsCOMPtr<nsIClipboard> clipboard =
      do_GetService("@mozilla.org/widget/clipboard;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    const char * flavors[] = { NS_MOZ_DATA_FROM_PRIVATEBROWSING };
    bool haveFlavors;
    rv = clipboard->HasDataMatchingFlavors(flavors,
                                           NS_ARRAY_LENGTH(flavors),
                                           nsIClipboard::kGlobalClipboard,
                                           &haveFlavors);
    if (NS_SUCCEEDED(rv) && haveFlavors) {
#if defined(XP_WIN)
      // Workaround for bug 518412.  On Windows 7 x64, there is a bug
      // in handling clipboard data without any formats between
      // 32-bit/64-bit boundaries, which could lead Explorer to crash.
      // We work around the problem by clearing the clipboard using
      // the usual Win32 API.
      NS_ENSURE_TRUE(SUCCEEDED(::OleSetClipboard(NULL)), NS_ERROR_FAILURE);
#else
      // Empty the native clipboard by copying an empty transferable
      nsCOMPtr<nsITransferable> nullData =
        do_CreateInstance("@mozilla.org/widget/transferable;1", &rv);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = clipboard->SetData(nullData, nsnull,
                              nsIClipboard::kGlobalClipboard);
      NS_ENSURE_SUCCESS(rv, rv);
#endif
    }
  }

  return NS_OK;
}

bool
nsClipboardPrivacyHandler::InPrivateBrowsing()
{
  bool inPrivateBrowsingMode = false;
  if (!mPBService)
    mPBService = do_GetService(NS_PRIVATE_BROWSING_SERVICE_CONTRACTID);
  if (mPBService)
    mPBService->GetPrivateBrowsingEnabled(&inPrivateBrowsingMode);
  return inPrivateBrowsingMode;
}

nsresult
NS_NewClipboardPrivacyHandler(nsClipboardPrivacyHandler ** aHandler)
{
  NS_PRECONDITION(aHandler != nsnull, "null ptr");
  if (!aHandler)
    return NS_ERROR_NULL_POINTER;

  *aHandler = new nsClipboardPrivacyHandler();
  if (!*aHandler)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aHandler);
  nsresult rv = (*aHandler)->Init();
  if (NS_FAILED(rv))
    NS_RELEASE(*aHandler);

  return rv;
}
