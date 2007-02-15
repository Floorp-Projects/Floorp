/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian Ryner <bryner@brianryner.com>
 *   Javier Delgadillo <javi@netscape.com>
 *   Kai Engert <kengert@redhat.com>
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

#include "nsNSSComponent.h"
#include "nsNSSIOLayer.h"
#include "nsNSSCallbacks.h"

#include "prlog.h"
#include "prnetdb.h"
#include "nsIPrompt.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIServiceManager.h"
#include "nsIWebProgressListener.h"
#include "nsIChannel.h"
#include "nsIBadCertListener.h"
#include "nsNSSCertificate.h"
#include "nsIProxyObjectManager.h"
#include "nsProxiedService.h"
#include "nsIDateTimeFormat.h"
#include "nsDateTimeFormatCID.h"
#include "nsIClientAuthDialogs.h"

#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsHashSets.h"
#include "nsCRT.h"
#include "nsAutoPtr.h"
#include "nsPrintfCString.h"
#include "nsAutoLock.h"
#include "nsSSLThread.h"
#include "nsNSSShutDown.h"
#include "nsNSSCertHelper.h"
#include "nsNSSCleaner.h"
#include "nsThreadUtils.h"

#include "ssl.h"
#include "secerr.h"
#include "sslerr.h"
#include "secder.h"
#include "secasn1.h"
#include "certdb.h"
#include "cert.h"


//#define DEBUG_SSL_VERBOSE //Enable this define to get minimal 
                            //reports when doing SSL read/write
                            
//#define DUMP_BUFFER  //Enable this define along with
                       //DEBUG_SSL_VERBOSE to dump SSL
                       //read/write buffer to a log.
                       //Uses PR_LOG except on Mac where
                       //we always write out to our own
                       //file.

NSSCleanupAutoPtrClass(CERTCertificate, CERT_DestroyCertificate)

/* SSM_UserCertChoice: enum for cert choice info */
typedef enum {ASK, AUTO} SSM_UserCertChoice;


static SECStatus PR_CALLBACK
nsNSS_SSLGetClientAuthData(void *arg, PRFileDesc *socket,
						   CERTDistNames *caNames,
						   CERTCertificate **pRetCert,
						   SECKEYPrivateKey **pRetKey);
static SECStatus PR_CALLBACK
nsNSS_SSLGetClientAuthData(void *arg, PRFileDesc *socket,
						   CERTDistNames *caNames,
						   CERTCertificate **pRetCert,
						   SECKEYPrivateKey **pRetKey);
#ifdef PR_LOGGING
extern PRLogModuleInfo* gPIPNSSLog;
#endif

#if defined(DEBUG_SSL_VERBOSE) && defined (XP_MAC)

#ifdef PR_LOG
#undef PR_LOG
#endif

static PRFileDesc *gMyLogFile = nsnull;
#define MAC_LOG_FILE "MAC PIPNSS Log File"

void MyLogFunction(const char *fmt, ...)
{
  
  va_list ap;
  va_start(ap,fmt);
  if (gMyLogFile == nsnull)
    gMyLogFile = PR_Open(MAC_LOG_FILE, PR_WRONLY | PR_CREATE_FILE | PR_APPEND,
                         0600);
  if (!gMyLogFile)
      return;
  PR_vfprintf(gMyLogFile, fmt, ap);
  va_end(ap);
}

#define PR_LOG(module,level,args) MyLogFunction args
#endif


nsSSLSocketThreadData::nsSSLSocketThreadData()
: mSSLState(ssl_idle)
, mPRErrorCode(PR_SUCCESS)
, mSSLDataBuffer(nsnull)
, mSSLDataBufferAllocatedSize(0)
, mSSLRequestedTransferAmount(0)
, mSSLRemainingReadResultData(nsnull)
, mSSLResultRemainingBytes(0)
, mReplacedSSLFileDesc(nsnull)
{
}

nsSSLSocketThreadData::~nsSSLSocketThreadData()
{
  NS_ASSERTION(mSSLState != ssl_pending_write
               &&
               mSSLState != ssl_pending_read, 
               "oops??? ssl socket is not idle at the time it is being destroyed");
}

PRBool nsSSLSocketThreadData::ensure_buffer_size(PRInt32 amount)
{
  if (amount > mSSLDataBufferAllocatedSize) {
    if (mSSLDataBuffer) {
      mSSLDataBuffer = (char*)nsMemory::Realloc(mSSLDataBuffer, amount);
    }
    else {
      mSSLDataBuffer = (char*)nsMemory::Alloc(amount);
    }
    
    if (!mSSLDataBuffer)
      return PR_FALSE;

    mSSLDataBufferAllocatedSize = amount;
  }
  
  return PR_TRUE;
}

nsNSSSocketInfo::nsNSSSocketInfo()
  : mFd(nsnull),
    mBlockingState(blocking_state_unknown),
    mSecurityState(nsIWebProgressListener::STATE_IS_INSECURE),
    mForSTARTTLS(PR_FALSE),
    mHandshakePending(PR_TRUE),
    mCanceled(PR_FALSE),
    mHasCleartextPhase(PR_FALSE),
    mHandshakeInProgress(PR_FALSE),
    mAllowTLSIntoleranceTimeout(PR_TRUE),
    mBlockedOnBadCertUI(PR_FALSE),
    mHandshakeStartTime(0),
    mPort(0),
    mCAChain(nsnull)
{
  mThreadData = new nsSSLSocketThreadData;
}

nsNSSSocketInfo::~nsNSSSocketInfo()
{
  delete mThreadData;

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return;

  destructorSafeDestroyNSSReference();
  shutdown(calledFromObject);
}

void nsNSSSocketInfo::virtualDestroyNSSReference()
{
  destructorSafeDestroyNSSReference();
}

void nsNSSSocketInfo::destructorSafeDestroyNSSReference()
{
  if (isAlreadyShutDown())
    return;

  if (mCAChain) {
    CERT_DestroyCertList(mCAChain);
    mCAChain = nsnull;
  }
}

NS_IMPL_THREADSAFE_ISUPPORTS4(nsNSSSocketInfo,
                              nsITransportSecurityInfo,
                              nsISSLSocketControl,
                              nsIInterfaceRequestor,
                              nsISSLStatusProvider)

nsresult
nsNSSSocketInfo::GetHandshakePending(PRBool *aHandshakePending)
{
  *aHandshakePending = mHandshakePending;
  return NS_OK;
}

nsresult
nsNSSSocketInfo::SetHandshakePending(PRBool aHandshakePending)
{
  mHandshakePending = aHandshakePending;
  return NS_OK;
}

nsresult
nsNSSSocketInfo::SetHostName(const char* host)
{
  mHostName.Adopt(host ? nsCRT::strdup(host) : 0);
  return NS_OK;
}

nsresult
nsNSSSocketInfo::GetHostName(char **host)
{
  *host = (mHostName) ? nsCRT::strdup(mHostName) : nsnull;
  return NS_OK;
}

nsresult
nsNSSSocketInfo::SetPort(PRInt32 aPort)
{
  mPort = aPort;
  return NS_OK;
}

nsresult
nsNSSSocketInfo::GetPort(PRInt32 *aPort)
{
  *aPort = mPort;
  return NS_OK;
}

void nsNSSSocketInfo::SetCanceled(PRBool aCanceled)
{
  mCanceled = aCanceled;
}

PRBool nsNSSSocketInfo::GetCanceled()
{
  return mCanceled;
}

void nsNSSSocketInfo::SetHasCleartextPhase(PRBool aHasCleartextPhase)
{
  mHasCleartextPhase = aHasCleartextPhase;
}

PRBool nsNSSSocketInfo::GetHasCleartextPhase()
{
  return mHasCleartextPhase;
}

NS_IMETHODIMP
nsNSSSocketInfo::GetNotificationCallbacks(nsIInterfaceRequestor** aCallbacks)
{
  *aCallbacks = mCallbacks;
  NS_IF_ADDREF(*aCallbacks);
  return NS_OK;
}

NS_IMETHODIMP
nsNSSSocketInfo::SetNotificationCallbacks(nsIInterfaceRequestor* aCallbacks)
{
  if (!aCallbacks) {
    mCallbacks = nsnull;
    return NS_OK;
  }

  nsCOMPtr<nsIInterfaceRequestor> proxiedCallbacks;
  NS_GetProxyForObject(NS_PROXY_TO_MAIN_THREAD,
                       NS_GET_IID(nsIInterfaceRequestor),
                       NS_STATIC_CAST(nsIInterfaceRequestor*,aCallbacks),
                       NS_PROXY_SYNC,
                       getter_AddRefs(proxiedCallbacks));

  mCallbacks = proxiedCallbacks;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSSocketInfo::GetSecurityState(PRUint32* state)
{
  *state = mSecurityState;
  return NS_OK;
}

nsresult
nsNSSSocketInfo::SetSecurityState(PRUint32 aState)
{
  mSecurityState = aState;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSSocketInfo::GetShortSecurityDescription(PRUnichar** aText) {
  if (mShortDesc.IsEmpty())
    *aText = nsnull;
  else {
    *aText = ToNewUnicode(mShortDesc);
    NS_ENSURE_TRUE(*aText, NS_ERROR_OUT_OF_MEMORY);
  }
  return NS_OK;
}

nsresult
nsNSSSocketInfo::SetShortSecurityDescription(const PRUnichar* aText) {
  mShortDesc.Assign(aText);
  return NS_OK;
}

/* void getInterface (in nsIIDRef uuid, [iid_is (uuid), retval] out nsQIResult result); */
NS_IMETHODIMP nsNSSSocketInfo::GetInterface(const nsIID & uuid, void * *result)
{
  nsresult rv;
  if (!mCallbacks) {
    nsCOMPtr<nsIInterfaceRequestor> ir = new PipUIContext();
    if (!ir)
      return NS_ERROR_OUT_OF_MEMORY;

    rv = ir->GetInterface(uuid, result);
  } else {
    // Proxy of the channel callbacks should probably go here, rather
    // than in the password callback code

    rv = mCallbacks->GetInterface(uuid, result);
  }
  return rv;
}

NS_IMETHODIMP
nsNSSSocketInfo::GetForceHandshake(PRBool* forceHandshake)
{
  *forceHandshake = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSSocketInfo::SetForceHandshake(PRBool forceHandshake)
{
  (void)forceHandshake;
  return NS_OK;
}

nsresult
nsNSSSocketInfo::GetForSTARTTLS(PRBool* aForSTARTTLS)
{
  *aForSTARTTLS = mForSTARTTLS;
  return NS_OK;
}

nsresult
nsNSSSocketInfo::SetForSTARTTLS(PRBool aForSTARTTLS)
{
  mForSTARTTLS = aForSTARTTLS;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSSocketInfo::ProxyStartSSL()
{
  return ActivateSSL();
}

NS_IMETHODIMP
nsNSSSocketInfo::StartTLS()
{
  return ActivateSSL();
}

nsresult nsNSSSocketInfo::ActivateSSL()
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  nsresult rv = nsSSLThread::requestActivateSSL(this);
  
  if (NS_FAILED(rv))
    return rv;

  mHandshakePending = PR_TRUE;

  return NS_OK;
}

nsresult nsNSSSocketInfo::GetFileDescPtr(PRFileDesc** aFilePtr)
{
  *aFilePtr = mFd;
  return NS_OK;
}

nsresult nsNSSSocketInfo::SetFileDescPtr(PRFileDesc* aFilePtr)
{
  mFd = aFilePtr;
  return NS_OK;
}

nsresult nsNSSSocketInfo::GetSSLStatus(nsISupports** _result)
{
  NS_ENSURE_ARG_POINTER(_result);

  *_result = mSSLStatus;
  NS_IF_ADDREF(*_result);

  return NS_OK;
}

nsresult nsNSSSocketInfo::RememberCAChain(CERTCertList *aCertList)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  if (mCAChain) {
    CERT_DestroyCertList(mCAChain);
  }
  mCAChain = aCertList;
  return NS_OK;
}

nsresult nsNSSSocketInfo::SetSSLStatus(nsISSLStatus *aSSLStatus)
{
  mSSLStatus = aSSLStatus;

  return NS_OK;
}

void nsNSSSocketInfo::SetHandshakeInProgress(PRBool aIsIn)
{
  mHandshakeInProgress = aIsIn;

  if (mHandshakeInProgress && !mHandshakeStartTime)
  {
    mHandshakeStartTime = PR_IntervalNow();
  }
}

void nsNSSSocketInfo::SetBlockedOnBadCertUI(PRBool aCurrentlyBlockedOnUI)
{
  if (mBlockedOnBadCertUI && !aCurrentlyBlockedOnUI)
  {
    // we were blocked and going back to unblocked,
    // so let's reset the handshake start time, in order to ensure
    // we do not count the amount of time while the UI was shown.
    mHandshakeStartTime = PR_IntervalNow();
  }

  mBlockedOnBadCertUI = aCurrentlyBlockedOnUI;
}

void nsNSSSocketInfo::SetAllowTLSIntoleranceTimeout(PRBool aAllow)
{
  mAllowTLSIntoleranceTimeout = aAllow;
}

#define HANDSHAKE_TIMEOUT_SECONDS 25

PRBool nsNSSSocketInfo::HandshakeTimeout()
{
  if (!mHandshakeInProgress || !mAllowTLSIntoleranceTimeout || mBlockedOnBadCertUI)
    return PR_FALSE;

  return ((PRIntervalTime)(PR_IntervalNow() - mHandshakeStartTime)
          > PR_SecondsToInterval(HANDSHAKE_TIMEOUT_SECONDS));
}

void nsSSLIOLayerHelpers::Cleanup()
{
  if (mTLSIntolerantSites) {
    delete mTLSIntolerantSites;
    mTLSIntolerantSites = nsnull;
  }

  if (mSharedPollableEvent)
    PR_DestroyPollableEvent(mSharedPollableEvent);

  if (mutex)
    PR_DestroyLock(mutex);
}

static nsresult
displayAlert(nsAFlatString &formattedString, nsNSSSocketInfo *infoObject)
{
  // The interface requestor object may not be safe, so proxy the call to get
  // the nsIPrompt.

  nsCOMPtr<nsIInterfaceRequestor> proxiedCallbacks;
  NS_GetProxyForObject(NS_PROXY_TO_MAIN_THREAD,
                       NS_GET_IID(nsIInterfaceRequestor),
                       NS_STATIC_CAST(nsIInterfaceRequestor*, infoObject),
                       NS_PROXY_SYNC,
                       getter_AddRefs(proxiedCallbacks));

  nsCOMPtr<nsIPrompt> prompt (do_GetInterface(proxiedCallbacks));
  if (!prompt)
    return NS_ERROR_NO_INTERFACE;

  // Finally, get a proxy for the nsIPrompt

  nsCOMPtr<nsIPrompt> proxyPrompt;
  NS_GetProxyForObject(NS_PROXY_TO_MAIN_THREAD,
                       NS_GET_IID(nsIPrompt),
                       prompt,
                       NS_PROXY_SYNC,
                       getter_AddRefs(proxyPrompt));

  proxyPrompt->Alert(nsnull, formattedString.get());
  return NS_OK;
}

static nsresult
nsHandleSSLError(nsNSSSocketInfo *socketInfo, PRInt32 err)
{
  if (socketInfo->GetCanceled()) {
    // If the socket has been flagged as canceled,
    // the code who did was responsible for showing
    // an error message (if desired).
    return NS_OK;
  }

  nsresult rv;
  NS_DEFINE_CID(nssComponentCID, NS_NSSCOMPONENT_CID);
  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(nssComponentCID, &rv));
  if (NS_FAILED(rv))
    return rv;

  char buf[80];
  PR_snprintf(buf, 80, "%ld", err);
  NS_ConvertASCIItoUTF16 errorCode(buf);

  nsXPIDLCString hostName;
  socketInfo->GetHostName(getter_Copies(hostName));
  NS_ConvertASCIItoUTF16 hostNameU(hostName);

  NS_DEFINE_CID(StringBundleServiceCID,  NS_STRINGBUNDLESERVICE_CID);
  nsCOMPtr<nsIStringBundleService> service = 
                              do_GetService(StringBundleServiceCID, &rv);
  nsCOMPtr<nsIStringBundle> brandBundle;
  service->CreateBundle("chrome://branding/locale/brand.properties",
                        getter_AddRefs(brandBundle));
  nsXPIDLString brandShortName;
  brandBundle->GetStringFromName(NS_LITERAL_STRING("brandShortName").get(),
                                 getter_Copies(brandShortName));
    
  const PRUnichar *params[2];
  nsAutoString formattedString;

  switch (err) {
  case SSL_ERROR_SSL_DISABLED:
    params[0] = brandShortName.get();
    params[1] = hostNameU.get();
    nssComponent->PIPBundleFormatStringFromName("SSL_Disabled",
                                                params, 2, formattedString);
    break;
  case SSL_ERROR_SSL2_DISABLED:
    params[0] = brandShortName.get();
    params[1] = hostNameU.get();
    nssComponent->PIPBundleFormatStringFromName("SSL2_Disabled",
                                                params, 2, formattedString);
    break;
  case SSL_ERROR_EXPORT_ONLY_SERVER:
  case SSL_ERROR_US_ONLY_SERVER:
  case SSL_ERROR_NO_CYPHER_OVERLAP:
  case SSL_ERROR_UNSUPPORTED_VERSION:
  case SSL_ERROR_UNKNOWN_CIPHER_SUITE:
  case SSL_ERROR_NO_CIPHERS_SUPPORTED:
  case SSL_ERROR_FORTEZZA_PQG:
    params[0] = brandShortName.get();
    params[1] = hostNameU.get();
    nssComponent->PIPBundleFormatStringFromName("SSL_NoMatchingCiphers",
                                                params, 2, formattedString);
                                                  
    break;

  //Clients Cert Rejected
  case SSL_ERROR_REVOKED_CERT_ALERT :
    params[0] = hostNameU.get();
    nssComponent->PIPBundleFormatStringFromName("UsersCertRevoked",
                                                params, 1, formattedString);
    break;

  case SSL_ERROR_EXPIRED_CERT_ALERT:
    params[0] = hostNameU.get();
    nssComponent->PIPBundleFormatStringFromName("UsersCertExpired",
                                                params, 1, formattedString);
    break;

  case SSL_ERROR_BAD_CERT_ALERT:
  case SSL_ERROR_UNSUPPORTED_CERT_ALERT:
  case SSL_ERROR_CERTIFICATE_UNKNOWN_ALERT:
    params[0] = hostNameU.get();
    params[1] = errorCode.get();
    nssComponent->PIPBundleFormatStringFromName("UsersCertRejected",
                                                params, 2, formattedString);
    break;

  //Errors related to Peers Certificate
  case SEC_ERROR_CRL_EXPIRED:
    params[0] = hostNameU.get();
    nssComponent->PIPBundleFormatStringFromName("CRLExpired", 
                                                params, 1, formattedString);
    break;

  case SEC_ERROR_CRL_NOT_YET_VALID:
    params[0] = hostNameU.get();
    nssComponent->PIPBundleFormatStringFromName("CRLNotYetValid", 
                                                params, 1, formattedString);
    break;

  case SEC_ERROR_CRL_INVALID:
    params[0] = hostNameU.get();
    nssComponent->PIPBundleFormatStringFromName("CRLSNotValid", 
                                                params, 1, formattedString);
    break;

  case SEC_ERROR_CRL_BAD_SIGNATURE:
    params[0] = hostNameU.get();
    nssComponent->PIPBundleFormatStringFromName("CRLSigNotValid", 
                                                params, 1, formattedString);
    break;

  case SEC_ERROR_OCSP_MALFORMED_REQUEST:
    params[0] = hostNameU.get();
    nssComponent->PIPBundleFormatStringFromName("OCSPMalformedRequest", 
                                             params, 1, formattedString);
    break;

  case SEC_ERROR_OCSP_REQUEST_NEEDS_SIG:
    params[0] = hostNameU.get();
    nssComponent->PIPBundleFormatStringFromName("OCSPRequestNeedsSig", 
                                             params, 1, formattedString);
    break;

  case SEC_ERROR_OCSP_UNAUTHORIZED_REQUEST:
    params[0] = hostNameU.get();
    nssComponent->PIPBundleFormatStringFromName("OCSPUnauthorizedReq", 
                                             params, 1, formattedString);
    break;

  case SEC_ERROR_OCSP_SERVER_ERROR:
    params[0] = hostNameU.get();
    nssComponent->PIPBundleFormatStringFromName("OCSPServerError", 
                                             params, 1, formattedString);
    break;

  case SEC_ERROR_OCSP_TRY_SERVER_LATER:
    params[0] = hostNameU.get();
    nssComponent->PIPBundleFormatStringFromName("OCSPTryServerLater", 
                                             params, 1, formattedString);
    break;
  
  case SEC_ERROR_OCSP_FUTURE_RESPONSE:
    params[0] = hostNameU.get();
    nssComponent->PIPBundleFormatStringFromName("OCSPFutureResponse", 
                                             params, 1, formattedString);
    break;

  case SEC_ERROR_OCSP_OLD_RESPONSE:
    params[0] = hostNameU.get();
    nssComponent->PIPBundleFormatStringFromName("OCSPOldResponse", 
                                             params, 1, formattedString);
    break;

  case SEC_ERROR_OCSP_UNKNOWN_RESPONSE_TYPE:
  case SEC_ERROR_OCSP_BAD_HTTP_RESPONSE:
  case SEC_ERROR_OCSP_UNKNOWN_RESPONSE_STATUS:
  case SEC_ERROR_OCSP_MALFORMED_RESPONSE:
    params[0] = hostNameU.get();
    params[1] = errorCode.get();
    nssComponent->PIPBundleFormatStringFromName("OCSPCorruptedResponse", 
                                             params, 2, formattedString);
    break;

  case SEC_ERROR_OCSP_UNAUTHORIZED_RESPONSE:
    params[0] = hostNameU.get();
    nssComponent->PIPBundleFormatStringFromName("OCSPUnauthorizedResponse", 
                                             params, 1, formattedString);
    break;
    
  case SEC_ERROR_OCSP_UNKNOWN_CERT:
    params[0] = hostNameU.get();
    nssComponent->PIPBundleFormatStringFromName("OCSPUnknownCert", 
                                             params, 1, formattedString);
    break;


  case SEC_ERROR_OCSP_NO_DEFAULT_RESPONDER:
    params[0] = hostNameU.get();
    nssComponent->PIPBundleFormatStringFromName("OCSPNoDefaultResponder", 
                                             params, 1, formattedString);
    break;
  
  case PR_DIRECTORY_LOOKUP_ERROR:
    params[0] = hostNameU.get();
    nssComponent->PIPBundleFormatStringFromName("OCSPDirLookup", 
                                             params, 1, formattedString);
    break;

  case SEC_ERROR_REVOKED_CERTIFICATE:
    params[0] = hostNameU.get();
    nssComponent->PIPBundleFormatStringFromName("PeersCertRevoked", 
                                             params, 1, formattedString);
    break;

  case SEC_ERROR_UNTRUSTED_CERT:
    params[0] = hostNameU.get();
	  nssComponent->PIPBundleFormatStringFromName("PeersCertUntrusted", 
                                            params, 1, formattedString);
	  break;

  case SSL_ERROR_BAD_CERT_DOMAIN:
    params[0] = hostNameU.get();
	  nssComponent->PIPBundleFormatStringFromName("PeersCertWrongDomain", 
                                            params, 1, formattedString);
	  break;

  case SEC_ERROR_EXPIRED_CERTIFICATE:
    params[0] = hostNameU.get();
	  nssComponent->PIPBundleFormatStringFromName("PeersCertExpired", 
                                            params, 1, formattedString);
	  break;

  case SEC_ERROR_BAD_SIGNATURE:
    params[0] = hostNameU.get();
    nssComponent->PIPBundleFormatStringFromName("PeersCertBadSignature", 
                                                params, 1, formattedString);
    break;

  //A generic error handler for peer cert
  case SEC_ERROR_UNKNOWN_CERT:
  case SEC_ERROR_BAD_KEY:
  case SEC_ERROR_CERT_USAGES_INVALID:
  case SEC_ERROR_INADEQUATE_KEY_USAGE:
  case SEC_ERROR_INADEQUATE_CERT_TYPE:
  case SEC_ERROR_CERT_NOT_IN_NAME_SPACE:
  case SEC_ERROR_CERT_NOT_VALID:
  case SEC_ERROR_CERT_ADDR_MISMATCH:
  case SSL_ERROR_BAD_CERTIFICATE:
  case SSL_ERROR_UNSUPPORTED_CERTIFICATE_TYPE:
  case SSL_ERROR_WRONG_CERTIFICATE:
  case SSL_ERROR_CERT_KEA_MISMATCH:
  case SEC_ERROR_EXTENSION_VALUE_INVALID :
  case SEC_ERROR_EXTENSION_NOT_FOUND:
  case SEC_ERROR_UNKNOWN_CRITICAL_EXTENSION:
    params[0] = hostNameU.get();
    params[1] = errorCode.get();
    nssComponent->PIPBundleFormatStringFromName("PeersCertNoGood", 
                                               params, 2, formattedString);
	  break;

  case SSL_ERROR_BAD_MAC_READ:
    params[0] = brandShortName.get();
    nssComponent->PIPBundleFormatStringFromName("BadMac",
                                                params, 1, formattedString);
    break;

  case SSL_ERROR_BAD_MAC_ALERT:
    params[0] = hostNameU.get();
    nssComponent->PIPBundleFormatStringFromName("BadMac",
                                                params, 1, formattedString);
    break;

  //Connection Reset by peer
  case SSL_ERROR_CLOSE_NOTIFY_ALERT:
  case SSL_ERROR_SOCKET_WRITE_FAILURE:
    params[0] = hostNameU.get();
    params[1] = errorCode.get();
    nssComponent->PIPBundleFormatStringFromName("PeerResetConnection",
                                                params, 2, formattedString);
    break;

  //Connection reset by host
  case SEC_ERROR_USER_CANCELLED:
  case SEC_ERROR_MESSAGE_SEND_ABORTED:
    nssComponent->GetPIPNSSBundleString("HostResetConnection", formattedString);
    break;

  //Bad password
  case SEC_ERROR_BAD_PASSWORD:
  case SEC_ERROR_RETRY_PASSWORD:
    nssComponent->GetPIPNSSBundleString("BadPassword", formattedString);
    break;

  //Bad Database
  case SEC_ERROR_BAD_DATABASE:
  case SEC_ERROR_NO_KEY:
  case SEC_ERROR_CERT_NO_RESPONSE:
    params[0] = errorCode.get();
    nssComponent->PIPBundleFormatStringFromName("BadDatabase",
                                                params, 1, formattedString);
    break;

  //Malformed or unxepected data or message was received from server
  case SSL_ERROR_BAD_SERVER:
  case SSL_ERROR_BAD_BLOCK_PADDING:
  case SSL_ERROR_RX_RECORD_TOO_LONG:
  case SSL_ERROR_TX_RECORD_TOO_LONG:
  case SSL_ERROR_RX_MALFORMED_HELLO_REQUEST:
  case SSL_ERROR_RX_MALFORMED_SERVER_HELLO:
  case SSL_ERROR_RX_MALFORMED_CERTIFICATE:
  case SSL_ERROR_RX_MALFORMED_SERVER_KEY_EXCH:
  case SSL_ERROR_RX_MALFORMED_CERT_REQUEST:
  case SSL_ERROR_RX_MALFORMED_HELLO_DONE:
  case SSL_ERROR_RX_MALFORMED_FINISHED:
  case SSL_ERROR_RX_MALFORMED_CHANGE_CIPHER:
  case SSL_ERROR_RX_MALFORMED_ALERT:
  case SSL_ERROR_RX_MALFORMED_HANDSHAKE:
  case SSL_ERROR_RX_MALFORMED_APPLICATION_DATA:
  case SSL_ERROR_RX_UNEXPECTED_HELLO_REQUEST:
  case SSL_ERROR_RX_UNEXPECTED_SERVER_HELLO:
  case SSL_ERROR_RX_UNEXPECTED_CERTIFICATE:
  case SSL_ERROR_RX_UNEXPECTED_SERVER_KEY_EXCH:
  case SSL_ERROR_RX_UNEXPECTED_CERT_REQUEST:
  case SSL_ERROR_RX_UNEXPECTED_HELLO_DONE:
  case SSL_ERROR_RX_UNEXPECTED_FINISHED:
  case SSL_ERROR_RX_UNEXPECTED_CHANGE_CIPHER:
  case SSL_ERROR_RX_UNEXPECTED_ALERT:
  case SSL_ERROR_RX_UNEXPECTED_HANDSHAKE:
  case SSL_ERROR_RX_UNEXPECTED_APPLICATION_DATA:
  case SSL_ERROR_RX_UNKNOWN_RECORD_TYPE:
  case SSL_ERROR_RX_UNKNOWN_HANDSHAKE:
  case SSL_ERROR_RX_UNKNOWN_ALERT:
    params[0] = hostNameU.get();
    params[1] = errorCode.get();
    nssComponent->PIPBundleFormatStringFromName("BadServer",
                                                params, 2, formattedString);
    break;

  //Alert for Malformed or unexpected data or message recieved by server
  case SSL_ERROR_HANDSHAKE_UNEXPECTED_ALERT:
  case SSL_ERROR_DECOMPRESSION_FAILURE_ALERT:
  case SSL_ERROR_HANDSHAKE_FAILURE_ALERT:
  case SSL_ERROR_ILLEGAL_PARAMETER_ALERT:
    params[0] = hostNameU.get();
    params[1] = errorCode.get();
    nssComponent->PIPBundleFormatStringFromName("BadClient",
                                                params, 2, formattedString);
    break;

  case SEC_ERROR_REUSED_ISSUER_AND_SERIAL:
    nssComponent->GetPIPNSSBundleString("HostReusedIssuerSerial", formattedString);
    break;

  default:
    params[0] = hostNameU.get();
    params[1] = errorCode.get(); 
    nssComponent->PIPBundleFormatStringFromName("SSLGenericError",
                                                params, 2, formattedString);
      
  }

  {
    nsPSMUITracker tracker;
    if (tracker.isUIForbidden()) {
      rv = NS_ERROR_NOT_AVAILABLE;
    }
    else {
      rv = displayAlert(formattedString, socketInfo);
    }
  }
  return rv;
}


static PRStatus PR_CALLBACK
nsSSLIOLayerConnect(PRFileDesc* fd, const PRNetAddr* addr,
                    PRIntervalTime timeout)
{
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("[%p] connecting SSL socket\n", (void*)fd));
  nsNSSShutDownPreventionLock locker;
  if (!fd || !fd->lower)
    return PR_FAILURE;
  
  PRStatus status = PR_SUCCESS;

#if defined(XP_BEOS)
  // Due to BeOS net_server's lack of support for nonblocking sockets,
  // we must execute this entire connect as a blocking operation - bug 70217
 
  PRSocketOptionData sockopt;
  sockopt.option = PR_SockOpt_Nonblocking;
  PR_GetSocketOption(fd, &sockopt);
  PRBool oldBlockVal = sockopt.value.non_blocking;
  sockopt.option = PR_SockOpt_Nonblocking;
  sockopt.value.non_blocking = PR_FALSE;
  PR_SetSocketOption(fd, &sockopt);
#endif
  
  status = fd->lower->methods->connect(fd->lower, addr, 
#if defined(XP_BEOS)  // bug 70217
                                       PR_INTERVAL_NO_TIMEOUT);
#else
                                       timeout);
#endif
  if (status != PR_SUCCESS) {
    PR_LOG(gPIPNSSLog, PR_LOG_ERROR, ("[%p] Lower layer connect error: %d\n",
                                      (void*)fd, PR_GetError()));
#if defined(XP_BEOS)  // bug 70217
    goto loser;
#else
    return status;
#endif
  }
  
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("[%p] Connect\n", (void*)fd));

#if defined(XP_BEOS)  // bug 70217
 loser:
  // We put the Nonblocking bit back to the value it was when 
  // we entered this function.
  NS_ASSERTION(sockopt.option == PR_SockOpt_Nonblocking,
               "sockopt.option was re-set to an unexpected value");
  sockopt.value.non_blocking = oldBlockVal;
  PR_SetSocketOption(fd, &sockopt);
#endif

  return status;
}

// Call this function to report a site that is possibly TLS intolerant.
// This function will return true, if the given socket is currently using TLS.
PRBool
nsSSLIOLayerHelpers::rememberPossibleTLSProblemSite(PRFileDesc* ssl_layer_fd, nsNSSSocketInfo *socketInfo)
{
  PRBool currentlyUsesTLS = PR_FALSE;

  SSL_OptionGet(ssl_layer_fd, SSL_ENABLE_TLS, &currentlyUsesTLS);
  if (currentlyUsesTLS) {
    // Add this site to the list of TLS intolerant sites.
    PRInt32 port;
    nsXPIDLCString host;
    socketInfo->GetPort(&port);
    socketInfo->GetHostName(getter_Copies(host));
    nsCAutoString key;
    key = host + NS_LITERAL_CSTRING(":") + nsPrintfCString("%d", port);

    addIntolerantSite(key);
  }
  
  return currentlyUsesTLS;
}

static PRStatus PR_CALLBACK
nsSSLIOLayerClose(PRFileDesc *fd)
{
  nsNSSShutDownPreventionLock locker;
  if (!fd)
    return PR_FAILURE;

  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("[%p] Shutting down socket\n", (void*)fd));
  
  nsNSSSocketInfo *socketInfo = (nsNSSSocketInfo*)fd->secret;
  NS_ASSERTION(socketInfo,"nsNSSSocketInfo was null for an fd");

  return nsSSLThread::requestClose(socketInfo);
}

PRStatus nsNSSSocketInfo::CloseSocketAndDestroy()
{
  nsNSSShutDownPreventionLock locker;

  nsNSSShutDownList::trackSSLSocketClose();

  PRFileDesc* popped = PR_PopIOLayer(mFd, PR_TOP_IO_LAYER);

  if (GetHandshakeInProgress()) {
    nsSSLIOLayerHelpers::rememberPossibleTLSProblemSite(mFd->lower, this);
  }

  PRStatus status = mFd->methods->close(mFd);
  if (status != PR_SUCCESS) return status;

  popped->identity = PR_INVALID_IO_LAYER;
  NS_RELEASE_THIS();
  popped->dtor(popped);

  return PR_SUCCESS;
}

#if defined(DEBUG_SSL_VERBOSE) && defined(DUMP_BUFFER)
/* Dumps a (potentially binary) buffer using SSM_DEBUG. 
   (We could have used the version in ssltrace.c, but that's
   specifically tailored to SSLTRACE. Sigh. */
#define DUMPBUF_LINESIZE 24
static void
nsDumpBuffer(unsigned char *buf, PRIntn len)
{
  char hexbuf[DUMPBUF_LINESIZE*3+1];
  char chrbuf[DUMPBUF_LINESIZE+1];
  static const char *hex = "0123456789abcdef";
  PRIntn i = 0, l = 0;
  char ch, *c, *h;
  if (len == 0)
    return;
  hexbuf[DUMPBUF_LINESIZE*3] = '\0';
  chrbuf[DUMPBUF_LINESIZE] = '\0';
  (void) memset(hexbuf, 0x20, DUMPBUF_LINESIZE*3);
  (void) memset(chrbuf, 0x20, DUMPBUF_LINESIZE);
  h = hexbuf;
  c = chrbuf;

  while (i < len)
  {
    ch = buf[i];

    if (l == DUMPBUF_LINESIZE)
    {
      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("%s%s\n", hexbuf, chrbuf));
      (void) memset(hexbuf, 0x20, DUMPBUF_LINESIZE*3);
      (void) memset(chrbuf, 0x20, DUMPBUF_LINESIZE);
      h = hexbuf;
      c = chrbuf;
      l = 0;
    }

    /* Convert a character to hex. */
    *h++ = hex[(ch >> 4) & 0xf];
    *h++ = hex[ch & 0xf];
    h++;
        
    /* Put the character (if it's printable) into the character buffer. */
    if ((ch >= 0x20) && (ch <= 0x7e))
      *c++ = ch;
    else
      *c++ = '.';
    i++; l++;
  }
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("%s%s\n", hexbuf, chrbuf));
}

#define DEBUG_DUMP_BUFFER(buf,len) nsDumpBuffer(buf,len)
#else
#define DEBUG_DUMP_BUFFER(buf,len)
#endif

static PRBool
isTLSIntoleranceError(PRInt32 err, PRBool withInitialCleartext)
{
  switch (err)
  {
    case PR_CONNECT_RESET_ERROR:
      if (!withInitialCleartext)
        return PR_TRUE;
      break;
    
    case PR_END_OF_FILE_ERROR:
    case SSL_ERROR_BAD_MAC_ALERT:
    case SSL_ERROR_BAD_MAC_READ:
    case SSL_ERROR_HANDSHAKE_FAILURE_ALERT:
    case SSL_ERROR_HANDSHAKE_UNEXPECTED_ALERT:
    case SSL_ERROR_CLIENT_KEY_EXCHANGE_FAILURE:
    case SSL_ERROR_ILLEGAL_PARAMETER_ALERT:
    case SSL_ERROR_NO_CYPHER_OVERLAP:
    case SSL_ERROR_BAD_SERVER:
    case SSL_ERROR_BAD_BLOCK_PADDING:
    case SSL_ERROR_UNSUPPORTED_VERSION:
    case SSL_ERROR_PROTOCOL_VERSION_ALERT:
    case SSL_ERROR_RX_MALFORMED_FINISHED:
    case SSL_ERROR_BAD_HANDSHAKE_HASH_VALUE:
    case SSL_ERROR_DECODE_ERROR_ALERT:
    case SSL_ERROR_RX_UNKNOWN_ALERT:
      return PR_TRUE;
  }
  
  return PR_FALSE;
}

PRInt32
nsSSLThread::checkHandshake(PRInt32 bytesTransfered, PRFileDesc* ssl_layer_fd, nsNSSSocketInfo *socketInfo)
{
  // This is where we work around all of those SSL servers that don't 
  // conform to the SSL spec and shutdown a connection when we request
  // SSL v3.1 (aka TLS).  The spec says the client says what version
  // of the protocol we're willing to perform, in our case SSL v3.1
  // In its response, the server says which version it wants to perform.
  // Many servers out there only know how to do v3.0.  Next, we're supposed
  // to send back the version of the protocol we requested (ie v3.1).  At
  // this point many servers's implementations are broken and they shut
  // down the connection when they don't see the version they sent back.
  // This is supposed to prevent a man in the middle from forcing one
  // side to dumb down to a lower level of the protocol.  Unfortunately,
  // there are enough broken servers out there that such a gross work-around
  // is necessary.  :(

  // Additional comment added in August 2006:
  // When we begun to use TLS hello extensions, we encountered a new class of
  // broken server, which simply stall for a very long time.
  // We would like to shorten the timeout, but limit this shorter timeout 
  // to the handshake phase.
  // When we arrive here for the first time (for a given socket),
  // we know the connection is established, and the application code
  // tried the first read or write. This triggers the beginning of the
  // SSL handshake phase at the SSL FD level.
  // We'll make a note of the current time,
  // and use this to measure the elapsed time since handshake begin.

  PRBool handleHandshakeResultNow;
  socketInfo->GetHandshakePending(&handleHandshakeResultNow);

  if (0 > bytesTransfered) {
    PRInt32 err = PR_GetError();
    PRBool wantRetry = PR_FALSE;
    
    if (handleHandshakeResultNow) {
      // Let's see if there was an error set by the SSL libraries that we
      // should tell the user about.
      if (PR_WOULD_BLOCK_ERROR == err) {
        socketInfo->SetHandshakeInProgress(PR_TRUE);
        return bytesTransfered;
      }
      
      PRBool withInitialCleartext = socketInfo->GetHasCleartextPhase();

      // When not using a proxy we'll see a connection reset error.
      // When using a proxy, we'll see an end of file error.
      // In addition check for some error codes where it is reasonable
      // to retry without TLS.

      if (isTLSIntoleranceError(err, withInitialCleartext)) {
        wantRetry = nsSSLIOLayerHelpers::rememberPossibleTLSProblemSite(ssl_layer_fd, socketInfo);

        if (wantRetry) {
          // We want to cause the network layer to retry the connection.
          PR_SetError(PR_CONNECT_RESET_ERROR, 0);
        }
      }
    }
    
    if (!wantRetry && (IS_SSL_ERROR(err) || IS_SEC_ERROR(err))) {
      nsHandleSSLError(socketInfo, err);
    }
  }

  // TLS intolerant servers only cause the first transfer to fail, so let's 
  // set the HandshakePending attribute to false so that we don't try the logic
  // above again in a subsequent transfer.
  if (handleHandshakeResultNow) {
    socketInfo->SetHandshakePending(PR_FALSE);
    socketInfo->SetHandshakeInProgress(PR_FALSE);
  }
  
  return bytesTransfered;
}

static PRInt16 PR_CALLBACK
nsSSLIOLayerPoll(PRFileDesc *fd, PRInt16 in_flags, PRInt16 *out_flags)
{
  nsNSSShutDownPreventionLock locker;

  if (!out_flags)
  {
    NS_WARNING("nsSSLIOLayerPoll called with null out_flags");
    return 0;
  }

  *out_flags = 0;

  if (!fd)
  {
    NS_WARNING("nsSSLIOLayerPoll called with null fd");
    return 0;
  }

  nsNSSSocketInfo *socketInfo = (nsNSSSocketInfo*)fd->secret;
  NS_ASSERTION(socketInfo,"nsNSSSocketInfo was null for an fd");

  return nsSSLThread::requestPoll(socketInfo, in_flags, out_flags);
}

PRDescIdentity nsSSLIOLayerHelpers::nsSSLIOLayerIdentity;
PRIOMethods nsSSLIOLayerHelpers::nsSSLIOLayerMethods;
PRLock *nsSSLIOLayerHelpers::mutex = nsnull;
nsCStringHashSet *nsSSLIOLayerHelpers::mTLSIntolerantSites = nsnull;
PRFileDesc *nsSSLIOLayerHelpers::mSharedPollableEvent = nsnull;
nsNSSSocketInfo *nsSSLIOLayerHelpers::mSocketOwningPollableEvent = nsnull;
PRBool nsSSLIOLayerHelpers::mPollableEventCurrentlySet = PR_FALSE;

static PRIntn _PSM_InvalidInt(void)
{
    PR_ASSERT(!"I/O method is invalid");
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return -1;
}

static PRInt64 _PSM_InvalidInt64(void)
{
    PR_ASSERT(!"I/O method is invalid");
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return -1;
}

static PRStatus _PSM_InvalidStatus(void)
{
    PR_ASSERT(!"I/O method is invalid");
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return PR_FAILURE;
}

static PRFileDesc *_PSM_InvalidDesc(void)
{
    PR_ASSERT(!"I/O method is invalid");
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return NULL;
}

static PRStatus PR_CALLBACK PSMGetsockname(PRFileDesc *fd, PRNetAddr *addr)
{
  nsNSSShutDownPreventionLock locker;
  if (!fd || !fd->lower) {
    return PR_FAILURE;
  }

  nsNSSSocketInfo *socketInfo = (nsNSSSocketInfo*)fd->secret;
  NS_ASSERTION(socketInfo,"nsNSSSocketInfo was null for an fd");

  return nsSSLThread::requestGetsockname(socketInfo, addr);
}

static PRStatus PR_CALLBACK PSMGetpeername(PRFileDesc *fd, PRNetAddr *addr)
{
  nsNSSShutDownPreventionLock locker;
  if (!fd || !fd->lower) {
    return PR_FAILURE;
  }

  nsNSSSocketInfo *socketInfo = (nsNSSSocketInfo*)fd->secret;
  NS_ASSERTION(socketInfo,"nsNSSSocketInfo was null for an fd");

  return nsSSLThread::requestGetpeername(socketInfo, addr);
}

static PRStatus PR_CALLBACK PSMGetsocketoption(PRFileDesc *fd, 
                                        PRSocketOptionData *data)
{
  nsNSSShutDownPreventionLock locker;
  if (!fd || !fd->lower) {
    return PR_FAILURE;
  }

  nsNSSSocketInfo *socketInfo = (nsNSSSocketInfo*)fd->secret;
  NS_ASSERTION(socketInfo,"nsNSSSocketInfo was null for an fd");

  return nsSSLThread::requestGetsocketoption(socketInfo, data);
}

static PRStatus PR_CALLBACK PSMSetsocketoption(PRFileDesc *fd, 
                                        const PRSocketOptionData *data)
{
  nsNSSShutDownPreventionLock locker;
  if (!fd || !fd->lower) {
    return PR_FAILURE;
  }

  nsNSSSocketInfo *socketInfo = (nsNSSSocketInfo*)fd->secret;
  NS_ASSERTION(socketInfo,"nsNSSSocketInfo was null for an fd");

  return nsSSLThread::requestSetsocketoption(socketInfo, data);
}

static PRInt32 PR_CALLBACK PSMRecv(PRFileDesc *fd, void *buf, PRInt32 amount,
    PRIntn flags, PRIntervalTime timeout)
{
  nsNSSShutDownPreventionLock locker;
  if (!fd || !fd->lower) {
    PR_SetError(PR_BAD_DESCRIPTOR_ERROR, 0);
    return -1;
  }

  nsNSSSocketInfo *socketInfo = (nsNSSSocketInfo*)fd->secret;
  NS_ASSERTION(socketInfo,"nsNSSSocketInfo was null for an fd");

  if (flags == PR_MSG_PEEK) {
    return nsSSLThread::requestRecvMsgPeek(socketInfo, buf, amount, flags, timeout);
  }

  if (flags != 0) {
    PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
    return -1;
  }

  return nsSSLThread::requestRead(socketInfo, buf, amount, timeout);
}

static PRInt32 PR_CALLBACK PSMSend(PRFileDesc *fd, const void *buf, PRInt32 amount,
    PRIntn flags, PRIntervalTime timeout)
{
  nsNSSShutDownPreventionLock locker;
  if (!fd || !fd->lower) {
    PR_SetError(PR_BAD_DESCRIPTOR_ERROR, 0);
    return -1;
  }

  if (flags != 0) {
    PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
    return -1;
  }

  nsNSSSocketInfo *socketInfo = (nsNSSSocketInfo*)fd->secret;
  NS_ASSERTION(socketInfo,"nsNSSSocketInfo was null for an fd");

  return nsSSLThread::requestWrite(socketInfo, buf, amount, timeout);
}

static PRInt32 PR_CALLBACK
nsSSLIOLayerRead(PRFileDesc* fd, void* buf, PRInt32 amount)
{
  return PSMRecv(fd, buf, amount, 0, PR_INTERVAL_NO_TIMEOUT);
}

static PRInt32 PR_CALLBACK
nsSSLIOLayerWrite(PRFileDesc* fd, const void* buf, PRInt32 amount)
{
  return PSMSend(fd, buf, amount, 0, PR_INTERVAL_NO_TIMEOUT);
}

static PRStatus PR_CALLBACK PSMConnectcontinue(PRFileDesc *fd, PRInt16 out_flags)
{
  nsNSSShutDownPreventionLock locker;
  if (!fd || !fd->lower) {
    return PR_FAILURE;
  }

  nsNSSSocketInfo *socketInfo = (nsNSSSocketInfo*)fd->secret;
  NS_ASSERTION(socketInfo,"nsNSSSocketInfo was null for an fd");

  return nsSSLThread::requestConnectcontinue(socketInfo, out_flags);
}

nsresult nsSSLIOLayerHelpers::Init()
{
  nsSSLIOLayerIdentity = PR_GetUniqueIdentity("NSS layer");
  nsSSLIOLayerMethods  = *PR_GetDefaultIOMethods();

  nsSSLIOLayerMethods.available = (PRAvailableFN)_PSM_InvalidInt;
  nsSSLIOLayerMethods.available64 = (PRAvailable64FN)_PSM_InvalidInt64;
  nsSSLIOLayerMethods.fsync = (PRFsyncFN)_PSM_InvalidStatus;
  nsSSLIOLayerMethods.seek = (PRSeekFN)_PSM_InvalidInt;
  nsSSLIOLayerMethods.seek64 = (PRSeek64FN)_PSM_InvalidInt64;
  nsSSLIOLayerMethods.fileInfo = (PRFileInfoFN)_PSM_InvalidStatus;
  nsSSLIOLayerMethods.fileInfo64 = (PRFileInfo64FN)_PSM_InvalidStatus;
  nsSSLIOLayerMethods.writev = (PRWritevFN)_PSM_InvalidInt;
  nsSSLIOLayerMethods.accept = (PRAcceptFN)_PSM_InvalidDesc;
  nsSSLIOLayerMethods.bind = (PRBindFN)_PSM_InvalidStatus;
  nsSSLIOLayerMethods.listen = (PRListenFN)_PSM_InvalidStatus;
  nsSSLIOLayerMethods.shutdown = (PRShutdownFN)_PSM_InvalidStatus;
  nsSSLIOLayerMethods.recvfrom = (PRRecvfromFN)_PSM_InvalidInt;
  nsSSLIOLayerMethods.sendto = (PRSendtoFN)_PSM_InvalidInt;
  nsSSLIOLayerMethods.acceptread = (PRAcceptreadFN)_PSM_InvalidInt;
  nsSSLIOLayerMethods.transmitfile = (PRTransmitfileFN)_PSM_InvalidInt;
  nsSSLIOLayerMethods.sendfile = (PRSendfileFN)_PSM_InvalidInt;

  nsSSLIOLayerMethods.getsockname = PSMGetsockname;
  nsSSLIOLayerMethods.getpeername = PSMGetpeername;
  nsSSLIOLayerMethods.getsocketoption = PSMGetsocketoption;
  nsSSLIOLayerMethods.setsocketoption = PSMSetsocketoption;
  nsSSLIOLayerMethods.recv = PSMRecv;
  nsSSLIOLayerMethods.send = PSMSend;
  nsSSLIOLayerMethods.connectcontinue = PSMConnectcontinue;

  nsSSLIOLayerMethods.connect = nsSSLIOLayerConnect;
  nsSSLIOLayerMethods.close = nsSSLIOLayerClose;
  nsSSLIOLayerMethods.write = nsSSLIOLayerWrite;
  nsSSLIOLayerMethods.read = nsSSLIOLayerRead;
  nsSSLIOLayerMethods.poll = nsSSLIOLayerPoll;

  mutex = PR_NewLock();
  if (!mutex)
    return NS_ERROR_OUT_OF_MEMORY;

  mSharedPollableEvent = PR_NewPollableEvent();

  // if we can not get a pollable event, we'll have to do busy waiting

  mTLSIntolerantSites = new nsCStringHashSet();
  if (!mTLSIntolerantSites)
    return NS_ERROR_OUT_OF_MEMORY;

  mTLSIntolerantSites->Init(1);

  return NS_OK;
}

void nsSSLIOLayerHelpers::addIntolerantSite(const nsCString &str)
{
  nsAutoLock lock(mutex);
  nsSSLIOLayerHelpers::mTLSIntolerantSites->Put(str);
}

PRBool nsSSLIOLayerHelpers::isKnownAsIntolerantSite(const nsCString &str)
{
  nsAutoLock lock(mutex);
  return mTLSIntolerantSites->Contains(str);
}

nsresult
nsSSLIOLayerNewSocket(PRInt32 family,
                      const char *host,
                      PRInt32 port,
                      const char *proxyHost,
                      PRInt32 proxyPort,
                      PRFileDesc **fd,
                      nsISupports** info,
                      PRBool forSTARTTLS)
{

  PRFileDesc* sock = PR_OpenTCPSocket(family);
  if (!sock) return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv = nsSSLIOLayerAddToSocket(family, host, port, proxyHost, proxyPort,
                                        sock, info, forSTARTTLS);
  if (NS_FAILED(rv)) {
    PR_Close(sock);
    return rv;
  }

  *fd = sock;
  return NS_OK;
}

static nsresult
addCertToDB(CERTCertificate *peerCert, PRInt16 addType)
{
  CERTCertTrust trust;
  SECStatus rv;
  nsresult retVal = NS_ERROR_FAILURE;
  char *nickname;
  
  switch (addType) {
    case nsIBadCertListener::ADD_TRUSTED_PERMANENTLY:
      nickname = nsNSSCertificate::defaultServerNickname(peerCert);
      if (nsnull == nickname)
        break;
      memset((void*)&trust, 0, sizeof(trust));
      rv = CERT_DecodeTrustString(&trust, "P"); 
      if (rv != SECSuccess) {
        return NS_ERROR_FAILURE;
      }
      rv = CERT_AddTempCertToPerm(peerCert, nickname, &trust);
      if (rv == SECSuccess)
        retVal = NS_OK;
      PR_Free(nickname);
      break;
    case nsIBadCertListener::ADD_TRUSTED_FOR_SESSION:
      // XXX We need an API from NSS to do this so 
      //     that we don't have to access the fields 
      //     in the cert directly.
      peerCert->keepSession = PR_TRUE;
      CERTCertTrust *trustPtr;
      if (!peerCert->trust) {
        trustPtr = (CERTCertTrust*)PORT_ArenaZAlloc(peerCert->arena,
                                                    sizeof(CERTCertTrust));
        if (!trustPtr)
          break;

        peerCert->trust = trustPtr;
      } else {
        trustPtr = peerCert->trust;
      }
      rv = CERT_DecodeTrustString(trustPtr, "P");
      if (rv != SECSuccess)
        break;

      retVal = NS_OK;      
      break;
    default:
      PR_ASSERT(!"Invalid value for addType passed to addCertDB");
      break;
  }
  return retVal;
}

static PRBool
nsContinueDespiteCertError(nsNSSSocketInfo  *infoObject,
                           PRFileDesc       *sslSocket,
                           int               error,
                           nsNSSCertificate *nssCert)
{
  PRBool retVal = PR_FALSE;
  nsIBadCertListener *badCertHandler = nsnull;
  PRInt16 addType = nsIBadCertListener::UNINIT_ADD_FLAG;
  nsresult rv;

  if (!nssCert)
    return PR_FALSE;

  // Try to get a nsIBadCertListener implementation from the socket consumer
  // first.  If that fails, fallback to the default UI.
  nsCOMPtr<nsIInterfaceRequestor> callbacks;
  infoObject->GetNotificationCallbacks(getter_AddRefs(callbacks));
  if (callbacks) {
    nsCOMPtr<nsIBadCertListener> handler = do_GetInterface(callbacks);
    if (handler) {
      NS_GetProxyForObject(NS_PROXY_TO_MAIN_THREAD,
                           NS_GET_IID(nsIBadCertListener),
                           handler,
                           NS_PROXY_SYNC,
                           (void**)&badCertHandler);
    }
  }
  if (!badCertHandler) {
    rv = getNSSDialogs((void**)&badCertHandler, 
                       NS_GET_IID(nsIBadCertListener),
                       NS_BADCERTLISTENER_CONTRACTID);
    if (NS_FAILED(rv)) 
      return PR_FALSE;
  }
  nsIInterfaceRequestor *csi = NS_STATIC_CAST(nsIInterfaceRequestor*,
                                                 infoObject);
  nsIX509Cert *callBackCert = NS_STATIC_CAST(nsIX509Cert*, nssCert);
  CERTCertificate *peerCert = nssCert->GetCert();
  NS_ASSERTION(peerCert, "Got nsnull cert back from nsNSSCertificate");
  switch (error) {
  case SEC_ERROR_UNKNOWN_ISSUER:
  case SEC_ERROR_CA_CERT_INVALID:
  case SEC_ERROR_UNTRUSTED_ISSUER:
  /* This is a temporay fix for bug# - We are showing a unknown ca dialog,
     when actually the ca cert has expired/not yet valid. We need to change
     this in future - need to define a proper ui for this situation
  */
  case SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE:
    {
      nsPSMUITracker tracker;
      if (tracker.isUIForbidden()) {
        rv = NS_ERROR_NOT_AVAILABLE;
      }
      else {
        rv = badCertHandler->ConfirmUnknownIssuer(csi, callBackCert, &addType, &retVal);
      }
    }
    break;
  case SSL_ERROR_BAD_CERT_DOMAIN:
    {
      nsXPIDLCString url; url.Adopt(SSL_RevealURL(sslSocket));
      NS_ASSERTION(url.get(), "could not find valid URL in ssl socket");
      {
        nsPSMUITracker tracker;
        if (tracker.isUIForbidden()) {
          rv = NS_ERROR_NOT_AVAILABLE;
        }
        else {
        rv = badCertHandler->ConfirmMismatchDomain(csi, url,
                                            callBackCert, &retVal);
        }
      }
      if (NS_SUCCEEDED(rv) && retVal) {
        rv = CERT_AddOKDomainName(peerCert, url);
      }
    }
    break;
  case SEC_ERROR_EXPIRED_CERTIFICATE:
    {
      nsPSMUITracker tracker;
      if (tracker.isUIForbidden()) {
        rv = NS_ERROR_NOT_AVAILABLE;
      }
      else {
        rv = badCertHandler->ConfirmCertExpired(csi, callBackCert, & retVal);
      }
    }
    if (rv == SECSuccess && retVal) {
      // XXX We need an NSS API for this equivalent functionality.
      //     Having to reach inside the cert is evil.
      peerCert->timeOK = PR_TRUE;
    }
    break;
  case SEC_ERROR_CRL_EXPIRED:
    {
      nsXPIDLCString url; url.Adopt(SSL_RevealURL(sslSocket));
      NS_ASSERTION(url, "could not find valid URL in ssl socket");
      {
        nsPSMUITracker tracker;
        if (tracker.isUIForbidden()) {
          rv = NS_ERROR_NOT_AVAILABLE;
        }
        else {
          rv = badCertHandler->NotifyCrlNextupdate(csi, url, callBackCert);
        }
      }
      retVal = PR_FALSE;
    }
    break;
  default:
    nsHandleSSLError(infoObject,error);
    retVal = PR_FALSE;
    rv = NS_ERROR_FAILURE;
  }
  if (retVal && addType != nsIBadCertListener::UNINIT_ADD_FLAG) {
    addCertToDB(peerCert, addType);
  }
  NS_RELEASE(badCertHandler);
  CERT_DestroyCertificate(peerCert);
  return NS_FAILED(rv) ? PR_FALSE : retVal;
}

static SECStatus
verifyCertAgain(CERTCertificate *cert, 
                PRFileDesc      *sslSocket,
                nsNSSSocketInfo *infoObject)
{
  SECStatus rv;

  // If we get here, the user has accepted the cert so
  // far, so we don't check the signature again.
  rv = CERT_VerifyCertificateNow(CERT_GetDefaultCertDB(), cert,
                          PR_FALSE, certificateUsageSSLServer,
                          (void*)infoObject, NULL);

  if (rv != SECSuccess) {
    return rv;
  }
  
  // Check the name field against the desired hostname.
  char *hostname = SSL_RevealURL(sslSocket); 
  if (hostname && hostname[0]) {
    rv = CERT_VerifyCertName(cert, hostname);
  } else {
    rv = SECFailure;
  }

  if (rv != SECSuccess) {
    PR_SetError(SSL_ERROR_BAD_CERT_DOMAIN, 0);
  }
  PR_FREEIF(hostname);
  return rv;
}
/*
 * Function: SECStatus nsConvertCANamesToStrings()
 * Purpose: creates CA names strings from (CERTDistNames* caNames)
 *
 * Arguments and return values
 * - arena: arena to allocate strings on
 * - caNameStrings: filled with CA names strings on return
 * - caNames: CERTDistNames to extract strings from
 * - return: SECSuccess if successful; error code otherwise
 *
 * Note: copied in its entirety from Nova code
 */
SECStatus nsConvertCANamesToStrings(PRArenaPool* arena, char** caNameStrings,
                                      CERTDistNames* caNames)
{
    SECItem* dername;
    SECStatus rv;
    int headerlen;
    uint32 contentlen;
    SECItem newitem;
    int n;
    char* namestring;

    for (n = 0; n < caNames->nnames; n++) {
        newitem.data = NULL;
        dername = &caNames->names[n];

        rv = DER_Lengths(dername, &headerlen, &contentlen);

        if (rv != SECSuccess) {
            goto loser;
        }

        if (headerlen + contentlen != dername->len) {
            /* This must be from an enterprise 2.x server, which sent
             * incorrectly formatted der without the outer wrapper of
             * type and length.  Fix it up by adding the top level
             * header.
             */
            if (dername->len <= 127) {
                newitem.data = (unsigned char *) PR_Malloc(dername->len + 2);
                if (newitem.data == NULL) {
                    goto loser;
                }
                newitem.data[0] = (unsigned char)0x30;
                newitem.data[1] = (unsigned char)dername->len;
                (void)memcpy(&newitem.data[2], dername->data, dername->len);
            }
            else if (dername->len <= 255) {
                newitem.data = (unsigned char *) PR_Malloc(dername->len + 3);
                if (newitem.data == NULL) {
                    goto loser;
                }
                newitem.data[0] = (unsigned char)0x30;
                newitem.data[1] = (unsigned char)0x81;
                newitem.data[2] = (unsigned char)dername->len;
                (void)memcpy(&newitem.data[3], dername->data, dername->len);
            }
            else {
                /* greater than 256, better be less than 64k */
                newitem.data = (unsigned char *) PR_Malloc(dername->len + 4);
                if (newitem.data == NULL) {
                    goto loser;
                }
                newitem.data[0] = (unsigned char)0x30;
                newitem.data[1] = (unsigned char)0x82;
                newitem.data[2] = (unsigned char)((dername->len >> 8) & 0xff);
                newitem.data[3] = (unsigned char)(dername->len & 0xff);
                memcpy(&newitem.data[4], dername->data, dername->len);
            }
            dername = &newitem;
        }

        namestring = CERT_DerNameToAscii(dername);
        if (namestring == NULL) {
            /* XXX - keep going until we fail to convert the name */
            caNameStrings[n] = "";
        }
        else {
            caNameStrings[n] = PORT_ArenaStrdup(arena, namestring);
            PR_Free(namestring);
            if (caNameStrings[n] == NULL) {
                goto loser;
            }
        }

        if (newitem.data != NULL) {
            PR_Free(newitem.data);
        }
    }

    return SECSuccess;
loser:
    if (newitem.data != NULL) {
        PR_Free(newitem.data);
    }
    return SECFailure;
}

/*
 * structs and ASN1 templates for the limited scope-of-use extension
 *
 * CertificateScopeEntry ::= SEQUENCE {
 *     name GeneralName, -- pattern, as for NameConstraints
 *     portNumber INTEGER OPTIONAL }
 *
 * CertificateScopeOfUse ::= SEQUENCE OF CertificateScopeEntry
 */
/*
 * CERTCertificateScopeEntry: struct for scope entry that can be consumed by
 *                            the code
 * certCertificateScopeOfUse: struct that represents the decoded extension data
 */
typedef struct {
    SECItem derConstraint;
    SECItem derPort;
    CERTGeneralName* constraint; /* decoded constraint */
    PRIntn port; /* decoded port number */
} CERTCertificateScopeEntry;

typedef struct {
    CERTCertificateScopeEntry** entries;
} certCertificateScopeOfUse;

/* corresponding ASN1 templates */
static const SEC_ASN1Template cert_CertificateScopeEntryTemplate[] = {
    { SEC_ASN1_SEQUENCE, 
      0, NULL, sizeof(CERTCertificateScopeEntry) },
    { SEC_ASN1_ANY,
      offsetof(CERTCertificateScopeEntry, derConstraint) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_INTEGER,
      offsetof(CERTCertificateScopeEntry, derPort) },
    { 0 }
};

static const SEC_ASN1Template cert_CertificateScopeOfUseTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, cert_CertificateScopeEntryTemplate }
};

#if 0
/* 
 * decodes the extension data and create CERTCertificateScopeEntry that can
 * be consumed by the code
 */
static
SECStatus cert_DecodeScopeOfUseEntries(PRArenaPool* arena, SECItem* extData,
                                       CERTCertificateScopeEntry*** entries,
                                       int* numEntries)
{
    certCertificateScopeOfUse* scope = NULL;
    SECStatus rv = SECSuccess;
    int i;

    *entries = NULL; /* in case of failure */
    *numEntries = 0; /* ditto */

    scope = (certCertificateScopeOfUse*)
        PORT_ArenaZAlloc(arena, sizeof(certCertificateScopeOfUse));
    if (scope == NULL) {
        goto loser;
    }

    rv = SEC_ASN1DecodeItem(arena, (void*)scope, 
                            cert_CertificateScopeOfUseTemplate, extData);
    if (rv != SECSuccess) {
        goto loser;
    }

    *entries = scope->entries;
    PR_ASSERT(*entries != NULL);

    /* first, let's count 'em. */
    for (i = 0; (*entries)[i] != NULL; i++) ;
    *numEntries = i;

    /* convert certCertificateScopeEntry sequence into what we can readily
     * use
     */
    for (i = 0; i < *numEntries; i++) {
        (*entries)[i]->constraint = 
            CERT_DecodeGeneralName(arena, &((*entries)[i]->derConstraint), 
                                   NULL);
        if ((*entries)[i]->derPort.data != NULL) {
            (*entries)[i]->port = 
                (int)DER_GetInteger(&((*entries)[i]->derPort));
        }
        else {
            (*entries)[i]->port = 0;
        }
    }

    goto done;
loser:
    if (rv == SECSuccess) {
        rv = SECFailure;
    }
done:
    return rv;
}

static SECStatus cert_DecodeCertIPAddress(SECItem* genname, 
                                          PRUint32* constraint, PRUint32* mask)
{
    /* in case of failure */
    *constraint = 0;
    *mask = 0;

    PR_ASSERT(genname->data != NULL);
    if (genname->data == NULL) {
        return SECFailure;
    }
    if (genname->len != 8) {
        /* the length must be 4 byte IP address with 4 byte subnet mask */
        return SECFailure;
    }

    /* get them in the right order */
    *constraint = PR_ntohl((PRUint32)(*genname->data));
    *mask = PR_ntohl((PRUint32)(*(genname->data + 4)));

    return SECSuccess;
}

static char* _str_to_lower(char* string)
{
#ifdef XP_WIN
    return _strlwr(string);
#else
    int i;
    for (i = 0; string[i] != '\0'; i++) {
        string[i] = tolower(string[i]);
    }
    return string;
#endif
}

/*
 * Sees if the client certificate has a restriction in presenting the cert
 * to the host: returns PR_TRUE if there is no restriction or if the hostname
 * (and the port) satisfies the restriction, or PR_FALSE if the hostname (and
 * the port) does not satisfy the restriction
 */
static PRBool CERT_MatchesScopeOfUse(CERTCertificate* cert, char* hostname,
                                     char* hostIP, PRIntn port)
{
    PRBool rv = PR_TRUE; /* whether the cert can be presented */
    SECStatus srv;
    SECItem extData;
    PRArenaPool* arena = NULL;
    CERTCertificateScopeEntry** entries = NULL;
    /* arrays of decoded scope entries */
    int numEntries = 0;
    int i;
    char* hostLower = NULL;
    PRUint32 hostIPAddr = 0;

    PR_ASSERT((cert != NULL) && (hostname != NULL) && (hostIP != NULL));

    /* find cert extension */
    srv = CERT_FindCertExtension(cert, SEC_OID_NS_CERT_EXT_SCOPE_OF_USE,
                                 &extData);
    if (srv != SECSuccess) {
        /* most of the time, this means the extension was not found: also,
         * since this is not a critical extension (as of now) we may simply
         * return PR_TRUE
         */
        goto done;
    }

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) {
        goto done;
    }

    /* decode the scope of use entries into pairs of GeneralNames and
     * an optional port numbers
     */
    srv = cert_DecodeScopeOfUseEntries(arena, &extData, &entries, &numEntries);
    if (srv != SECSuccess) {
        /* XXX What should we do when we failed to decode the extension?  This
         *     may mean either the extension was malformed or some (unlikely)
         *     fatal error on our part: my argument is that if the extension 
         *     was malformed the extension "disqualifies" as a valid 
         *     constraint and we may present the cert
         */
        goto done;
    }

    /* loop over these structures */
    for (i = 0; i < numEntries; i++) {
        /* determine whether the GeneralName is a DNS pattern, an IP address 
         * constraint, or else
         */
        CERTGeneralName* genname = entries[i]->constraint;

        /* if constraint is NULL, don't bother looking */
        if (genname == NULL) {
            /* this is not a failure: just continue */
            continue;
        }

        switch (genname->type) {
        case certDNSName: {
            /* we have a DNS name constraint; we should use only the host name
             * information
             */
            char* pattern = NULL;
            char* substring = NULL;

            /* null-terminate the string */
            genname->name.other.data[genname->name.other.len] = '\0';
            pattern = _str_to_lower((char*)genname->name.other.data);

            if (hostLower == NULL) {
                /* so that it's done only if necessary and only once */
                hostLower = _str_to_lower(PL_strdup(hostname));
            }

            /* the hostname satisfies the constraint */
            if (((substring = strstr(hostLower, pattern)) != NULL) &&
                /* the hostname contains the pattern */
                (strlen(substring) == strlen(pattern)) &&
                /* the hostname ends with the pattern */
                ((substring == hostLower) || (*(substring-1) == '.'))) {
                /* the hostname either is identical to the pattern or
                 * belongs to a subdomain
                 */
                rv = PR_TRUE;
            }
            else {
                rv = PR_FALSE;
            }
            /* clean up strings if necessary */
            break;
        }
        case certIPAddress: {
            PRUint32 constraint;
            PRUint32 mask;
            PRNetAddr addr;
            
            if (hostIPAddr == 0) {
                /* so that it's done only if necessary and only once */
                PR_StringToNetAddr(hostIP, &addr);
                hostIPAddr = addr.inet.ip;
            }

            if (cert_DecodeCertIPAddress(&(genname->name.other), &constraint, 
                                         &mask) != SECSuccess) {
                continue;
            }
            if ((hostIPAddr & mask) == (constraint & mask)) {
                rv = PR_TRUE;
            }
            else {
                rv = PR_FALSE;
            }
            break;
        }
        default:
            /* ill-formed entry: abort */
            continue; /* go to the next entry */
        }

        if (!rv) {
            /* we do not need to check the port: go to the next entry */
            continue;
        }

        /* finally, check the optional port number */
        if ((entries[i]->port != 0) && (port != entries[i]->port)) {
            /* port number does not match */
            rv = PR_FALSE;
            continue;
        }

        /* we have a match */
        PR_ASSERT(rv);
        break;
    }
done:
    /* clean up entries */
    if (arena != NULL) {
        PORT_FreeArena(arena, PR_FALSE);
    }
    if (hostLower != NULL) {
        PR_Free(hostLower);
    }
    return rv;
}
#endif

/*
 * Function: SSMStatus SSM_SetUserCertChoice()

 * Purpose: sets certChoice by reading the preference
 *
 * Arguments and return values
 * - conn: SSMSSLDataConnection
 * - returns: SSM_SUCCESS if successful; SSM_FAILURE otherwise
 *
 * Note: If done properly, this function will read the identifier strings
 *		 for ASK and AUTO modes, read the selected strings from the
 *		 preference, compare the strings, and determine in which mode it is
 *		 in.
 *       We currently use ASK mode for UI apps and AUTO mode for UI-less
 *       apps without really asking for preferences.
 */
nsresult nsGetUserCertChoice(SSM_UserCertChoice* certChoice)
{
	char *mode=NULL;
	nsresult ret;

	NS_ENSURE_ARG_POINTER(certChoice);

	nsCOMPtr<nsIPrefBranch> pref = do_GetService(NS_PREFSERVICE_CONTRACTID);

	ret = pref->GetCharPref("security.default_personal_cert", &mode);
	if (NS_FAILED(ret)) {
		goto loser;
	}

    if (PL_strcmp(mode, "Select Automatically") == 0) {
		*certChoice = AUTO;
	}
    else if (PL_strcmp(mode, "Ask Every Time") == 0) {
        *certChoice = ASK;
    }
    else {
      // Most likely we see a nickname from a migrated cert.
      // We do not currently support that, ask the user which cert to use.
		  *certChoice = ASK;
	}

loser:
	if (mode) {
		nsMemory::Free(mode);
	}
	return ret;
}

static PRBool hasExplicitKeyUsageNonRepudiation(CERTCertificate *cert)
{
  /* There is no extension, v1 or v2 certificate */
  if (!cert->extensions)
    return PR_FALSE;

  SECStatus srv;
  SECItem keyUsageItem;
  keyUsageItem.data = NULL;

  srv = CERT_FindKeyUsageExtension(cert, &keyUsageItem);
  if (srv == SECFailure)
    return PR_FALSE;

  unsigned char keyUsage = keyUsageItem.data[0];
  PORT_Free (keyUsageItem.data);

  return (keyUsage & KU_NON_REPUDIATION);
}

/*
 * Function: SECStatus SSM_SSLGetClientAuthData()
 * Purpose: this callback function is used to pull client certificate
 *			information upon server request
 *
 * Arguments and return values
 * - arg: SSL data connection
 * - socket: SSL socket we're dealing with
 * - caNames: list of CA names
 * - pRetCert: returns a pointer to a pointer to a valid certificate if
 *			   successful; otherwise NULL
 * - pRetKey: returns a pointer to a pointer to the corresponding key if
 *			  successful; otherwise NULL
 * - returns: SECSuccess if successful; error code otherwise
 */
SECStatus nsNSS_SSLGetClientAuthData(void* arg, PRFileDesc* socket,
								   CERTDistNames* caNames,
								   CERTCertificate** pRetCert,
								   SECKEYPrivateKey** pRetKey)
{
  nsNSSShutDownPreventionLock locker;
  void* wincx = NULL;
  SECStatus ret = SECFailure;
  nsresult rv;
  nsNSSSocketInfo* info = NULL;
  PRArenaPool* arena = NULL;
  char** caNameStrings;
  CERTCertificate* cert = NULL;
  CERTCertificate* serverCert = NULL;
  SECKEYPrivateKey* privKey = NULL;
  CERTCertList* certList = NULL;
  CERTCertListNode* node;
  CERTCertNicknames* nicknames = NULL;
  char* extracted = NULL;
  PRIntn keyError = 0; /* used for private key retrieval error */
  SSM_UserCertChoice certChoice;
  PRInt32 NumberOfCerts = 0;
	
  /* do some argument checking */
  if (socket == NULL || caNames == NULL || pRetCert == NULL ||
      pRetKey == NULL) {
    PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
    return SECFailure;
  }

  /* get PKCS11 pin argument */
  wincx = SSL_RevealPinArg(socket);
  if (wincx == NULL) {
    return SECFailure;
  }

  /* get the socket info */
  info = (nsNSSSocketInfo*)socket->higher->secret;

  /* create caNameStrings */
  arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
  if (arena == NULL) {
    goto loser;
  }

  caNameStrings = (char**)PORT_ArenaAlloc(arena, 
                                          sizeof(char*)*(caNames->nnames));
  if (caNameStrings == NULL) {
    goto loser;
  }


  ret = nsConvertCANamesToStrings(arena, caNameStrings, caNames);
  if (ret != SECSuccess) {
    goto loser;
  }

  /* get the preference */
  if (NS_FAILED(nsGetUserCertChoice(&certChoice))) {
    goto loser;
  }

  /* find valid user cert and key pair */	
  if (certChoice == AUTO) {
    /* automatically find the right cert */

    /* find all user certs that are valid and for SSL */
    certList = CERT_FindUserCertsByUsage(CERT_GetDefaultCertDB(), 
                                         certUsageSSLClient, PR_FALSE,
                                         PR_TRUE, wincx);
    if (certList == NULL) {
      goto noCert;
    }

    /* filter the list to those issued by CAs supported by the server */
    ret = CERT_FilterCertListByCANames(certList, caNames->nnames,
                                       caNameStrings, certUsageSSLClient);
    if (ret != SECSuccess) {
      goto noCert;
    }

    /* make sure the list is not empty */
    node = CERT_LIST_HEAD(certList);
    if (CERT_LIST_END(node, certList)) {
      goto noCert;
    }

    CERTCertificate* low_prio_nonrep_cert = NULL;
    CERTCertificateCleaner low_prio_cleaner(low_prio_nonrep_cert);

    /* loop through the list until we find a cert with a key */
    while (!CERT_LIST_END(node, certList)) {
      /* if the certificate has restriction and we do not satisfy it
       * we do not use it
       */
#if 0		/* XXX This must be re-enabled */
      if (!CERT_MatchesScopeOfUse(node->cert, info->GetHostName,
                                  info->GetHostIP, info->GetHostPort)) {
          node = CERT_LIST_NEXT(node);
          continue;
      }
#endif

      privKey = PK11_FindKeyByAnyCert(node->cert, wincx);
      if (privKey != NULL) {
        if (hasExplicitKeyUsageNonRepudiation(node->cert)) {
          // Not a prefered cert
          if (!low_prio_nonrep_cert) // did not yet find a low prio cert
            low_prio_nonrep_cert = CERT_DupCertificate(node->cert);
        }
        else {
          // this is a good cert to present
          cert = CERT_DupCertificate(node->cert);
          break;
        }
      }
      keyError = PR_GetError();
      if (keyError == SEC_ERROR_BAD_PASSWORD) {
          /* problem with password: bail */
          goto loser;
      }

      node = CERT_LIST_NEXT(node);
    }

    if (!cert && low_prio_nonrep_cert) {
      cert = low_prio_nonrep_cert;
      low_prio_nonrep_cert = NULL; // take it away from the cleaner
    }

    if (cert == NULL) {
        goto noCert;
    }
  }
  else {
    /* user selects a cert to present */
    nsIClientAuthDialogs *dialogs = NULL;
    PRInt32 selectedIndex = -1;
    PRUnichar **certNicknameList = NULL;
    PRUnichar **certDetailsList = NULL;
    PRBool canceled;

    /* find all user certs that are for SSL */
    /* note that we are allowing expired certs in this list */
    certList = CERT_FindUserCertsByUsage(CERT_GetDefaultCertDB(), 
                                         certUsageSSLClient, PR_FALSE, 
                                         PR_FALSE, wincx);
    if (certList == NULL) {
      goto noCert;
    }

    if (caNames->nnames != 0) {
      /* filter the list to those issued by CAs supported by the 
       * server 
       */
      ret = CERT_FilterCertListByCANames(certList, caNames->nnames, 
                                        caNameStrings, 
                                        certUsageSSLClient);
      if (ret != SECSuccess) {
        goto loser;
      }
    }

    if (CERT_LIST_END(CERT_LIST_HEAD(certList), certList)) {
      /* list is empty - no matching certs */
      goto noCert;
    }

    /* filter it further for hostname restriction */
    node = CERT_LIST_HEAD(certList);
    while (!CERT_LIST_END(node, certList)) {
      ++NumberOfCerts;
#if 0 /* XXX Fix this */
      if (!CERT_MatchesScopeOfUse(node->cert, conn->hostName,
                                  conn->hostIP, conn->port)) {
        CERTCertListNode* removed = node;
        node = CERT_LIST_NEXT(removed);
        CERT_RemoveCertListNode(removed);
      }
      else {
        node = CERT_LIST_NEXT(node);
      }
#endif
      node = CERT_LIST_NEXT(node);
    }
    if (CERT_LIST_END(CERT_LIST_HEAD(certList), certList)) {
      goto noCert;
    }

    nicknames = getNSSCertNicknamesFromCertList(certList);

    if (nicknames == NULL) {
      goto loser;
    }

    NS_ASSERTION(nicknames->numnicknames == NumberOfCerts, "nicknames->numnicknames != NumberOfCerts");

    /* Get the SSL Certificate */
    serverCert = SSL_PeerCertificate(socket);
    if (serverCert == NULL) {
      /* couldn't get the server cert: what do I do? */
      goto loser;
    }

    /* Get CN and O of the subject and O of the issuer */
    char *ccn = CERT_GetCommonName(&serverCert->subject);
    NS_ConvertUTF8toUTF16 cn(ccn);
    if (ccn) PORT_Free(ccn);

    char *corg = CERT_GetOrgName(&serverCert->subject);
    NS_ConvertUTF8toUTF16 org(corg);
    if (corg) PORT_Free(corg);

    char *cissuer = CERT_GetOrgName(&serverCert->issuer);
    NS_ConvertUTF8toUTF16 issuer(cissuer);
    if (cissuer) PORT_Free(cissuer);

    CERT_DestroyCertificate(serverCert);

    certNicknameList = (PRUnichar **)nsMemory::Alloc(sizeof(PRUnichar *) * nicknames->numnicknames);
    if (!certNicknameList)
      goto loser;
    certDetailsList = (PRUnichar **)nsMemory::Alloc(sizeof(PRUnichar *) * nicknames->numnicknames);
    if (!certDetailsList) {
      nsMemory::Free(certNicknameList);
      goto loser;
    }

    PRInt32 CertsToUse;
    for (CertsToUse = 0, node = CERT_LIST_HEAD(certList);
         !CERT_LIST_END(node, certList) && CertsToUse < nicknames->numnicknames;
         node = CERT_LIST_NEXT(node)
        )
    {
      nsRefPtr<nsNSSCertificate> tempCert = new nsNSSCertificate(node->cert);

      if (!tempCert)
        continue;
      
      NS_ConvertUTF8toUTF16 i_nickname(nicknames->nicknames[CertsToUse]);
      nsAutoString nickWithSerial, details;
      
      if (NS_FAILED(tempCert->FormatUIStrings(i_nickname, nickWithSerial, details)))
        continue;

      certNicknameList[CertsToUse] = ToNewUnicode(nickWithSerial);
      if (!certNicknameList[CertsToUse])
        continue;
      certDetailsList[CertsToUse] = ToNewUnicode(details);
      if (!certDetailsList[CertsToUse]) {
        nsMemory::Free(certNicknameList[CertsToUse]);
        continue;
      }

      ++CertsToUse;
    }

    /* Throw up the client auth dialog and get back the index of the selected cert */
    rv = getNSSDialogs((void**)&dialogs, 
                       NS_GET_IID(nsIClientAuthDialogs),
                       NS_CLIENTAUTHDIALOGS_CONTRACTID);

    if (NS_FAILED(rv)) {
      NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(CertsToUse, certNicknameList);
      NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(CertsToUse, certDetailsList);
      goto loser;
    }

    {
      nsPSMUITracker tracker;
      if (tracker.isUIForbidden()) {
        rv = NS_ERROR_NOT_AVAILABLE;
      }
      else {
        rv = dialogs->ChooseCertificate(info, cn.get(), org.get(), issuer.get(), 
          (const PRUnichar**)certNicknameList, (const PRUnichar**)certDetailsList,
          CertsToUse, &selectedIndex, &canceled);
      }
    }

    NS_RELEASE(dialogs);
    NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(CertsToUse, certNicknameList);
    NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(CertsToUse, certDetailsList);
    
    if (NS_FAILED(rv)) goto loser;

    if (canceled) { rv = NS_ERROR_NOT_AVAILABLE; goto loser; }

    int i;
    for (i = 0, node = CERT_LIST_HEAD(certList);
         !CERT_LIST_END(node, certList);
         ++i, node = CERT_LIST_NEXT(node)) {

      if (i == selectedIndex) {
        cert = CERT_DupCertificate(node->cert);
        break;
      }
    }

    if (cert == NULL) {
      goto loser;
    }

    /* go get the private key */
    privKey = PK11_FindKeyByAnyCert(cert, wincx);
    if (privKey == NULL) {
      keyError = PR_GetError();
      if (keyError == SEC_ERROR_BAD_PASSWORD) {
          /* problem with password: bail */
          goto loser;
      }
      else {
          goto noCert;
      }
    }
  }
  goto done;

noCert:
loser:
  if (ret == SECSuccess) {
    ret = SECFailure;
  }
  if (cert != NULL) {
    CERT_DestroyCertificate(cert);
    cert = NULL;
  }
done:
  if (extracted != NULL) {
    PR_Free(extracted);
  }
  if (nicknames != NULL) {
    CERT_FreeNicknames(nicknames);
  }
  if (certList != NULL) {
    CERT_DestroyCertList(certList);
  }
  if (arena != NULL) {
    PORT_FreeArena(arena, PR_FALSE);
  }

  *pRetCert = cert;
  *pRetKey = privKey;

  return ret;
}

static SECStatus
nsNSSBadCertHandler(void *arg, PRFileDesc *sslSocket)
{
  nsNSSShutDownPreventionLock locker;
  SECStatus rv = SECFailure;
  int error;
  nsNSSSocketInfo* infoObject = (nsNSSSocketInfo *)arg;
  CERTCertificate *peerCert;
  nsNSSCertificate *nssCert;

  error = PR_GetError();
  peerCert = SSL_PeerCertificate(sslSocket);
  nssCert = new nsNSSCertificate(peerCert);
  if (!nssCert) {
    return SECFailure;
  } 
  NS_ADDREF(nssCert);
  infoObject->SetBlockedOnBadCertUI(PR_TRUE);
  while (rv != SECSuccess) {
     //Func nsContinueDespiteCertError does the same set of checks as func.
     //nsCertErrorNeedsDialog. So, removing call to nsCertErrorNeedsDialog
     if (!nsContinueDespiteCertError(infoObject, sslSocket, 
                                    error, nssCert)) {
      break;
    }
    rv = verifyCertAgain(peerCert, sslSocket, infoObject);
	error = PR_GetError();
  }
  infoObject->SetBlockedOnBadCertUI(PR_FALSE);
  NS_RELEASE(nssCert);
  CERT_DestroyCertificate(peerCert); 
  if (rv != SECSuccess) {
    // if the cert is bad, we don't want to connect
    infoObject->SetCanceled(PR_TRUE);
  }
  return rv;
}

static PRFileDesc*
nsSSLIOLayerImportFD(PRFileDesc *fd,
                     nsNSSSocketInfo *infoObject,
                     const char *host)
{
  nsNSSShutDownPreventionLock locker;
  PRFileDesc* sslSock = SSL_ImportFD(nsnull, fd);
  if (!sslSock) {
    NS_ASSERTION(PR_FALSE, "NSS: Error importing socket");
    return nsnull;
  }
  SSL_SetPKCS11PinArg(sslSock, (nsIInterfaceRequestor*)infoObject);
  SSL_HandshakeCallback(sslSock, HandshakeCallback, infoObject);
  SSL_GetClientAuthDataHook(sslSock, 
                            (SSLGetClientAuthData)nsNSS_SSLGetClientAuthData,
                            infoObject);
  SSL_AuthCertificateHook(sslSock, AuthCertificateCallback, 0);

  PRInt32 ret = SSL_SetURL(sslSock, host);
  if (ret == -1) {
    NS_ASSERTION(PR_FALSE, "NSS: Error setting server name");
    goto loser;
  }
  return sslSock;
loser:
  if (sslSock) {
    PR_Close(sslSock);
  }
  return nsnull;
}

static nsresult
nsSSLIOLayerSetOptions(PRFileDesc *fd, PRBool forSTARTTLS, 
                       const char *proxyHost, const char *host, PRInt32 port,
                       nsNSSSocketInfo *infoObject)
{
  nsNSSShutDownPreventionLock locker;
  if (forSTARTTLS || proxyHost) {
    if (SECSuccess != SSL_OptionSet(fd, SSL_SECURITY, PR_FALSE)) {
      return NS_ERROR_FAILURE;
    }
    infoObject->SetHasCleartextPhase(PR_TRUE);
  }

  if (forSTARTTLS) {
    if (SECSuccess != SSL_OptionSet(fd, SSL_ENABLE_SSL2, PR_FALSE)) {
      return NS_ERROR_FAILURE;
    }
    if (SECSuccess != SSL_OptionSet(fd, SSL_V2_COMPATIBLE_HELLO, PR_FALSE)) {
      return NS_ERROR_FAILURE;
    }
  }

  // Let's see if we're trying to connect to a site we know is
  // TLS intolerant.
  nsCAutoString key;
  key = nsDependentCString(host) + NS_LITERAL_CSTRING(":") + nsPrintfCString("%d", port);

  if (nsSSLIOLayerHelpers::isKnownAsIntolerantSite(key)) {
    if (SECSuccess != SSL_OptionSet(fd, SSL_ENABLE_TLS, PR_FALSE))
      return NS_ERROR_FAILURE;

    infoObject->SetAllowTLSIntoleranceTimeout(PR_FALSE);
      
    // We assume that protocols that use the STARTTLS mechanism should support
    // modern hellos. For other protocols, if we suspect a site 
    // does not support TLS, let's also use V2 hellos.
    // One advantage of this approach, if a site only supports the older
    // hellos, it is more likely that we will get a reasonable error code
    // on our single retry attempt.
    
    if (!forSTARTTLS &&
        SECSuccess != SSL_OptionSet(fd, SSL_V2_COMPATIBLE_HELLO, PR_TRUE))
      return NS_ERROR_FAILURE;
  }

  if (SECSuccess != SSL_OptionSet(fd, SSL_HANDSHAKE_AS_CLIENT, PR_TRUE)) {
    return NS_ERROR_FAILURE;
  }
  if (SECSuccess != SSL_BadCertHook(fd, (SSLBadCertHandler) nsNSSBadCertHandler,
                                    infoObject)) {
    return NS_ERROR_FAILURE;
  }

  // Set the Peer ID so that SSL proxy connections work properly.
  char *peerId = PR_smprintf("%s:%d", host, port);
  if (SECSuccess != SSL_SetSockPeerID(fd, peerId)) {
    PR_smprintf_free(peerId);
    return NS_ERROR_FAILURE;
  }

  PR_smprintf_free(peerId);
  return NS_OK;
}

nsresult
nsSSLIOLayerAddToSocket(PRInt32 family,
                        const char* host,
                        PRInt32 port,
                        const char* proxyHost,
                        PRInt32 proxyPort,
                        PRFileDesc* fd,
                        nsISupports** info,
                        PRBool forSTARTTLS)
{
  nsNSSShutDownPreventionLock locker;
  PRFileDesc* layer = nsnull;
  nsresult rv;

  nsNSSSocketInfo* infoObject = new nsNSSSocketInfo();
  if (!infoObject) return NS_ERROR_FAILURE;
  
  NS_ADDREF(infoObject);
  infoObject->SetForSTARTTLS(forSTARTTLS);
  infoObject->SetHostName(host);
  infoObject->SetPort(port);

  PRFileDesc *sslSock = nsSSLIOLayerImportFD(fd, infoObject, host);
  if (!sslSock) {
    NS_ASSERTION(PR_FALSE, "NSS: Error importing socket");
    goto loser;
  }

  infoObject->SetFileDescPtr(sslSock);

  rv = nsSSLIOLayerSetOptions(sslSock, forSTARTTLS, proxyHost, host, port,
                              infoObject);

  if (NS_FAILED(rv))
    goto loser;

  /* Now, layer ourselves on top of the SSL socket... */
  layer = PR_CreateIOLayerStub(nsSSLIOLayerHelpers::nsSSLIOLayerIdentity,
                               &nsSSLIOLayerHelpers::nsSSLIOLayerMethods);
  if (!layer)
    goto loser;
  
  layer->secret = (PRFilePrivate*) infoObject;
  rv = PR_PushIOLayer(sslSock, PR_GetLayersIdentity(sslSock), layer);
  
  if (NS_FAILED(rv)) {
    goto loser;
  }
  
  nsNSSShutDownList::trackSSLSocketCreate();

  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("[%p] Socket set up\n", (void*)sslSock));
  infoObject->QueryInterface(NS_GET_IID(nsISupports), (void**) (info));

  // We are going use a clear connection first //
  if (forSTARTTLS || proxyHost) {
    infoObject->SetHandshakePending(PR_FALSE);
  }

  return NS_OK;
 loser:
  NS_IF_RELEASE(infoObject);
  if (layer) {
    layer->dtor(layer);
  }
  return NS_ERROR_FAILURE;
}
