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
 *  Terry Hayes <thayes@netscape.com>
*/
#include "nsNSSComponent.h" // for PIPNSS string bundle calls.
#include "nsNSSCallbacks.h"
#include "nsNSSCertificate.h"
#include "nsISSLStatus.h"
#include "nsNSSIOLayer.h" // for nsNSSSocketInfo
#include "nsIWebProgressListener.h"
#include "nsIStringBundle.h"
#include "nsXPIDLString.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsReadableUtils.h"
#include "nsIPrompt.h"
#include "nsProxiedService.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"

#include "ssl.h"
#include "cert.h"


static NS_DEFINE_CID(kNSSComponentCID, NS_NSSCOMPONENT_CID);

/* Implementation of nsISSLStatus */
class nsSSLStatus
  : public nsISSLStatus
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISSLSTATUS

  nsSSLStatus();
  virtual ~nsSSLStatus();

  /* public for initilization in this file */
  nsCOMPtr<nsIX509Cert> mServerCert;
  PRUint32 mKeyLength;
  PRUint32 mSecretKeyLength;
  nsXPIDLCString mCipherName;
};

NS_IMETHODIMP
nsSSLStatus::GetServerCert(nsIX509Cert** _result)
{
  NS_ASSERTION(_result, "non-NULL destination required");

  *_result = mServerCert;
  NS_IF_ADDREF(*_result);

  return NS_OK;
}

NS_IMETHODIMP
nsSSLStatus::GetKeyLength(PRUint32* _result)
{
  NS_ASSERTION(_result, "non-NULL destination required");

  *_result = mKeyLength;

  return NS_OK;
}

NS_IMETHODIMP
nsSSLStatus::GetSecretKeyLength(PRUint32* _result)
{
  NS_ASSERTION(_result, "non-NULL destination required");

  *_result = mSecretKeyLength;

  return NS_OK;
}

NS_IMETHODIMP
nsSSLStatus::GetCipherName(char** _result)
{
  NS_ASSERTION(_result, "non-NULL destination required");

  *_result = PL_strdup(mCipherName.get());

  return NS_OK;
}

nsSSLStatus::nsSSLStatus()
: mKeyLength(0), mSecretKeyLength(0)
{
  NS_INIT_ISUPPORTS();
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsSSLStatus, nsISSLStatus);

nsSSLStatus::~nsSSLStatus()
{
}


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
  if (!prompt) {
    NS_ASSERTION(PR_FALSE, "callbacks does not implement nsIPrompt");
    return nsnull;
  }

  // Finally, get a proxy for the nsIPrompt
  proxyman->GetProxyForObject(NS_UI_THREAD_EVENTQ,
	                      NS_GET_IID(nsIPrompt),
                              prompt,
                              PROXY_SYNC,
                              getter_AddRefs(proxyPrompt));


  nsXPIDLString promptString;
  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));

  if (NS_FAILED(rv))
    return nsnull; 

  const PRUnichar* formatStrings[1] = { ToNewUnicode(nsDependentCString(PK11_GetTokenName(slot))) };
  rv = nssComponent->PIPBundleFormatStringFromName(NS_LITERAL_STRING("CertPassPrompt").get(),
                                      formatStrings, 1,
                                      getter_Copies(promptString));
  nsMemory::Free(NS_CONST_CAST(PRUnichar*, formatStrings[0]));

  if (NS_FAILED(rv))
    return nsnull;

  rv = proxyPrompt->PromptPassword(nsnull, promptString.get(),
                                   &password, nsnull, nsnull, &value);
  if (NS_SUCCEEDED(rv) && value) {
    char* str = ToNewCString(nsDependentString(password));
    Recycle(password);
    return str;
  }

  return nsnull;
}

void PR_CALLBACK HandshakeCallback(PRFileDesc* fd, void* client_data) {
  PRInt32 sslStatus;
  char* signer = nsnull;
  char* cipherName = nsnull;
  PRInt32 keyLength;
  nsresult rv;
  PRInt32 encryptBits;

  if (SECSuccess == SSL_SecurityStatus(fd, &sslStatus, &cipherName, &keyLength,
                                       &encryptBits, &signer, nsnull))
    {
      PRInt32 secStatus;
      if (sslStatus == SSL_SECURITY_STATUS_OFF)
        secStatus = nsIWebProgressListener::STATE_IS_BROKEN;
      else if (encryptBits >= 90)
        secStatus = (nsIWebProgressListener::STATE_IS_SECURE |
                     nsIWebProgressListener::STATE_SECURE_HIGH);
      else
        secStatus = (nsIWebProgressListener::STATE_IS_SECURE |
                     nsIWebProgressListener::STATE_SECURE_LOW);

      CERTCertificate *peerCert = SSL_PeerCertificate(fd);
      char* caName = CERT_GetOrgName(&peerCert->issuer);
      CERT_DestroyCertificate(peerCert);
      if (!caName) {
        caName = signer;
      }

      // If the CA name is RSA Data Security, then change the name to the real
      // name of the company i.e. VeriSign, Inc.
      if (nsCRT::strcmp((const char*)caName, "RSA Data Security, Inc.") == 0) {
        // In this case, caName != signer since the logic implies signer
        // would be at minimal "O=RSA Data Security, Inc" because caName
        // is what comes after to O=.  So we're OK just freeing this memory
        // without checking to see if it's equal to signer;
        NS_ASSERTION(caName != signer, "caName was equal to caName when it shouldn't be");
        PR_Free(caName);
        caName = PL_strdup("Verisign, Inc.");
      }

      nsXPIDLString shortDesc;
      const PRUnichar* formatStrings[1] = { ToNewUnicode(nsDependentCString(caName)) };
      nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));
      if (NS_FAILED(rv))
        return; 

      rv = nssComponent->PIPBundleFormatStringFromName(NS_LITERAL_STRING("SignedBy").get(),
                                                     formatStrings, 1,
                                                     getter_Copies(shortDesc));

      nsMemory::Free(NS_CONST_CAST(PRUnichar*, formatStrings[0]));
  
      nsNSSSocketInfo* infoObject = (nsNSSSocketInfo*) fd->higher->secret;
      infoObject->SetSecurityState(secStatus);
      infoObject->SetShortSecurityDescription((const PRUnichar*)shortDesc);

      /* Set the SSL Status information */
      nsCOMPtr<nsSSLStatus> status = new nsSSLStatus();

      CERTCertificate *serverCert = SSL_PeerCertificate(fd);
      if (serverCert) status->mServerCert = new nsNSSCertificate(serverCert);

      status->mKeyLength = keyLength;
      status->mSecretKeyLength = encryptBits;
      status->mCipherName.Adopt(cipherName);

      infoObject->SetSSLStatus(status);

      if (caName != signer)
        PR_Free(caName);
      PR_Free(signer);
    }
}

SECStatus PR_CALLBACK AuthCertificateCallback(void* client_data, PRFileDesc* fd,
                                              PRBool checksig, PRBool isServer) {
  // first the default action
  SECStatus rv = SSL_AuthCertificate(CERT_GetDefaultCertDB(), fd, checksig, isServer);

  // We want to remember the CA certs in the temp db, so that the application can find the
  // complete chain at any time it might need it.
  // But we keep only those CA certs in the temp db, that we didn't already know.
  
  if (SECSuccess == rv) {
    CERTCertificate *serverCert = SSL_PeerCertificate(fd);
    if (serverCert) {
      CERTCertList *certList = CERT_GetCertChainFromCert(serverCert, PR_Now(), certUsageSSLCA);

      nsCOMPtr<nsINSSComponent> nssComponent;
      
      for (CERTCertListNode *node = CERT_LIST_HEAD(certList);
           !CERT_LIST_END(node, certList);
           node = CERT_LIST_NEXT(node)) {

        if (node->cert->slot) {
          // This cert was found on a token, no need to remember it in the temp db.
          continue;
        }

        if (node->cert->isperm) {
          // We don't need to remember certs already stored in perm db.
          continue;
        }
        
        if (node->cert == serverCert) {
          // We don't want to remember the server cert, 
          // the code that cares for displaying page info does this already.
          continue;
        }
        
        // We have found a signer cert that we want to remember.

        if (!nssComponent) {
          // delay getting the service until we really need it
          nsresult rv;
          nssComponent = do_GetService(kNSSComponentCID, &rv);
        }
        
        if (nssComponent) {
          nssComponent->RememberCert(node->cert);
        }
      }

      CERT_DestroyCertList(certList);
      CERT_DestroyCertificate(serverCert);
    }
  }

  return rv;
}
