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
 *   Hubbie Shaw
 *   Doug Turner <dougt@netscape.com>
 *   Mitch Stoltz <mstoltz@netscape.com>
 *   Brian Ryner <bryner@netscape.com>
 */

#include "nsNSSComponent.h"
#include "nsNSSCallbacks.h"

#include "nsNetUtil.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryService.h"
#include "nsIStreamListener.h"
#include "prlog.h"

#include "nss.h"
#include "pk11func.h"
#include "ssl.h"
#include "sslproto.h"

#ifdef PR_LOGGING
PRLogModuleInfo* gPIPNSSLog = nsnull;
#endif


nsNSSComponent::nsNSSComponent()
{
  NS_INIT_ISUPPORTS();
}

nsNSSComponent::~nsNSSComponent()
{
}

NS_IMETHODIMP
nsNSSComponent::Init()
{
#ifdef PR_LOGGING
  if (!gPIPNSSLog)
    gPIPNSSLog = PR_NewLogModule("pipnss");
#endif

  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("Beginning NSS initialization\n"));
  
  nsXPIDLCString profileStr;
  nsCOMPtr<nsIFile> profilePath;
  
  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                       getter_AddRefs(profilePath));
  if (NS_FAILED(rv)) {
    PR_LOG(gPIPNSSLog, PR_LOG_ERROR, ("Unable to get profile directory\n"));
    return rv;
  }

  rv = profilePath->GetPath(getter_Copies(profileStr));
  if (NS_FAILED(rv)) return rv;
  
  PK11_SetPasswordFunc(PK11PasswordPrompt);
  NSS_InitReadWrite(profileStr);
  NSS_SetDomesticPolicy();
  //  SSL_EnableCipher(SSL_RSA_WITH_NULL_MD5, SSL_ALLOWED);

  // XXX should use prefs
  SSL_OptionSetDefault(SSL_ENABLE_SSL2, PR_TRUE);
  SSL_OptionSetDefault(SSL_ENABLE_SSL3, PR_TRUE);
  SSL_OptionSetDefault(SSL_ENABLE_TLS, PR_TRUE);

  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("NSS Initialization done\n"));

  return rv;
}

/* nsISupports Implementation for the class */
NS_IMPL_THREADSAFE_ISUPPORTS3(nsNSSComponent,
                              nsISecurityManagerComponent,
                              nsIContentHandler,
                              nsISignatureVerifier);


NS_IMETHODIMP
nsNSSComponent::DisplaySecurityAdvisor()
{
  return NS_ERROR_FAILURE; // not implemented
}

class CertDownloader : public nsIStreamListener
{
public:
  CertDownloader() {NS_ASSERTION(PR_FALSE, "don't use this constructor."); }
  CertDownloader(PRInt32 type);
  virtual ~CertDownloader();
  
  NS_DECL_ISUPPORTS
  NS_DECL_NSISTREAMOBSERVER
  NS_DECL_NSISTREAMLISTENER
protected:
  char* mByteData;
  PRInt32 mBufferOffset;
  PRInt32 mContentLength;
  PRInt32 mType;
};


CertDownloader::CertDownloader(PRInt32 type)
  : mByteData(nsnull),
    mType(type)
{
  NS_INIT_ISUPPORTS();
}

CertDownloader::~CertDownloader()
{
  if (mByteData)
    nsMemory::Free(mByteData);
}

NS_IMPL_ISUPPORTS1(CertDownloader, nsIStreamListener);

NS_IMETHODIMP
CertDownloader::OnStartRequest(nsIChannel* channel, nsISupports* context)
{
  channel->GetContentLength(&mContentLength);
  if (mContentLength == -1)
    return NS_ERROR_FAILURE;
  
  mBufferOffset = 0;
  mByteData = (char*) nsMemory::Alloc(mContentLength);
  if (!mByteData)
    return NS_ERROR_OUT_OF_MEMORY;
  
  return NS_OK;
}


NS_IMETHODIMP
CertDownloader::OnDataAvailable(nsIChannel* channel,
                                nsISupports* context,
                                nsIInputStream *aIStream,
                                PRUint32 aSourceOffset,
                                PRUint32 aLength)
{
  if (!mByteData)
    return NS_ERROR_OUT_OF_MEMORY;
  
  PRUint32 amt;
  nsresult err;
  
  do {
    err = aIStream->Read(mByteData+mBufferOffset,
                         mContentLength-mBufferOffset, &amt);
    if (amt == 0) break;
    if (NS_FAILED(err)) return err;
    
    aLength -= amt;
    mBufferOffset += amt;
    
  } while (aLength > 0);
  
  return NS_OK;
}


NS_IMETHODIMP
CertDownloader::OnStopRequest(nsIChannel* channel,
                              nsISupports* context,
                              nsresult aStatus,
                              const PRUnichar* aMsg)
{
  return NS_OK;
}


/* other mime types that we should handle sometime:
   
   application/x-pkcs7-crl
   application/x-pkcs7-mime
   application/pkcs7-signature
   application/pre-encrypted
   
*/


NS_IMETHODIMP
nsNSSComponent::HandleContent(const char * aContentType,
                              const char * aCommand,
                              const char * aWindowTarget,
                              nsISupports* aWindowContext,
                              nsIChannel * aChannel)
{
  // We were called via CI.  We better protect ourselves and addref.
  NS_ADDREF_THIS();
  
  nsresult rv = NS_OK;
  if (!aChannel) return NS_ERROR_NULL_POINTER;
  
  PRUint32 type = (PRUint32) -1;
  
  if (!nsCRT::strcasecmp(aContentType, "application/x-x509-ca-cert"))
    type = 1;  //CA cert
  else if (!nsCRT::strcasecmp(aContentType, "application/x-x509-server-cert"))
    type =  2; //Server cert
  else if (!nsCRT::strcasecmp(aContentType, "application/x-x509-user-cert"))
    type =  3; //User cert
  else if (!nsCRT::strcasecmp(aContentType, "application/x-x509-email-cert"))
    type =  4; //Someone else's email cert
  
  if (type != (PRUint32) -1) {
    // I can't directly open the passed channel cause it fails :-(
    
    nsCOMPtr<nsIURI> uri;
    rv = aChannel->GetURI(getter_AddRefs(uri));
    if (NS_FAILED(rv)) return rv;
    
    nsCOMPtr<nsIChannel> channel;
    rv = NS_OpenURI(getter_AddRefs(channel), uri);
    if (NS_FAILED(rv)) return rv;
    
    return channel->AsyncRead(new CertDownloader(type),
                              NS_STATIC_CAST(nsISecurityManagerComponent*,this));
  }
  
  return NS_ERROR_NOT_IMPLEMENTED;
}


//---------------------------------------------
// Functions Implenenting NSISignatureVerifier
//---------------------------------------------
NS_IMETHODIMP
nsNSSComponent::HashBegin(PRUint32 alg, PRUint32* id)
{
  return NS_OK; /* not sure what the implications of this are */
}

NS_IMETHODIMP
nsNSSComponent::HashUpdate(PRUint32 id, const char* buf, PRUint32 buflen)
{
  return NS_OK; /* not sure what the implications of this are */
}

NS_IMETHODIMP
nsNSSComponent::HashEnd(PRUint32 id, unsigned char** hash, 
                        PRUint32* hashLen, PRUint32 maxLen)
{
  return NS_OK; /* not sure what the implications of this are */
}

NS_IMETHODIMP
nsNSSComponent::CreatePrincipalFromSignature(const char* aRSABuf,
                                             PRUint32 aRSABufLen,
                                             nsIPrincipal** aPrincipal)
{
  PRInt32 errorCode;
  return VerifySignature(aRSABuf, aRSABufLen, nsnull, 0, &errorCode,
                         aPrincipal);
}

NS_IMETHODIMP
nsNSSComponent::GetPassword(char **aRet)
{
  // This functionality is only used in wallet.
  // This interface can go away once we get rid of PSM 1.x.
  *aRet = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSComponent::VerifySignature(const char* aRSABuf, PRUint32 aRSABufLen,
                                const char* aPlaintext, PRUint32 aPlaintextLen,
                                PRInt32* aErrorCode,
                                nsIPrincipal** aPrincipal)
{
  return NS_OK;
}
