/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@netscape.com>
*/

#include "nsNSSCallbacks.h"
#include "nsNSSIOLayer.h" // for nsNSSSocketInfo
#include "nsIWebProgressListener.h"
#include "nsIStringBundle.h"
#include "nsXPIDLString.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsReadableUtils.h"

#include "nsINetSupportDialogService.h"
#include "nsIPrompt.h"
#include "nsProxiedService.h"

#include "ssl.h"
#include "cert.h"

static NS_DEFINE_CID(kNetSupportDialogCID, NS_NETSUPPORTDIALOG_CID);
#define PIPNSS_STRBUNDLE_URL "chrome://pipnss/locale/pipnss.properties"

char* PK11PasswordPrompt(PK11SlotInfo* slot, PRBool retry, void* arg) {
  nsresult rv = NS_OK;
  PRUnichar *password = nsnull;
  PRBool value = PR_FALSE;

  if (retry)
    return nsnull;

  NS_WITH_PROXIED_SERVICE(nsIPrompt, dialog, kNetSupportDialogCID,
                          NS_UI_THREAD_EVENTQ, &rv);
  if (NS_FAILED(rv)) return nsnull;

  nsXPIDLString promptStr;
  nsCOMPtr<nsIStringBundleService> bundleService(do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv));
  if (NS_FAILED(rv) || !bundleService) return nsnull;

  nsCOMPtr<nsIStringBundle> bundle;
  bundleService->CreateBundle(PIPNSS_STRBUNDLE_URL, nsnull,
                              getter_AddRefs(bundle));
  if (!bundle) return nsnull;

  bundle->GetStringFromName(NS_LITERAL_STRING("CertPassPrompt"),
                            getter_Copies(promptStr));
  
  rv = dialog->PromptPassword(nsnull, promptStr,
                              NS_LITERAL_STRING(" "),
                              nsIPrompt::SAVE_PASSWORD_NEVER,
                              &password, &value);
  if (NS_SUCCEEDED(rv) && value) {
    char* str = nsString(password).ToNewCString();
    Recycle(password);
    return str;
  }

  return nsnull;
}

void HandshakeCallback(PRFileDesc* fd, void* client_data) {
  PRInt32 sslStatus;
  char* signer = nsnull;
  nsresult rv;

  if (SECSuccess == SSL_SecurityStatus(fd, &sslStatus, nsnull, nsnull,
                                       nsnull, &signer, nsnull))
    {
      PRInt32 secStatus;
      if (sslStatus == SSL_SECURITY_STATUS_OFF)
        secStatus = nsIWebProgressListener::STATE_IS_BROKEN;
      else
        secStatus = nsIWebProgressListener::STATE_IS_SECURE;

      CERTName* certName = CERT_AsciiToName(signer);
      char* caName = CERT_GetOrgName(certName);

      // If the CA name is RSA Data Security, then change the name to the real
      // name of the company i.e. VeriSign, Inc.
      if (nsCRT::strcmp((const char*)caName, "RSA Data Security, Inc.") == 0) {
        PR_Free(caName);
        caName = PL_strdup("Verisign, Inc.");
      }

      nsXPIDLString shortDesc;
      nsCOMPtr<nsIStringBundleService> bundleService(do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv));
      if (NS_SUCCEEDED(rv) && bundleService) {
        nsCOMPtr<nsIStringBundle> bundle;
        bundleService->CreateBundle(PIPNSS_STRBUNDLE_URL, nsnull,
                              getter_AddRefs(bundle));

        const PRUnichar* formatStrings[1] = { ToNewUnicode(nsLiteralCString(caName)) };
        rv = bundle->FormatStringFromName(NS_LITERAL_STRING("SignedBy"),
                                          formatStrings, 1,
                                          getter_Copies(shortDesc));
        nsMemory::Free(NS_CONST_CAST(PRUnichar*, formatStrings[0]));
      }

      nsNSSSocketInfo* infoObject = (nsNSSSocketInfo*) fd->higher->secret;
      infoObject->SetSecurityState(secStatus);
      infoObject->SetShortSecurityDescription((const PRUnichar*)shortDesc);

      PR_Free(caName);
      CERT_DestroyName(certName);
      PR_Free(signer);
    }
}

