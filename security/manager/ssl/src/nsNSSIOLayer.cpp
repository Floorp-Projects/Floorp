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
#include "nsNSSCertificate.h"
#include "nsIX509CertValidity.h"
#include "nsIProxyObjectManager.h"
#include "nsProxiedService.h"
#include "nsIDateTimeFormat.h"
#include "nsDateTimeFormatCID.h"
#include "nsIClientAuthDialogs.h"
#include "nsClientAuthRemember.h"
#include "nsICertOverrideService.h"
#include "nsIBadCertListener2.h"
#include "nsISSLErrorListener.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsRecentBadCerts.h"
#include "nsISSLCertErrorDialog.h"
#include "nsIStrictTransportSecurityService.h"

#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsHashSets.h"
#include "nsCRT.h"
#include "nsAutoPtr.h"
#include "nsPrintfCString.h"
#include "nsSSLThread.h"
#include "nsNSSShutDown.h"
#include "nsSSLStatus.h"
#include "nsNSSCertHelper.h"
#include "nsNSSCleaner.h"
#include "nsThreadUtils.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsISecureBrowserUI.h"
#include "nsProxyRelease.h"
#include "nsIClassInfoImpl.h"
#include "nsIProgrammingLanguage.h"
#include "nsIArray.h"
#include "nsCharSeparatedTokenizer.h"

#include "ssl.h"
#include "secerr.h"
#include "sslerr.h"
#include "secder.h"
#include "secasn1.h"
#include "certdb.h"
#include "cert.h"
#include "keyhi.h"
#include "secport.h"

using namespace mozilla;

//#define DEBUG_SSL_VERBOSE //Enable this define to get minimal 
                            //reports when doing SSL read/write
                            
//#define DUMP_BUFFER  //Enable this define along with
                       //DEBUG_SSL_VERBOSE to dump SSL
                       //read/write buffer to a log.
                       //Uses PR_LOG except on Mac where
                       //we always write out to our own
                       //file.

NSSCleanupAutoPtrClass(CERTCertificate, CERT_DestroyCertificate)
NSSCleanupAutoPtrClass(char, PL_strfree)
NSSCleanupAutoPtrClass(void, PR_FREEIF)
NSSCleanupAutoPtrClass_WithParam(PRArenaPool, PORT_FreeArena, FalseParam, PR_FALSE)

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
, mOneBytePendingFromEarlierWrite(PR_FALSE)
, mThePendingByte(0)
, mOriginalRequestedTransferAmount(0)
{
}

nsSSLSocketThreadData::~nsSSLSocketThreadData()
{
  NS_ASSERTION(mSSLState != ssl_pending_write
               &&
               mSSLState != ssl_pending_read, 
               "oops??? ssl socket is not idle at the time it is being destroyed");
  if (mSSLDataBuffer) {
    nsMemory::Free(mSSLDataBuffer);
  }
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
    mSubRequestsHighSecurity(0),
    mSubRequestsLowSecurity(0),
    mSubRequestsBrokenSecurity(0),
    mSubRequestsNoSecurity(0),
    mDocShellDependentStuffKnown(PR_FALSE),
    mExternalErrorReporting(PR_FALSE),
    mForSTARTTLS(PR_FALSE),
    mHandshakePending(PR_TRUE),
    mCanceled(PR_FALSE),
    mHasCleartextPhase(PR_FALSE),
    mHandshakeInProgress(PR_FALSE),
    mAllowTLSIntoleranceTimeout(PR_TRUE),
    mRememberClientAuthCertificate(PR_FALSE),
    mHandshakeStartTime(0),
    mPort(0)
{
  mThreadData = new nsSSLSocketThreadData;
}

nsNSSSocketInfo::~nsNSSSocketInfo()
{
  delete mThreadData;

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return;

  shutdown(calledFromObject);
}

void nsNSSSocketInfo::virtualDestroyNSSReference()
{
}

NS_IMPL_THREADSAFE_ISUPPORTS9(nsNSSSocketInfo,
                              nsITransportSecurityInfo,
                              nsISSLSocketControl,
                              nsIInterfaceRequestor,
                              nsISSLStatusProvider,
                              nsIIdentityInfo,
                              nsIAssociatedContentSecurity,
                              nsISerializable,
                              nsIClassInfo,
                              nsIClientAuthUserDecision)

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
  mHostName.Adopt(host ? NS_strdup(host) : 0);
  return NS_OK;
}

nsresult
nsNSSSocketInfo::GetHostName(char **host)
{
  *host = (mHostName) ? NS_strdup(mHostName) : nsnull;
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

NS_IMETHODIMP nsNSSSocketInfo::GetRememberClientAuthCertificate(PRBool *aRememberClientAuthCertificate)
{
  NS_ENSURE_ARG_POINTER(aRememberClientAuthCertificate);
  *aRememberClientAuthCertificate = mRememberClientAuthCertificate;
  return NS_OK;
}

NS_IMETHODIMP nsNSSSocketInfo::SetRememberClientAuthCertificate(PRBool aRememberClientAuthCertificate)
{
  mRememberClientAuthCertificate = aRememberClientAuthCertificate;
  return NS_OK;
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

  mCallbacks = aCallbacks;
  mDocShellDependentStuffKnown = PR_FALSE;

  return NS_OK;
}

nsresult
nsNSSSocketInfo::EnsureDocShellDependentStuffKnown()
{
  if (mDocShellDependentStuffKnown)
    return NS_OK;

  if (!mCallbacks || nsSSLThread::exitRequested())
    return NS_ERROR_FAILURE;

  mDocShellDependentStuffKnown = PR_TRUE;

  nsCOMPtr<nsIInterfaceRequestor> proxiedCallbacks;
  NS_GetProxyForObject(NS_PROXY_TO_MAIN_THREAD,
                       NS_GET_IID(nsIInterfaceRequestor),
                       static_cast<nsIInterfaceRequestor*>(mCallbacks),
                       NS_PROXY_SYNC,
                       getter_AddRefs(proxiedCallbacks));

  // Are we running within a context that wants external SSL error reporting?
  // We'll look at the presence of a security UI object inside docshell.
  // If the docshell wants the lock icon, you'll get the ssl error pages, too.
  // This is helpful to distinguish from all other contexts, like mail windows,
  // or any other SSL connections running in the background.
  // We must query it now and remember, because fatal SSL errors will come 
  // with a socket close, and the socket transport might detach the callbacks 
  // instance prior to our error reporting.

  nsISecureBrowserUI* secureUI = nsnull;
  CallGetInterface(proxiedCallbacks.get(), &secureUI);

  nsCOMPtr<nsIDocShell> docshell;

  nsCOMPtr<nsIDocShellTreeItem> item(do_GetInterface(proxiedCallbacks));
  if (item)
  {
    nsCOMPtr<nsIDocShellTreeItem> proxiedItem;
    nsCOMPtr<nsIDocShellTreeItem> rootItem;
    NS_GetProxyForObject(NS_PROXY_TO_MAIN_THREAD,
                         NS_GET_IID(nsIDocShellTreeItem),
                         item.get(),
                         NS_PROXY_SYNC,
                         getter_AddRefs(proxiedItem));

    proxiedItem->GetSameTypeRootTreeItem(getter_AddRefs(rootItem));
    docshell = do_QueryInterface(rootItem);
    NS_ASSERTION(docshell, "rootItem do_QI is null");
  }

  if (docshell && !secureUI)
  {
    nsCOMPtr<nsIDocShell> proxiedDocShell;
    NS_GetProxyForObject(NS_PROXY_TO_MAIN_THREAD,
                         NS_GET_IID(nsIDocShell),
                         docshell.get(),
                         NS_PROXY_SYNC,
                         getter_AddRefs(proxiedDocShell));
    if (proxiedDocShell)
      proxiedDocShell->GetSecurityUI(&secureUI);
  }

  if (secureUI)
  {
    nsCOMPtr<nsIThread> mainThread(do_GetMainThread());
    NS_ProxyRelease(mainThread, secureUI, PR_FALSE);
    mExternalErrorReporting = PR_TRUE;

    // If this socket is associated to a docshell, let's try to remember
    // the currently used cert. If this socket gets a notification from NSS
    // having the same raw socket, we can keep the PSM wrapper object
    // and all the data it has cached (like verification results).
    nsCOMPtr<nsISSLStatusProvider> statprov = do_QueryInterface(secureUI);
    if (statprov) {
      nsCOMPtr<nsISupports> isup_stat;
      statprov->GetSSLStatus(getter_AddRefs(isup_stat));
      if (isup_stat) {
        nsCOMPtr<nsISSLStatus> sslstat = do_QueryInterface(isup_stat);
        if (sslstat) {
          sslstat->GetServerCert(getter_AddRefs(mPreviousCert));
        }
      }
    }
  }

  return NS_OK;
}

nsresult
nsNSSSocketInfo::GetExternalErrorReporting(PRBool* state)
{
  nsresult rv = EnsureDocShellDependentStuffKnown();
  NS_ENSURE_SUCCESS(rv, rv);
  *state = mExternalErrorReporting;
  return NS_OK;
}

nsresult
nsNSSSocketInfo::SetExternalErrorReporting(PRBool aState)
{
  mExternalErrorReporting = aState;
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

/* attribute unsigned long countSubRequestsHighSecurity; */
NS_IMETHODIMP nsNSSSocketInfo::GetCountSubRequestsHighSecurity(PRInt32 *aSubRequestsHighSecurity)
{
  *aSubRequestsHighSecurity = mSubRequestsHighSecurity;
  return NS_OK;
}
NS_IMETHODIMP nsNSSSocketInfo::SetCountSubRequestsHighSecurity(PRInt32 aSubRequestsHighSecurity)
{
  mSubRequestsHighSecurity = aSubRequestsHighSecurity;
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute unsigned long countSubRequestsLowSecurity; */
NS_IMETHODIMP nsNSSSocketInfo::GetCountSubRequestsLowSecurity(PRInt32 *aSubRequestsLowSecurity)
{
  *aSubRequestsLowSecurity = mSubRequestsLowSecurity;
  return NS_OK;
}
NS_IMETHODIMP nsNSSSocketInfo::SetCountSubRequestsLowSecurity(PRInt32 aSubRequestsLowSecurity)
{
  mSubRequestsLowSecurity = aSubRequestsLowSecurity;
  return NS_OK;
}

/* attribute unsigned long countSubRequestsBrokenSecurity; */
NS_IMETHODIMP nsNSSSocketInfo::GetCountSubRequestsBrokenSecurity(PRInt32 *aSubRequestsBrokenSecurity)
{
  *aSubRequestsBrokenSecurity = mSubRequestsBrokenSecurity;
  return NS_OK;
}
NS_IMETHODIMP nsNSSSocketInfo::SetCountSubRequestsBrokenSecurity(PRInt32 aSubRequestsBrokenSecurity)
{
  mSubRequestsBrokenSecurity = aSubRequestsBrokenSecurity;
  return NS_OK;
}

/* attribute unsigned long countSubRequestsNoSecurity; */
NS_IMETHODIMP nsNSSSocketInfo::GetCountSubRequestsNoSecurity(PRInt32 *aSubRequestsNoSecurity)
{
  *aSubRequestsNoSecurity = mSubRequestsNoSecurity;
  return NS_OK;
}
NS_IMETHODIMP nsNSSSocketInfo::SetCountSubRequestsNoSecurity(PRInt32 aSubRequestsNoSecurity)
{
  mSubRequestsNoSecurity = aSubRequestsNoSecurity;
  return NS_OK;
}
NS_IMETHODIMP nsNSSSocketInfo::Flush()
{
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

NS_IMETHODIMP
nsNSSSocketInfo::GetErrorMessage(PRUnichar** aText) {
  if (mErrorMessage.IsEmpty())
    *aText = nsnull;
  else {
    *aText = ToNewUnicode(mErrorMessage);
    NS_ENSURE_TRUE(*aText, NS_ERROR_OUT_OF_MEMORY);
  }
  return NS_OK;
}

nsresult
nsNSSSocketInfo::SetErrorMessage(const PRUnichar* aText) {
  mErrorMessage.Assign(aText);
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
    if (nsSSLThread::exitRequested())
      return NS_ERROR_FAILURE;

    nsCOMPtr<nsIInterfaceRequestor> proxiedCallbacks;
    NS_GetProxyForObject(NS_PROXY_TO_MAIN_THREAD,
                         NS_GET_IID(nsIInterfaceRequestor),
                         mCallbacks,
                         NS_PROXY_SYNC,
                         getter_AddRefs(proxiedCallbacks));

    rv = proxiedCallbacks->GetInterface(uuid, result);
  }
  return rv;
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

static NS_DEFINE_CID(kNSSCertificateCID, NS_X509CERT_CID);
#define NSSSOCKETINFOMAGIC { 0xa9863a23, 0x26b8, 0x4a9c, \
  { 0x83, 0xf1, 0xe9, 0xda, 0xdb, 0x36, 0xb8, 0x30 } }
static NS_DEFINE_CID(kNSSSocketInfoMagic, NSSSOCKETINFOMAGIC);

NS_IMETHODIMP
nsNSSSocketInfo::Write(nsIObjectOutputStream* stream) {
  stream->WriteID(kNSSSocketInfoMagic);

  // Store the flag if there is the certificate present
  stream->WriteBoolean(!!mCert);

  // As we are reading the object our self, not using ReadObject, we have
  // to store it here 'manually' as well, mimicking our object stream
  // implementation.
  nsCOMPtr<nsISerializable> certSerializable = do_QueryInterface(mCert);
  if (certSerializable) {
    stream->WriteID(kNSSCertificateCID);
    stream->WriteID(NS_GET_IID(nsISupports));
    certSerializable->Write(stream);
  }

  // Store the version number of the binary stream data format.
  // The 0xFFFF0000 mask is included to the version number
  // to distinguish version number from mSecurityState
  // field stored in times before versioning has been introduced.
  // This mask value has been chosen as mSecurityState could
  // never be assigned such value.
  PRUint32 version = 3;
  stream->Write32(version | 0xFFFF0000);
  stream->Write32(mSecurityState);
  stream->WriteWStringZ(mShortDesc.get());
  stream->WriteWStringZ(mErrorMessage.get());

  stream->WriteCompoundObject(NS_ISUPPORTS_CAST(nsISSLStatus*, mSSLStatus),
                              NS_GET_IID(nsISupports), PR_TRUE);

  stream->Write32((PRUint32)mSubRequestsHighSecurity);
  stream->Write32((PRUint32)mSubRequestsLowSecurity);
  stream->Write32((PRUint32)mSubRequestsBrokenSecurity);
  stream->Write32((PRUint32)mSubRequestsNoSecurity);
  return NS_OK;
}

static bool CheckUUIDEquals(PRUint32 m0,
                            nsIObjectInputStream* stream,
                            const nsCID& id)
{
  nsID tempID;
  tempID.m0 = m0;
  stream->Read16(&tempID.m1);
  stream->Read16(&tempID.m2);
  for (int i = 0; i < 8; ++i)
    stream->Read8(&tempID.m3[i]);
  return tempID.Equals(id);
}

NS_IMETHODIMP
nsNSSSocketInfo::Read(nsIObjectInputStream* stream) {
  nsresult rv;

  PRUint32 version;
  PRBool certificatePresent;

  // Check what we have here...
  PRUint32 UUID_0;
  stream->Read32(&UUID_0);
  if (UUID_0 == kNSSSocketInfoMagic.m0) {
    // It seems this stream begins with our magic ID, check it really is there
    if (!CheckUUIDEquals(UUID_0, stream, kNSSSocketInfoMagic))
      return NS_ERROR_FAILURE;

    // OK, this seems to be our stream, now continue to check there is
    // the certificate
    stream->ReadBoolean(&certificatePresent);
    stream->Read32(&UUID_0);
  }
  else {
    // There is no magic, assume there is a certificate present as in versions
    // prior to those with the magic didn't store that flag; we check the 
    // certificate is present by cheking the CID then
    certificatePresent = PR_TRUE;
  }

  if (certificatePresent && UUID_0 == kNSSCertificateCID.m0) {
    // It seems there is the certificate CID present, check it now; we only
    // have this single certificate implementation at this time.
    if (!CheckUUIDEquals(UUID_0, stream, kNSSCertificateCID))
      return NS_ERROR_FAILURE;

    // OK, we have read the CID of the certificate, check the interface ID
    nsID tempID;
    stream->ReadID(&tempID);
    if (!tempID.Equals(NS_GET_IID(nsISupports)))
      return NS_ERROR_FAILURE;

    nsCOMPtr<nsISerializable> serializable =
        do_CreateInstance(kNSSCertificateCID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    serializable->Read(stream);
    mCert = do_QueryInterface(serializable);

    // We are done with reading the certificate, now read the version
    // as we did before.
    stream->Read32(&version);
  }
  else {
    // There seems not to be the certificate present in the stream.
    version = UUID_0;
    mCert = nsnull;
  }

  // If the version field we have just read is not masked with 0xFFFF0000
  // then it is stored mSecurityState field and this is version 1 of
  // the binary data stream format.
  if ((version & 0xFFFF0000) == 0xFFFF0000) {
    version &= ~0xFFFF0000;
    stream->Read32(&mSecurityState);
  }
  else {
    mSecurityState = version;
    version = 1;
  }
  stream->ReadString(mShortDesc);
  stream->ReadString(mErrorMessage);

  nsCOMPtr<nsISupports> obj;
  stream->ReadObject(PR_TRUE, getter_AddRefs(obj));
  mSSLStatus = reinterpret_cast<nsSSLStatus*>(obj.get());

  if (version >= 2) {
    stream->Read32((PRUint32*)&mSubRequestsHighSecurity);
    stream->Read32((PRUint32*)&mSubRequestsLowSecurity);
    stream->Read32((PRUint32*)&mSubRequestsBrokenSecurity);
    stream->Read32((PRUint32*)&mSubRequestsNoSecurity);
  }
  else {
    mSubRequestsHighSecurity = 0;
    mSubRequestsLowSecurity = 0;
    mSubRequestsBrokenSecurity = 0;
    mSubRequestsNoSecurity = 0;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNSSSocketInfo::GetInterfaces(PRUint32 *count, nsIID * **array)
{
  *count = 0;
  *array = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSSocketInfo::GetHelperForLanguage(PRUint32 language, nsISupports **_retval)
{
  *_retval = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSSocketInfo::GetContractID(char * *aContractID)
{
  *aContractID = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSSocketInfo::GetClassDescription(char * *aClassDescription)
{
  *aClassDescription = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSSocketInfo::GetClassID(nsCID * *aClassID)
{
  *aClassID = (nsCID*) nsMemory::Alloc(sizeof(nsCID));
  if (!*aClassID)
    return NS_ERROR_OUT_OF_MEMORY;
  return GetClassIDNoAlloc(*aClassID);
}

NS_IMETHODIMP
nsNSSSocketInfo::GetImplementationLanguage(PRUint32 *aImplementationLanguage)
{
  *aImplementationLanguage = nsIProgrammingLanguage::CPLUSPLUS;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSSocketInfo::GetFlags(PRUint32 *aFlags)
{
  *aFlags = 0;
  return NS_OK;
}

static NS_DEFINE_CID(kNSSSocketInfoCID, NS_NSSSOCKETINFO_CID);

NS_IMETHODIMP
nsNSSSocketInfo::GetClassIDNoAlloc(nsCID *aClassIDNoAlloc)
{
  *aClassIDNoAlloc = kNSSSocketInfoCID;
  return NS_OK;
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

nsresult nsNSSSocketInfo::GetPreviousCert(nsIX509Cert** _result)
{
  NS_ENSURE_ARG_POINTER(_result);
  nsresult rv = EnsureDocShellDependentStuffKnown();
  NS_ENSURE_SUCCESS(rv, rv);

  *_result = mPreviousCert;
  NS_IF_ADDREF(*_result);

  return NS_OK;
}

nsresult nsNSSSocketInfo::GetCert(nsIX509Cert** _result)
{
  NS_ENSURE_ARG_POINTER(_result);

  *_result = mCert;
  NS_IF_ADDREF(*_result);

  return NS_OK;
}

nsresult nsNSSSocketInfo::SetCert(nsIX509Cert *aCert)
{
  mCert = aCert;

  return NS_OK;
}

nsresult nsNSSSocketInfo::GetSSLStatus(nsISupports** _result)
{
  NS_ENSURE_ARG_POINTER(_result);

  *_result = NS_ISUPPORTS_CAST(nsISSLStatus*, mSSLStatus);
  NS_IF_ADDREF(*_result);

  return NS_OK;
}

nsresult nsNSSSocketInfo::SetSSLStatus(nsSSLStatus *aSSLStatus)
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

void nsNSSSocketInfo::SetAllowTLSIntoleranceTimeout(PRBool aAllow)
{
  mAllowTLSIntoleranceTimeout = aAllow;
}

#define HANDSHAKE_TIMEOUT_SECONDS 25

PRBool nsNSSSocketInfo::HandshakeTimeout()
{
  if (!mHandshakeInProgress || !mAllowTLSIntoleranceTimeout)
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

  if (mTLSTolerantSites) {
    delete mTLSTolerantSites;
    mTLSTolerantSites = nsnull;
  }

  if (mRenegoUnrestrictedSites) {
    delete mRenegoUnrestrictedSites;
    mRenegoUnrestrictedSites = nsnull;
  }

  if (mSharedPollableEvent)
    PR_DestroyPollableEvent(mSharedPollableEvent);

  if (mutex) {
    delete mutex;
    mutex = nsnull;
  }

  if (mHostsWithCertErrors) {
    delete mHostsWithCertErrors;
    mHostsWithCertErrors = nsnull;
  }
}

static nsresult
getErrorMessage(PRInt32 err, 
                const nsString &host,
                PRInt32 port,
                PRBool externalErrorReporting,
                nsINSSComponent *component,
                nsString &returnedMessage)
{
  NS_ENSURE_ARG_POINTER(component);

  const PRUnichar *params[1];
  nsresult rv;

  if (host.Length())
  {
    nsString hostWithPort;

    // For now, hide port when it's 443 and we're reporting the error using
    // external reporting. In the future a better mechanism should be used
    // to make a decision about showing the port number, possibly by requiring
    // the context object to implement a specific interface.
    // The motivation is that Mozilla browser would like to hide the port number
    // in error pages in the common case.

    if (externalErrorReporting && port == 443) {
      params[0] = host.get();
    }
    else {
      hostWithPort = host;
      hostWithPort.AppendLiteral(":");
      hostWithPort.AppendInt(port);
      params[0] = hostWithPort.get();
    }

    nsString formattedString;
    rv = component->PIPBundleFormatStringFromName("SSLConnectionErrorPrefix", 
                                                  params, 1, 
                                                  formattedString);
    if (NS_SUCCEEDED(rv))
    {
      returnedMessage.Append(formattedString);
      returnedMessage.Append(NS_LITERAL_STRING("\n\n"));
    }
  }

  nsString explanation;
  rv = nsNSSErrors::getErrorMessageFromCode(err, component, explanation);
  if (NS_SUCCEEDED(rv))
    returnedMessage.Append(explanation);

  return NS_OK;
}

static void
AppendErrorTextUntrusted(PRErrorCode errTrust,
                         const nsString &host,
                         nsIX509Cert* ix509,
                         nsINSSComponent *component,
                         nsString &returnedMessage)
{
  const char *errorID = nsnull;
  nsCOMPtr<nsIX509Cert3> cert3 = do_QueryInterface(ix509);
  if (cert3) {
    PRBool isSelfSigned;
    if (NS_SUCCEEDED(cert3->GetIsSelfSigned(&isSelfSigned))
        && isSelfSigned) {
      errorID = "certErrorTrust_SelfSigned";
    }
  }

  if (!errorID) {
    switch (errTrust) {
      case SEC_ERROR_UNKNOWN_ISSUER:
      {
        nsCOMPtr<nsIArray> chain;
        ix509->GetChain(getter_AddRefs(chain));
        PRUint32 length = 0;
        if (chain && NS_FAILED(chain->GetLength(&length)))
          length = 0;
        if (length == 1)
          errorID = "certErrorTrust_MissingChain";
        else
          errorID = "certErrorTrust_UnknownIssuer";
        break;
      }
      case SEC_ERROR_INADEQUATE_KEY_USAGE:
        // Should get an individual string in the future
        // For now, use the same as CaInvalid
      case SEC_ERROR_CA_CERT_INVALID:
        errorID = "certErrorTrust_CaInvalid";
        break;
      case SEC_ERROR_UNTRUSTED_ISSUER:
        errorID = "certErrorTrust_Issuer";
        break;
      case SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE:
        errorID = "certErrorTrust_ExpiredIssuer";
        break;
      case SEC_ERROR_UNTRUSTED_CERT:
      default:
        errorID = "certErrorTrust_Untrusted";
        break;
    }
  }

  nsString formattedString;
  nsresult rv = component->GetPIPNSSBundleString(errorID, 
                                                 formattedString);
  if (NS_SUCCEEDED(rv))
  {
    returnedMessage.Append(formattedString);
    returnedMessage.Append(NS_LITERAL_STRING("\n"));
  }
}

// returns TRUE if SAN was used to produce names
// return FALSE if nothing was produced
// names => a single name or a list of names
// multipleNames => whether multiple names were delivered
static PRBool
GetSubjectAltNames(CERTCertificate *nssCert,
                   nsINSSComponent *component,
                   nsString &allNames,
                   PRUint32 &nameCount)
{
  allNames.Truncate();
  nameCount = 0;

  PRArenaPool *san_arena = nsnull;
  SECItem altNameExtension = {siBuffer, NULL, 0 };
  CERTGeneralName *sanNameList = nsnull;

  nsresult rv;
  rv = CERT_FindCertExtension(nssCert, SEC_OID_X509_SUBJECT_ALT_NAME,
                              &altNameExtension);
  if (rv != SECSuccess)
    return PR_FALSE;

  san_arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
  if (!san_arena)
    return PR_FALSE;

  sanNameList = CERT_DecodeAltNameExtension(san_arena, &altNameExtension);
  if (!sanNameList)
    return PR_FALSE;

  SECITEM_FreeItem(&altNameExtension, PR_FALSE);

  CERTGeneralName *current = sanNameList;
  do {
    nsAutoString name;
    switch (current->type) {
      case certDNSName:
        name.AssignASCII((char*)current->name.other.data, current->name.other.len);
        if (!allNames.IsEmpty()) {
          allNames.Append(NS_LITERAL_STRING(" , "));
        }
        ++nameCount;
        allNames.Append(name);
        break;

      case certIPAddress:
        {
          char buf[INET6_ADDRSTRLEN];
          PRNetAddr addr;
          if (current->name.other.len == 4) {
            addr.inet.family = PR_AF_INET;
            memcpy(&addr.inet.ip, current->name.other.data, current->name.other.len);
            PR_NetAddrToString(&addr, buf, sizeof(buf));
            name.AssignASCII(buf);
          } else if (current->name.other.len == 16) {
            addr.ipv6.family = PR_AF_INET6;
            memcpy(&addr.ipv6.ip, current->name.other.data, current->name.other.len);
            PR_NetAddrToString(&addr, buf, sizeof(buf));
            name.AssignASCII(buf);
          } else {
            /* invalid IP address */
          }
          if (!name.IsEmpty()) {
            if (!allNames.IsEmpty()) {
              allNames.Append(NS_LITERAL_STRING(" , "));
            }
            ++nameCount;
            allNames.Append(name);
          }
          break;
        }

      default: // all other types of names are ignored
        break;
    }
    current = CERT_GetNextGeneralName(current);
  } while (current != sanNameList); // double linked

  PORT_FreeArena(san_arena, PR_FALSE);
  return PR_TRUE;
}

static void
AppendErrorTextMismatch(const nsString &host,
                        nsIX509Cert* ix509,
                        nsINSSComponent *component,
                        PRBool wantsHtml,
                        nsString &returnedMessage)
{
  const PRUnichar *params[1];
  nsresult rv;

  CERTCertificate *nssCert = NULL;
  CERTCertificateCleaner nssCertCleaner(nssCert);

  nsCOMPtr<nsIX509Cert2> cert2 = do_QueryInterface(ix509, &rv);
  if (cert2)
    nssCert = cert2->GetCert();

  if (!nssCert) {
    // We are unable to extract the valid names, say "not valid for name".
    params[0] = host.get();
    nsString formattedString;
    rv = component->PIPBundleFormatStringFromName("certErrorMismatch", 
                                                  params, 1, 
                                                  formattedString);
    if (NS_SUCCEEDED(rv)) {
      returnedMessage.Append(formattedString);
      returnedMessage.Append(NS_LITERAL_STRING("\n"));
    }
    return;
  }

  nsString allNames;
  PRUint32 nameCount = 0;
  PRBool useSAN = PR_FALSE;

  if (nssCert)
    useSAN = GetSubjectAltNames(nssCert, component, allNames, nameCount);

  if (!useSAN) {
    char *certName = nsnull;
    // currently CERT_FindNSStringExtension is not being exported by NSS.
    // If it gets exported, enable the following line.
    //   certName = CERT_FindNSStringExtension(nssCert, SEC_OID_NS_CERT_EXT_SSL_SERVER_NAME);
    // However, it has been discussed to treat the extension as obsolete and ignore it.
    if (!certName)
      certName = CERT_GetCommonName(&nssCert->subject);
    if (certName) {
      ++nameCount;
      allNames.AssignASCII(certName);
      PORT_Free(certName);
    }
  }

  if (nameCount > 1) {
    nsString message;
    rv = component->GetPIPNSSBundleString("certErrorMismatchMultiple", 
                                          message);
    if (NS_SUCCEEDED(rv)) {
      returnedMessage.Append(message);
      returnedMessage.Append(NS_LITERAL_STRING("\n  "));
      returnedMessage.Append(allNames);
      returnedMessage.Append(NS_LITERAL_STRING("  \n"));
    }
  }
  else if (nameCount == 1) {
    const PRUnichar *params[1];
    params[0] = allNames.get();

    const char *stringID;
    if (wantsHtml)
      stringID = "certErrorMismatchSingle2";
    else
      stringID = "certErrorMismatchSinglePlain";

    nsString formattedString;
    rv = component->PIPBundleFormatStringFromName(stringID, 
                                                  params, 1, 
                                                  formattedString);
    if (NS_SUCCEEDED(rv)) {
      returnedMessage.Append(formattedString);
      returnedMessage.Append(NS_LITERAL_STRING("\n"));
    }
  }
  else { // nameCount == 0
    nsString message;
    nsresult rv = component->GetPIPNSSBundleString("certErrorMismatchNoNames",
                                                   message);
    if (NS_SUCCEEDED(rv)) {
      returnedMessage.Append(message);
      returnedMessage.Append(NS_LITERAL_STRING("\n"));
    }
  }
}

static void
GetDateBoundary(nsIX509Cert* ix509,
                nsString &formattedDate,
                nsString &nowDate,
                PRBool &trueExpired_falseNotYetValid)
{
  trueExpired_falseNotYetValid = PR_TRUE;
  formattedDate.Truncate();

  PRTime notAfter, notBefore, timeToUse;
  nsCOMPtr<nsIX509CertValidity> validity;
  nsresult rv;

  rv = ix509->GetValidity(getter_AddRefs(validity));
  if (NS_FAILED(rv))
    return;

  rv = validity->GetNotAfter(&notAfter);
  if (NS_FAILED(rv))
    return;

  rv = validity->GetNotBefore(&notBefore);
  if (NS_FAILED(rv))
    return;

  PRTime now = PR_Now();
  if (LL_CMP(now, >, notAfter)) {
    timeToUse = notAfter;
  } else {
    timeToUse = notBefore;
    trueExpired_falseNotYetValid = PR_FALSE;
  }

  nsCOMPtr<nsIDateTimeFormat> dateTimeFormat(do_CreateInstance(NS_DATETIMEFORMAT_CONTRACTID, &rv));
  if (NS_FAILED(rv))
    return;

  dateTimeFormat->FormatPRTime(nsnull, kDateFormatShort, 
                               kTimeFormatNoSeconds, timeToUse, 
                               formattedDate);
  dateTimeFormat->FormatPRTime(nsnull, kDateFormatShort,
                               kTimeFormatNoSeconds, now,
                               nowDate);
}

static void
AppendErrorTextTime(nsIX509Cert* ix509,
                    nsINSSComponent *component,
                    nsString &returnedMessage)
{
  nsAutoString formattedDate, nowDate;
  PRBool trueExpired_falseNotYetValid;
  GetDateBoundary(ix509, formattedDate, nowDate, trueExpired_falseNotYetValid);

  const PRUnichar *params[2];
  params[0] = formattedDate.get(); // might be empty, if helper function had a problem 
  params[1] = nowDate.get();

  const char *key = trueExpired_falseNotYetValid ? 
                    "certErrorExpiredNow" : "certErrorNotYetValidNow";
  nsresult rv;
  nsString formattedString;
  rv = component->PIPBundleFormatStringFromName(
           key,
           params, 
           NS_ARRAY_LENGTH(params),
           formattedString);
  if (NS_SUCCEEDED(rv))
  {
    returnedMessage.Append(formattedString);
    returnedMessage.Append(NS_LITERAL_STRING("\n"));
  }
}

static void
AppendErrorTextCode(PRErrorCode errorCodeToReport,
                    nsINSSComponent *component,
                    nsString &returnedMessage)
{
  const char *codeName = nsNSSErrors::getDefaultErrorStringName(errorCodeToReport);
  if (codeName)
  {
    nsCString error_id(codeName);
    ToLowerCase(error_id);
    NS_ConvertASCIItoUTF16 idU(error_id);

    const PRUnichar *params[1];
    params[0] = idU.get();

    nsString formattedString;
    nsresult rv;
    rv = component->PIPBundleFormatStringFromName("certErrorCodePrefix", 
                                                  params, 1, 
                                                  formattedString);
    if (NS_SUCCEEDED(rv)) {
      returnedMessage.Append(NS_LITERAL_STRING("\n"));
      returnedMessage.Append(formattedString);
      returnedMessage.Append(NS_LITERAL_STRING("\n"));
    }
    else {
      returnedMessage.Append(NS_LITERAL_STRING(" ("));
      returnedMessage.Append(idU);
      returnedMessage.Append(NS_LITERAL_STRING(")"));
    }
  }
}

static nsresult
getInvalidCertErrorMessage(PRUint32 multipleCollectedErrors, 
                           PRErrorCode errorCodeToReport, 
                           PRErrorCode errTrust, 
                           PRErrorCode errMismatch, 
                           PRErrorCode errExpired,
                           const nsString &host,
                           const nsString &hostWithPort,
                           PRInt32 port,
                           nsIX509Cert* ix509,
                           PRBool externalErrorReporting,
                           PRBool wantsHtml,
                           nsINSSComponent *component,
                           nsString &returnedMessage)
{
  NS_ENSURE_ARG_POINTER(component);

  const PRUnichar *params[1];
  nsresult rv;

  // For now, hide port when it's 443 and we're reporting the error using
  // external reporting. In the future a better mechanism should be used
  // to make a decision about showing the port number, possibly by requiring
  // the context object to implement a specific interface.
  // The motivation is that Mozilla browser would like to hide the port number
  // in error pages in the common case.
  
  if (externalErrorReporting && port == 443)
    params[0] = host.get();
  else
    params[0] = hostWithPort.get();

  nsString formattedString;
  rv = component->PIPBundleFormatStringFromName("certErrorIntro", 
                                                params, 1, 
                                                formattedString);
  if (NS_SUCCEEDED(rv))
  {
    returnedMessage.Append(formattedString);
    returnedMessage.Append(NS_LITERAL_STRING("\n\n"));
  }

  if (multipleCollectedErrors & nsICertOverrideService::ERROR_UNTRUSTED)
  {
    AppendErrorTextUntrusted(errTrust, host, ix509, 
                             component, returnedMessage);
  }

  if (multipleCollectedErrors & nsICertOverrideService::ERROR_MISMATCH)
  {
    AppendErrorTextMismatch(host, ix509, component, wantsHtml, returnedMessage);
  }

  if (multipleCollectedErrors & nsICertOverrideService::ERROR_TIME)
  {
    AppendErrorTextTime(ix509, component, returnedMessage);
  }

  AppendErrorTextCode(errorCodeToReport, component, returnedMessage);

  return NS_OK;
}

static nsresult
displayAlert(nsAFlatString &formattedString, nsNSSSocketInfo *infoObject)
{
  // The interface requestor object may not be safe, so proxy the call to get
  // the nsIPrompt.

  if (nsSSLThread::exitRequested())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIInterfaceRequestor> proxiedCallbacks;
  NS_GetProxyForObject(NS_PROXY_TO_MAIN_THREAD,
                       NS_GET_IID(nsIInterfaceRequestor),
                       static_cast<nsIInterfaceRequestor*>(infoObject),
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

  if (nsSSLThread::exitRequested()) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv;
  NS_DEFINE_CID(nssComponentCID, NS_NSSCOMPONENT_CID);
  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(nssComponentCID, &rv));
  if (NS_FAILED(rv))
    return rv;

  nsXPIDLCString hostName;
  socketInfo->GetHostName(getter_Copies(hostName));
  NS_ConvertASCIItoUTF16 hostNameU(hostName);

  PRInt32 port;
  socketInfo->GetPort(&port);

  // Try to get a nsISSLErrorListener implementation from the socket consumer.
  nsCOMPtr<nsIInterfaceRequestor> cb;
  socketInfo->GetNotificationCallbacks(getter_AddRefs(cb));
  if (cb) {
    nsCOMPtr<nsIInterfaceRequestor> callbacks;
    NS_GetProxyForObject(NS_PROXY_TO_MAIN_THREAD,
                         NS_GET_IID(nsIInterfaceRequestor),
                         cb,
                         NS_PROXY_SYNC,
                         getter_AddRefs(callbacks));

    nsCOMPtr<nsISSLErrorListener> sel = do_GetInterface(callbacks);
    if (sel) {
      nsISSLErrorListener *proxy_sel = nsnull;
      NS_GetProxyForObject(NS_PROXY_TO_MAIN_THREAD,
                           NS_GET_IID(nsISSLErrorListener),
                           sel,
                           NS_PROXY_SYNC,
                           (void**)&proxy_sel);
      if (proxy_sel) {
        nsIInterfaceRequestor *csi = static_cast<nsIInterfaceRequestor*>(socketInfo);
        PRBool suppressMessage = PR_FALSE;
        nsCString hostWithPortString = hostName;
        hostWithPortString.AppendLiteral(":");
        hostWithPortString.AppendInt(port);
        rv = proxy_sel->NotifySSLError(csi, err, hostWithPortString, 
                                       &suppressMessage);
        if (NS_SUCCEEDED(rv) && suppressMessage)
          return NS_OK;
      }
    }
  }

  PRBool external = PR_FALSE;
  socketInfo->GetExternalErrorReporting(&external);
  
  nsString formattedString;
  rv = getErrorMessage(err, hostNameU, port, external, nssComponent, formattedString);

  if (external)
  {
    socketInfo->SetErrorMessage(formattedString.get());
  }
  else
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

static nsresult
nsHandleInvalidCertError(nsNSSSocketInfo *socketInfo, 
                         PRUint32 multipleCollectedErrors, 
                         const nsACString &host, 
                         const nsACString &hostWithPort,
                         PRInt32 port,
                         PRErrorCode errorCodeToReport,
                         PRErrorCode errTrust, 
                         PRErrorCode errMismatch, 
                         PRErrorCode errExpired,
                         PRBool wantsHtml,
                         nsIX509Cert* ix509)
{
  nsresult rv;
  NS_DEFINE_CID(nssComponentCID, NS_NSSCOMPONENT_CID);
  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(nssComponentCID, &rv));
  if (NS_FAILED(rv))
    return rv;

  NS_ConvertASCIItoUTF16 hostU(host);
  NS_ConvertASCIItoUTF16 hostWithPortU(hostWithPort);

  // What mechanism is used to inform the user?
  // The highest priority has the "external error reporting" feature,
  // if set, we'll provide the strings to be used by the nsINSSErrorsService

  PRBool external = PR_FALSE;
  socketInfo->GetExternalErrorReporting(&external);
  
  nsString formattedString;
  rv = getInvalidCertErrorMessage(multipleCollectedErrors, errorCodeToReport,
                                  errTrust, errMismatch, errExpired,
                                  hostU, hostWithPortU, port, 
                                  ix509, external, wantsHtml,
                                  nssComponent, formattedString);

  if (external)
  {
    socketInfo->SetErrorMessage(formattedString.get());
  }
  else
  {
    nsPSMUITracker tracker;
    if (tracker.isUIForbidden()) {
      rv = NS_ERROR_NOT_AVAILABLE;
    }
    else {
      nsISSLCertErrorDialog *dialogs = nsnull;
      rv = getNSSDialogs((void**)&dialogs, 
        NS_GET_IID(nsISSLCertErrorDialog), 
        NS_SSLCERTERRORDIALOG_CONTRACTID);
  
      if (NS_SUCCEEDED(rv)) {
        nsPSMUITracker tracker;
        if (tracker.isUIForbidden()) {
          rv = NS_ERROR_NOT_AVAILABLE;
        }
        else {
          nsCOMPtr<nsISSLStatus> status;
          socketInfo->GetSSLStatus(getter_AddRefs(status));

          nsString empty;

          rv = dialogs->ShowCertError(nsnull, status, ix509, 
                                      formattedString, 
                                      empty, host, port);
        }
  
        NS_RELEASE(dialogs);
      }
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

// nsPSMRememberCertErrorsTable

nsPSMRememberCertErrorsTable::nsPSMRememberCertErrorsTable()
{
  mErrorHosts.Init(16);
}

nsresult
nsPSMRememberCertErrorsTable::GetHostPortKey(nsNSSSocketInfo* infoObject,
                                             nsCAutoString &result)
{
  nsresult rv;

  result.Truncate();

  nsXPIDLCString hostName;
  rv = infoObject->GetHostName(getter_Copies(hostName));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 port;
  rv = infoObject->GetPort(&port);
  NS_ENSURE_SUCCESS(rv, rv);

  result.Assign(hostName);
  result.Append(':');
  result.AppendInt(port);

  return NS_OK;
}

void
nsPSMRememberCertErrorsTable::RememberCertHasError(nsNSSSocketInfo* infoObject,
                                                   nsSSLStatus* status,
                                                   SECStatus certVerificationResult)
{
  nsresult rv;

  nsCAutoString hostPortKey;
  rv = GetHostPortKey(infoObject, hostPortKey);
  if (NS_FAILED(rv))
    return;

  if (certVerificationResult != SECSuccess) {
    NS_ASSERTION(status,
        "Must have nsSSLStatus object when remembering flags");

    if (!status)
      return;

    CertStateBits bits;
    bits.mIsDomainMismatch = status->mIsDomainMismatch;
    bits.mIsNotValidAtThisTime = status->mIsNotValidAtThisTime;
    bits.mIsUntrusted = status->mIsUntrusted;
    mErrorHosts.Put(hostPortKey, bits);
  }
  else {
    mErrorHosts.Remove(hostPortKey);
  }
}

void
nsPSMRememberCertErrorsTable::LookupCertErrorBits(nsNSSSocketInfo* infoObject,
                                                  nsSSLStatus* status)
{
  // Get remembered error bits from our cache, because of SSL session caching
  // the NSS library potentially hasn't notified us for this socket.
  if (status->mHaveCertErrorBits)
    // Rather do not modify bits if already set earlier
    return;

  nsresult rv;

  nsCAutoString hostPortKey;
  rv = GetHostPortKey(infoObject, hostPortKey);
  if (NS_FAILED(rv))
    return;

  CertStateBits bits;
  if (!mErrorHosts.Get(hostPortKey, &bits))
    // No record was found, this host had no cert errors
    return;

  // This host had cert errors, update the bits correctly
  status->mHaveCertErrorBits = PR_TRUE;
  status->mIsDomainMismatch = bits.mIsDomainMismatch;
  status->mIsNotValidAtThisTime = bits.mIsNotValidAtThisTime;
  status->mIsUntrusted = bits.mIsUntrusted;
}

void
nsSSLIOLayerHelpers::getSiteKey(nsNSSSocketInfo *socketInfo, nsCSubstring &key)
{
  PRInt32 port;
  socketInfo->GetPort(&port);

  nsXPIDLCString host;
  socketInfo->GetHostName(getter_Copies(host));

  key = host + NS_LITERAL_CSTRING(":") + nsPrintfCString("%d", port);
}

// Call this function to report a site that is possibly TLS intolerant.
// This function will return true, if the given socket is currently using TLS.
PRBool
nsSSLIOLayerHelpers::rememberPossibleTLSProblemSite(PRFileDesc* ssl_layer_fd, nsNSSSocketInfo *socketInfo)
{
  PRBool currentlyUsesTLS = PR_FALSE;

  nsCAutoString key;
  getSiteKey(socketInfo, key);

  SSL_OptionGet(ssl_layer_fd, SSL_ENABLE_TLS, &currentlyUsesTLS);
  if (!currentlyUsesTLS) {
    // We were not using TLS but failed with an intolerant error using
    // a different protocol. To give TLS a try on next connection attempt again
    // drop this site from the list of intolerant sites. TLS failure might be 
    // caused only by a traffic congestion while the server is TLS tolerant.
    removeIntolerantSite(key);
    return PR_FALSE;
  }

  PRBool enableSSL3 = PR_FALSE;
  SSL_OptionGet(ssl_layer_fd, SSL_ENABLE_SSL3, &enableSSL3);
  PRBool enableSSL2 = PR_FALSE;
  SSL_OptionGet(ssl_layer_fd, SSL_ENABLE_SSL2, &enableSSL2);
  if (enableSSL3 || enableSSL2) {
    // Add this site to the list of TLS intolerant sites.
    addIntolerantSite(key);
  }
  
  return currentlyUsesTLS;
}

void
nsSSLIOLayerHelpers::rememberTolerantSite(PRFileDesc* ssl_layer_fd, 
                                          nsNSSSocketInfo *socketInfo)
{
  PRBool usingSecurity = PR_FALSE;
  PRBool currentlyUsesTLS = PR_FALSE;
  SSL_OptionGet(ssl_layer_fd, SSL_SECURITY, &usingSecurity);
  SSL_OptionGet(ssl_layer_fd, SSL_ENABLE_TLS, &currentlyUsesTLS);
  if (!usingSecurity || !currentlyUsesTLS) {
    return;
  }

  nsCAutoString key;
  getSiteKey(socketInfo, key);

  MutexAutoLock lock(*mutex);
  nsSSLIOLayerHelpers::mTLSTolerantSites->Put(key);
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
isNonSSLErrorThatWeAllowToRetry(PRInt32 err, PRBool withInitialCleartext)
{
  switch (err)
  {
    case PR_CONNECT_RESET_ERROR:
      if (!withInitialCleartext)
        return PR_TRUE;
      break;
    
    case PR_END_OF_FILE_ERROR:
      return PR_TRUE;
  }

  return PR_FALSE;
}

static PRBool
isTLSIntoleranceError(PRInt32 err, PRBool withInitialCleartext)
{
  // This function is supposed to decide, which error codes should
  // be used to conclude server is TLS intolerant.
  // Note this only happens during the initial SSL handshake.
  // 
  // When not using a proxy we'll see a connection reset error.
  // When using a proxy, we'll see an end of file error.
  // In addition check for some error codes where it is reasonable
  // to retry without TLS.

  if (isNonSSLErrorThatWeAllowToRetry(err, withInitialCleartext))
    return PR_TRUE;

  switch (err)
  {
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
nsSSLThread::checkHandshake(PRInt32 bytesTransfered, 
                            PRBool wasReading,
                            PRFileDesc* ssl_layer_fd, 
                            nsNSSSocketInfo *socketInfo)
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

  // Do NOT assume TLS intolerance on a closed connection after bad cert ui was shown.
  // Simply retry.
  // This depends on the fact that Cert UI will not be shown again,
  // should the user override the bad cert.

  PRBool handleHandshakeResultNow;
  socketInfo->GetHandshakePending(&handleHandshakeResultNow);

  PRBool wantRetry = PR_FALSE;

  if (0 > bytesTransfered) {
    PRInt32 err = PR_GetError();

    if (handleHandshakeResultNow) {
      if (PR_WOULD_BLOCK_ERROR == err) {
        socketInfo->SetHandshakeInProgress(PR_TRUE);
        return bytesTransfered;
      }

      if (!wantRetry // no decision yet
          && isTLSIntoleranceError(err, socketInfo->GetHasCleartextPhase()))
      {
        wantRetry = nsSSLIOLayerHelpers::rememberPossibleTLSProblemSite(ssl_layer_fd, socketInfo);
      }
    }
    
    // This is the common place where we trigger an error message on a SSL socket.
    // This might be reached at any time of the connection.
    if (!wantRetry && (IS_SSL_ERROR(err) || IS_SEC_ERROR(err))) {
      nsHandleSSLError(socketInfo, err);
    }
  }
  else if (wasReading && 0 == bytesTransfered) // zero bytes on reading, socket closed
  {
    if (handleHandshakeResultNow)
    {
      if (!wantRetry // no decision yet
          && !socketInfo->GetHasCleartextPhase()) // mirror PR_CONNECT_RESET_ERROR treament
      {
        wantRetry = 
          nsSSLIOLayerHelpers::rememberPossibleTLSProblemSite(ssl_layer_fd, socketInfo);
      }
    }
  }

  if (wantRetry) {
    // We want to cause the network layer to retry the connection.
    PR_SetError(PR_CONNECT_RESET_ERROR, 0);
    if (wasReading)
      bytesTransfered = -1;
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
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("[%p] polling SSL socket\n", (void*)fd));
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

PRBool nsSSLIOLayerHelpers::nsSSLIOLayerInitialized = PR_FALSE;
PRDescIdentity nsSSLIOLayerHelpers::nsSSLIOLayerIdentity;
PRIOMethods nsSSLIOLayerHelpers::nsSSLIOLayerMethods;
Mutex *nsSSLIOLayerHelpers::mutex = nsnull;
nsCStringHashSet *nsSSLIOLayerHelpers::mTLSIntolerantSites = nsnull;
nsCStringHashSet *nsSSLIOLayerHelpers::mTLSTolerantSites = nsnull;
nsPSMRememberCertErrorsTable *nsSSLIOLayerHelpers::mHostsWithCertErrors = nsnull;
nsCStringHashSet *nsSSLIOLayerHelpers::mRenegoUnrestrictedSites = nsnull;
PRBool nsSSLIOLayerHelpers::mTreatUnsafeNegotiationAsBroken = PR_FALSE;
PRInt32 nsSSLIOLayerHelpers::mWarnLevelMissingRFC5746 = 1;
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
  if (!nsSSLIOLayerInitialized) {
    nsSSLIOLayerInitialized = PR_TRUE;
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
  }

  mutex = new Mutex("nsSSLIOLayerHelpers.mutex");

  mSharedPollableEvent = PR_NewPollableEvent();

  // if we can not get a pollable event, we'll have to do busy waiting

  mTLSIntolerantSites = new nsCStringHashSet();
  if (!mTLSIntolerantSites)
    return NS_ERROR_OUT_OF_MEMORY;

  mTLSIntolerantSites->Init(1);

  mTLSTolerantSites = new nsCStringHashSet();
  if (!mTLSTolerantSites)
    return NS_ERROR_OUT_OF_MEMORY;

  // Initialize the tolerant site hashtable to 16 items at the start seems
  // reasonable as most servers are TLS tolerant. We just want to lower 
  // the rate of hashtable array reallocation.
  mTLSTolerantSites->Init(16);

  mRenegoUnrestrictedSites = new nsCStringHashSet();
  if (!mRenegoUnrestrictedSites)
    return NS_ERROR_OUT_OF_MEMORY;

  mRenegoUnrestrictedSites->Init(1);

  mTreatUnsafeNegotiationAsBroken = PR_FALSE;
  
  mHostsWithCertErrors = new nsPSMRememberCertErrorsTable();
  if (!mHostsWithCertErrors || !mHostsWithCertErrors->mErrorHosts.IsInitialized())
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

void nsSSLIOLayerHelpers::addIntolerantSite(const nsCString &str)
{
  MutexAutoLock lock(*mutex);
  // Remember intolerant site only if it is not known as tolerant
  if (!mTLSTolerantSites->Contains(str))
    nsSSLIOLayerHelpers::mTLSIntolerantSites->Put(str);
}

void nsSSLIOLayerHelpers::removeIntolerantSite(const nsCString &str)
{
  MutexAutoLock lock(*mutex);
  nsSSLIOLayerHelpers::mTLSIntolerantSites->Remove(str);
}

PRBool nsSSLIOLayerHelpers::isKnownAsIntolerantSite(const nsCString &str)
{
  MutexAutoLock lock(*mutex);
  return mTLSIntolerantSites->Contains(str);
}

void nsSSLIOLayerHelpers::setRenegoUnrestrictedSites(const nsCString &str)
{
  MutexAutoLock lock(*mutex);
  
  if (mRenegoUnrestrictedSites) {
    delete mRenegoUnrestrictedSites;
    mRenegoUnrestrictedSites = nsnull;
  }

  mRenegoUnrestrictedSites = new nsCStringHashSet();
  if (!mRenegoUnrestrictedSites)
    return;
  
  mRenegoUnrestrictedSites->Init(1);
  
  nsCCharSeparatedTokenizer toker(str, ',');

  while (toker.hasMoreTokens()) {
    const nsCSubstring &host = toker.nextToken();
    if (!host.IsEmpty()) {
      mRenegoUnrestrictedSites->Put(host);
    }
  }
}

PRBool nsSSLIOLayerHelpers::isRenegoUnrestrictedSite(const nsCString &str)
{
  MutexAutoLock lock(*mutex);
  return mRenegoUnrestrictedSites->Contains(str);
}

void nsSSLIOLayerHelpers::setTreatUnsafeNegotiationAsBroken(PRBool broken)
{
  MutexAutoLock lock(*mutex);
  mTreatUnsafeNegotiationAsBroken = broken;
}

PRBool nsSSLIOLayerHelpers::treatUnsafeNegotiationAsBroken()
{
  MutexAutoLock lock(*mutex);
  return mTreatUnsafeNegotiationAsBroken;
}

void nsSSLIOLayerHelpers::setWarnLevelMissingRFC5746(PRInt32 level)
{
  MutexAutoLock lock(*mutex);
  mWarnLevelMissingRFC5746 = level;
}

PRInt32 nsSSLIOLayerHelpers::getWarnLevelMissingRFC5746()
{
  MutexAutoLock lock(*mutex);
  return mWarnLevelMissingRFC5746;
}

nsresult
nsSSLIOLayerNewSocket(PRInt32 family,
                      const char *host,
                      PRInt32 port,
                      const char *proxyHost,
                      PRInt32 proxyPort,
                      PRFileDesc **fd,
                      nsISupports** info,
                      PRBool forSTARTTLS,
                      PRBool anonymousLoad)
{

  PRFileDesc* sock = PR_OpenTCPSocket(family);
  if (!sock) return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv = nsSSLIOLayerAddToSocket(family, host, port, proxyHost, proxyPort,
                                        sock, info, forSTARTTLS, anonymousLoad);
  if (NS_FAILED(rv)) {
    PR_Close(sock);
    return rv;
  }

  *fd = sock;
  return NS_OK;
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
    PRUint32 contentlen;
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

  return !!(keyUsage & KU_NON_REPUDIATION);
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
  nsNSSSocketInfo* info = NULL;
  PRArenaPool* arena = NULL;
  char** caNameStrings;
  CERTCertificate* cert = NULL;
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
          SECKEY_DestroyPrivateKey(privKey);
          privKey = NULL;
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
      privKey = PK11_FindKeyByAnyCert(cert, wincx);
    }

    if (cert == NULL) {
        goto noCert;
    }
  }
  else { // Not Auto => ask
    /* Get the SSL Certificate */
    CERTCertificate* serverCert = NULL;
    CERTCertificateCleaner serverCertCleaner(serverCert);
    serverCert = SSL_PeerCertificate(socket);
    if (serverCert == NULL) {
      /* couldn't get the server cert: what do I do? */
      goto loser;
    }

    nsXPIDLCString hostname;
    info->GetHostName(getter_Copies(hostname));

    nsresult rv;
    NS_DEFINE_CID(nssComponentCID, NS_NSSCOMPONENT_CID);
    nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(nssComponentCID, &rv));
    nsRefPtr<nsClientAuthRememberService> cars;
    if (nssComponent) {
      nssComponent->GetClientAuthRememberService(getter_AddRefs(cars));
    }

    PRBool hasRemembered = PR_FALSE;
    nsCString rememberedDBKey;
    if (cars) {
      PRBool found;
      nsresult rv = cars->HasRememberedDecision(hostname, 
                                                serverCert,
                                                rememberedDBKey, &found);
      if (NS_SUCCEEDED(rv) && found) {
        hasRemembered = PR_TRUE;
      }
    }

    PRBool canceled = PR_FALSE;

if (hasRemembered)
{
    if (rememberedDBKey.IsEmpty())
    {
      canceled = PR_TRUE;
    }
    else
    {
      nsCOMPtr<nsIX509CertDB> certdb;
      certdb = do_GetService(NS_X509CERTDB_CONTRACTID);
      if (certdb)
      {
        nsCOMPtr<nsIX509Cert> found_cert;
        nsresult find_rv = 
          certdb->FindCertByDBKey(rememberedDBKey.get(), nsnull,
                                  getter_AddRefs(found_cert));
        if (NS_SUCCEEDED(find_rv) && found_cert) {
          nsNSSCertificate *obj_cert = reinterpret_cast<nsNSSCertificate *>(found_cert.get());
          if (obj_cert) {
            cert = obj_cert->GetCert();

#ifdef DEBUG_kaie
            nsAutoString nick, nickWithSerial, details;
            if (NS_SUCCEEDED(obj_cert->FormatUIStrings(nick, 
                                                       nickWithSerial, 
                                                       details))) {
              NS_LossyConvertUTF16toASCII asc(nickWithSerial);
              fprintf(stderr, "====> remembered serial %s\n", asc.get());
            }
#endif

          }
        }
        
        if (!cert) {
          hasRemembered = PR_FALSE;
        }
      }
    }
}

if (!hasRemembered)
{
    /* user selects a cert to present */
    nsIClientAuthDialogs *dialogs = NULL;
    PRInt32 selectedIndex = -1;
    PRUnichar **certNicknameList = NULL;
    PRUnichar **certDetailsList = NULL;

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

    /* Get CN and O of the subject and O of the issuer */
    char *ccn = CERT_GetCommonName(&serverCert->subject);
    void *v = ccn;
    voidCleaner ccnCleaner(v);
    NS_ConvertUTF8toUTF16 cn(ccn);

    PRInt32 port;
    info->GetPort(&port);

    nsString cn_host_port;
    if (ccn && strcmp(ccn, hostname) == 0) {
      cn_host_port.Append(cn);
      cn_host_port.AppendLiteral(":");
      cn_host_port.AppendInt(port);
    }
    else {
      cn_host_port.Append(cn);
      cn_host_port.AppendLiteral(" (");
      cn_host_port.AppendLiteral(":");
      cn_host_port.AppendInt(port);
      cn_host_port.AppendLiteral(")");
    }

    char *corg = CERT_GetOrgName(&serverCert->subject);
    NS_ConvertUTF8toUTF16 org(corg);
    if (corg) PORT_Free(corg);

    char *cissuer = CERT_GetOrgName(&serverCert->issuer);
    NS_ConvertUTF8toUTF16 issuer(cissuer);
    if (cissuer) PORT_Free(cissuer);

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
      nsRefPtr<nsNSSCertificate> tempCert = nsNSSCertificate::Create(node->cert);

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
        rv = dialogs->ChooseCertificate(info, cn_host_port.get(), org.get(), issuer.get(), 
          (const PRUnichar**)certNicknameList, (const PRUnichar**)certDetailsList,
          CertsToUse, &selectedIndex, &canceled);
      }
    }

    NS_RELEASE(dialogs);
    NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(CertsToUse, certNicknameList);
    NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(CertsToUse, certDetailsList);
    
    if (NS_FAILED(rv)) goto loser;

    // even if the user has canceled, we want to remember that, to avoid repeating prompts
    PRBool wantRemember = PR_FALSE;
    info->GetRememberClientAuthCertificate(&wantRemember);

    int i;
    if (!canceled)
    for (i = 0, node = CERT_LIST_HEAD(certList);
         !CERT_LIST_END(node, certList);
         ++i, node = CERT_LIST_NEXT(node)) {

      if (i == selectedIndex) {
        cert = CERT_DupCertificate(node->cert);
        break;
      }
    }

    if (cars && wantRemember) {
      cars->RememberDecision(hostname, 
                             serverCert, 
                             canceled ? 0 : cert);
    }
}

    if (canceled) { rv = NS_ERROR_NOT_AVAILABLE; goto loser; }

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
cancel_and_failure(nsNSSSocketInfo* infoObject)
{
  infoObject->SetCanceled(PR_TRUE);
  return SECFailure;
}

static SECStatus
nsNSSBadCertHandler(void *arg, PRFileDesc *sslSocket)
{
  nsNSSShutDownPreventionLock locker;
  nsNSSSocketInfo* infoObject = (nsNSSSocketInfo *)arg;
  if (!infoObject)
    return SECFailure;

  if (nsSSLThread::exitRequested())
    return cancel_and_failure(infoObject);

  CERTCertificate *peerCert = nsnull;
  CERTCertificateCleaner peerCertCleaner(peerCert);
  peerCert = SSL_PeerCertificate(sslSocket);
  if (!peerCert)
    return cancel_and_failure(infoObject);

  nsRefPtr<nsNSSCertificate> nssCert;
  nssCert = nsNSSCertificate::Create(peerCert);
  if (!nssCert)
    return cancel_and_failure(infoObject);

  nsCOMPtr<nsIX509Cert> ix509 = static_cast<nsIX509Cert*>(nssCert.get());

  SECStatus srv;
  nsresult nsrv;
  PRUint32 collected_errors = 0;
  PRUint32 remaining_display_errors = 0;

  PRErrorCode errorCodeTrust = SECSuccess;
  PRErrorCode errorCodeMismatch = SECSuccess;
  PRErrorCode errorCodeExpired = SECSuccess;

  char *hostname = SSL_RevealURL(sslSocket);
  if (!hostname)
    return cancel_and_failure(infoObject);

  charCleaner hostnameCleaner(hostname); 
  nsDependentCString hostString(hostname);

  PRInt32 port;
  infoObject->GetPort(&port);

  nsCString hostWithPortString = hostString;
  hostWithPortString.AppendLiteral(":");
  hostWithPortString.AppendInt(port);

  NS_ConvertUTF8toUTF16 hostWithPortStringUTF16(hostWithPortString);

  // Check the name field against the desired hostname.
  if (hostname[0] &&
      CERT_VerifyCertName(peerCert, hostname) != SECSuccess) {
    collected_errors |= nsICertOverrideService::ERROR_MISMATCH;
    errorCodeMismatch = SSL_ERROR_BAD_CERT_DOMAIN;
  }

  {
    PRArenaPool *log_arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (!log_arena)    
      return cancel_and_failure(infoObject);

    PRArenaPoolCleanerFalseParam log_arena_cleaner(log_arena);

    CERTVerifyLog *verify_log = PORT_ArenaZNew(log_arena, CERTVerifyLog);
    if (!verify_log)
      return cancel_and_failure(infoObject);

    CERTVerifyLogContentsCleaner verify_log_cleaner(verify_log);

    verify_log->arena = log_arena;

    srv = CERT_VerifyCertificate(CERT_GetDefaultCertDB(), peerCert,
                                 PR_TRUE, certificateUsageSSLServer,
                                 PR_Now(), (void*)infoObject, 
                                 verify_log, NULL);

    // We ignore the result code of the cert verification.
    // Either it is a failure, which is expected, and we'll process the
    //                         verify log below.
    // Or it is a success, then a domain mismatch is the only 
    //                     possible failure. 

    CERTVerifyLogNode *i_node;
    for (i_node = verify_log->head; i_node; i_node = i_node->next)
    {
      switch (i_node->error)
      {
        case SEC_ERROR_UNKNOWN_ISSUER:
        case SEC_ERROR_CA_CERT_INVALID:
        case SEC_ERROR_UNTRUSTED_ISSUER:
        case SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE:
        case SEC_ERROR_UNTRUSTED_CERT:
        case SEC_ERROR_INADEQUATE_KEY_USAGE:
          // We group all these errors as "cert not trusted"
          collected_errors |= nsICertOverrideService::ERROR_UNTRUSTED;
          if (errorCodeTrust == SECSuccess) {
            errorCodeTrust = i_node->error;
          }
          break;
        case SSL_ERROR_BAD_CERT_DOMAIN:
          collected_errors |= nsICertOverrideService::ERROR_MISMATCH;
          if (errorCodeMismatch == SECSuccess) {
            errorCodeMismatch = i_node->error;
          }
          break;
        case SEC_ERROR_EXPIRED_CERTIFICATE:
          collected_errors |= nsICertOverrideService::ERROR_TIME;
          if (errorCodeExpired == SECSuccess) {
            errorCodeExpired = i_node->error;
          }
          break;
        default:
          // we are not willing to continue on any other error
          nsHandleSSLError(infoObject, i_node->error);
          // this error is our stop condition, so let's make sure
          // this error code will be reported to the external world.
          PR_SetError(i_node->error, 0);
          return cancel_and_failure(infoObject);
      }
    }
  }

  if (!collected_errors)
  {
    NS_NOTREACHED("why did NSS call our bad cert handler if all looks good? Let's cancel the connection");
    return SECFailure;
  }

  nsRefPtr<nsSSLStatus> status = infoObject->SSLStatus();
  if (!status) {
    status = new nsSSLStatus();
    infoObject->SetSSLStatus(status);
  }

  if (status) {
    if (!status->mServerCert) {
      status->mServerCert = nssCert;
    }

    status->mHaveCertErrorBits = PR_TRUE;
    status->mIsDomainMismatch = collected_errors & nsICertOverrideService::ERROR_MISMATCH;
    status->mIsNotValidAtThisTime = collected_errors & nsICertOverrideService::ERROR_TIME;
    status->mIsUntrusted = collected_errors & nsICertOverrideService::ERROR_UNTRUSTED;

    nsSSLIOLayerHelpers::mHostsWithCertErrors->RememberCertHasError(
      infoObject, status, SECFailure);
  }

  remaining_display_errors = collected_errors;

  // Enforce Strict-Transport-Security for hosts that are "STS" hosts:
  // connections must be dropped when there are any certificate errors
  // (STS Spec section 7.3).

  nsCOMPtr<nsIStrictTransportSecurityService> stss
    = do_GetService(NS_STSSERVICE_CONTRACTID);
  nsCOMPtr<nsIStrictTransportSecurityService> proxied_stss;

  nsrv = NS_GetProxyForObject(NS_PROXY_TO_MAIN_THREAD,
                              NS_GET_IID(nsIStrictTransportSecurityService),
                              stss, NS_PROXY_SYNC,
                              getter_AddRefs(proxied_stss));
  NS_ENSURE_SUCCESS(nsrv, SECFailure);

  // now grab the host name to pass to the STS Service
  nsXPIDLCString hostName;
  nsrv = infoObject->GetHostName(getter_Copies(hostName));
  NS_ENSURE_SUCCESS(nsrv, SECFailure);

  PRBool strictTransportSecurityEnabled;
  nsrv = proxied_stss->IsStsHost(hostName, &strictTransportSecurityEnabled);
  NS_ENSURE_SUCCESS(nsrv, SECFailure);

  if (!strictTransportSecurityEnabled) {
    nsCOMPtr<nsICertOverrideService> overrideService =
      do_GetService(NS_CERTOVERRIDE_CONTRACTID);
    // it is fine to continue without the nsICertOverrideService

    PRUint32 overrideBits = 0;

    if (overrideService)
    {
      PRBool haveOverride;
      PRBool isTemporaryOverride; // we don't care

      nsrv = overrideService->HasMatchingOverride(hostString, port,
                                                  ix509,
                                                  &overrideBits,
                                                  &isTemporaryOverride, 
                                                  &haveOverride);
      if (NS_SUCCEEDED(nsrv) && haveOverride) 
      {
        // remove the errors that are already overriden
        remaining_display_errors -= overrideBits;
      }
    }

    if (!remaining_display_errors) {
      // all errors are covered by override rules, so let's accept the cert
      return SECSuccess;
    }
  } else {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("Strict-Transport-Security is violated: untrusted transport layer\n"));
  }

  // Ok, this is a full stop.
  // First, deliver the technical details of the broken SSL status,
  // giving the caller a chance to suppress the error messages.

  PRBool suppressMessage = PR_FALSE;
  nsresult rv;

  // Try to get a nsIBadCertListener2 implementation from the socket consumer.
  nsCOMPtr<nsIInterfaceRequestor> cb;
  infoObject->GetNotificationCallbacks(getter_AddRefs(cb));
  if (cb) {
    nsCOMPtr<nsIInterfaceRequestor> callbacks;
    NS_GetProxyForObject(NS_PROXY_TO_MAIN_THREAD,
                         NS_GET_IID(nsIInterfaceRequestor),
                         cb,
                         NS_PROXY_SYNC,
                         getter_AddRefs(callbacks));

    nsCOMPtr<nsIBadCertListener2> bcl = do_GetInterface(callbacks);
    if (bcl) {
      nsCOMPtr<nsIBadCertListener2> proxy_bcl;
      NS_GetProxyForObject(NS_PROXY_TO_MAIN_THREAD,
                           NS_GET_IID(nsIBadCertListener2),
                           bcl,
                           NS_PROXY_SYNC,
                           getter_AddRefs(proxy_bcl));
      if (proxy_bcl) {
        nsIInterfaceRequestor *csi = static_cast<nsIInterfaceRequestor*>(infoObject);
        rv = proxy_bcl->NotifyCertProblem(csi, status, hostWithPortString, 
                                          &suppressMessage);
      }
    }
  }

  nsCOMPtr<nsIRecentBadCertsService> recentBadCertsService = 
    do_GetService(NS_RECENTBADCERTS_CONTRACTID);

  if (recentBadCertsService) {
    recentBadCertsService->AddBadCert(hostWithPortStringUTF16, status);
  }

  // pick the error code to report by priority
  PRErrorCode errorCodeToReport = SECSuccess;
  if (remaining_display_errors & nsICertOverrideService::ERROR_UNTRUSTED)
    errorCodeToReport = errorCodeTrust;
  else if (remaining_display_errors & nsICertOverrideService::ERROR_MISMATCH)
    errorCodeToReport = errorCodeMismatch;
  else if (remaining_display_errors & nsICertOverrideService::ERROR_TIME)
    errorCodeToReport = errorCodeExpired;

  if (!suppressMessage) {
    PRBool external = PR_FALSE;
    infoObject->GetExternalErrorReporting(&external);

    nsHandleInvalidCertError(infoObject,
                             remaining_display_errors,
                             hostString,
                             hostWithPortString,
                             port,
                             errorCodeToReport,
                             errorCodeTrust,
                             errorCodeMismatch,
                             errorCodeExpired,
                             external, // wantsHtml
                             ix509);
  }

  PR_SetError(errorCodeToReport, 0);
  return cancel_and_failure(infoObject);
}

static PRFileDesc*
nsSSLIOLayerImportFD(PRFileDesc *fd,
                     nsNSSSocketInfo *infoObject,
                     const char *host,
                     PRBool anonymousLoad)
{
  nsNSSShutDownPreventionLock locker;
  PRFileDesc* sslSock = SSL_ImportFD(nsnull, fd);
  if (!sslSock) {
    NS_ASSERTION(PR_FALSE, "NSS: Error importing socket");
    return nsnull;
  }
  SSL_SetPKCS11PinArg(sslSock, (nsIInterfaceRequestor*)infoObject);
  SSL_HandshakeCallback(sslSock, HandshakeCallback, infoObject);

  // Disable this hook if we connect anonymously. See bug 466080.
  if (anonymousLoad) {
      SSL_GetClientAuthDataHook(sslSock, NULL, infoObject);
  } else {
      SSL_GetClientAuthDataHook(sslSock, 
                            (SSLGetClientAuthData)nsNSS_SSLGetClientAuthData,
                            infoObject);
  }
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
                       PRBool anonymousLoad, nsNSSSocketInfo *infoObject)
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
  
  if (nsSSLIOLayerHelpers::isRenegoUnrestrictedSite(nsDependentCString(host))) {
    if (SECSuccess != SSL_OptionSet(fd, SSL_REQUIRE_SAFE_NEGOTIATION, PR_FALSE)) {
      return NS_ERROR_FAILURE;
    }
    if (SECSuccess != SSL_OptionSet(fd, SSL_ENABLE_RENEGOTIATION, SSL_RENEGOTIATE_UNRESTRICTED)) {
      return NS_ERROR_FAILURE;
    }
  }

  // Set the Peer ID so that SSL proxy connections work properly.
  char *peerId;
  if (anonymousLoad) {  // See bug #466080. Separate the caches.
      peerId = PR_smprintf("anon:%s:%d", host, port);
  } else {
      peerId = PR_smprintf("%s:%d", host, port);
  }
  
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
                        PRBool forSTARTTLS,
                        PRBool anonymousLoad)
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

  PRFileDesc *sslSock = nsSSLIOLayerImportFD(fd, infoObject, host, anonymousLoad);
  if (!sslSock) {
    NS_ASSERTION(PR_FALSE, "NSS: Error importing socket");
    goto loser;
  }

  infoObject->SetFileDescPtr(sslSock);

  rv = nsSSLIOLayerSetOptions(sslSock,
                              forSTARTTLS, proxyHost, host, port, anonymousLoad,
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
