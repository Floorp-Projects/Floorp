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
#include "nsNSSComponent.h" // for PIPNSS string bundle calls.
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
#include "nsIChannel.h"
#include "nsIInterfaceRequestor.h"

#include "ssl.h"
#include "cert.h"


static NS_DEFINE_CID(kNetSupportDialogCID, NS_NETSUPPORTDIALOG_CID);
static NS_DEFINE_CID(kNSSComponentCID, NS_NSSCOMPONENT_CID);

char* PK11PasswordPrompt(PK11SlotInfo* slot, PRBool retry, void* arg) {
  nsresult rv = NS_OK;
  PRUnichar *password = nsnull;
  PRBool value = PR_FALSE;
  nsIInterfaceRequestor *ir = NS_STATIC_CAST(nsIInterfaceRequestor*, arg);
  nsCOMPtr<nsIPrompt> proxyPrompt;

  // If no context is provided, no prompt is possible.
  if (!ir)
    return nsnull;

  /* TODO: Retry should generate a different dialog message */
/*
  if (retry)
    return nsnull;
*/

  // The interface requestor object may not be safe, so
  // proxy the call to get the nsIPrompt.

  nsCOMPtr<nsIProxyObjectManager> proxyman(do_GetService(NS_XPCOMPROXY_CONTRACTID));
  if (!proxyman) return nsnull;

  nsCOMPtr<nsIInterfaceRequestor> proxiedCallbacks;
  proxyman->GetProxyForObject(NS_UI_THREAD_EVENTQ,
                              NS_GET_IID(nsIInterfaceRequestor),
                              ir,
                              PROXY_SYNC,
                              getter_AddRefs(proxiedCallbacks));

  // Get the desired interface
  nsCOMPtr<nsIPrompt> prompt(do_GetInterface(proxiedCallbacks));
  if (!prompt) return nsnull;

  // Finally, get a proxy for the nsIPrompt
  proxyman->GetProxyForObject(NS_UI_THREAD_EVENTQ,
	                      NS_GET_IID(nsIPrompt),
                              prompt,
                              PROXY_SYNC,
                              getter_AddRefs(proxyPrompt));

  if (!proxyPrompt) {
    NS_ASSERTION(PR_FALSE, "callbacks does not implement nsIPrompt");
    return nsnull;
  }

  nsString promptString;
  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));

  if (NS_FAILED(rv))
    return nsnull; 

  nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertPassPrompt"),
                                      promptString);

  PRUnichar *uniString = promptString.ToNewUnicode();
  rv = proxyPrompt->PromptPassword(nsnull, uniString,
                                   NS_LITERAL_STRING(" "),
                                   nsIPrompt::SAVE_PASSWORD_NEVER,
                                   &password, &value);
  nsMemory::Free(uniString);
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
      const PRUnichar* formatStrings[1] = { ToNewUnicode(nsLiteralCString(caName)) };
      nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));
      if (NS_FAILED(rv))
        return; 

      rv = nssComponent->PIPBundleFormatStringFromName(NS_LITERAL_STRING("SignedBy"),
                                                     formatStrings, 1,
                                                     getter_Copies(shortDesc));

      nsMemory::Free(NS_CONST_CAST(PRUnichar*, formatStrings[0]));
  
      nsNSSSocketInfo* infoObject = (nsNSSSocketInfo*) fd->higher->secret;
      infoObject->SetSecurityState(secStatus);
      infoObject->SetShortSecurityDescription((const PRUnichar*)shortDesc);

      PR_Free(caName);
      CERT_DestroyName(certName);
      PR_Free(signer);
    }
}

