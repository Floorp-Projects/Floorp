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
 *   Terry Hayes <thayes@netscape.com>
 *   Kai Engert <kengert@redhat.com>
 *   Petr Kostka <petr.kostka@st.com>
 *   Honza Bambas <honzab@firemni.cz>
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
#include "nsNSSComponent.h" // for PIPNSS string bundle calls.
#include "nsNSSCallbacks.h"
#include "nsNSSCertificate.h"
#include "nsNSSCleaner.h"
#include "nsSSLStatus.h"
#include "nsNSSIOLayer.h" // for nsNSSSocketInfo
#include "nsIWebProgressListener.h"
#include "nsIStringBundle.h"
#include "nsXPIDLString.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsIServiceManager.h"
#include "nsReadableUtils.h"
#include "nsIPrompt.h"
#include "nsProxiedService.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsProtectedAuthThread.h"
#include "nsITokenDialogs.h"
#include "nsCRT.h"
#include "nsNSSShutDown.h"
#include "nsIUploadChannel.h"
#include "nsSSLThread.h"
#include "nsThreadUtils.h"
#include "nsIThread.h"
#include "nsIWindowWatcher.h"
#include "nsIPrompt.h"
#include "nsProxyRelease.h"
#include "nsIConsoleService.h"

#include "ssl.h"
#include "cert.h"
#include "ocsp.h"
#include "nssb64.h"
#include "secerr.h"

using namespace mozilla;

static NS_DEFINE_CID(kNSSComponentCID, NS_NSSCOMPONENT_CID);
NSSCleanupAutoPtrClass(CERTCertificate, CERT_DestroyCertificate)

#ifdef PR_LOGGING
extern PRLogModuleInfo* gPIPNSSLog;
#endif

class nsHTTPDownloadEvent : public nsRunnable {
public:
  nsHTTPDownloadEvent();
  ~nsHTTPDownloadEvent();

  NS_IMETHOD Run();

  nsNSSHttpRequestSession *mRequestSession;
  
  nsCOMPtr<nsHTTPListener> mListener;
  PRBool mResponsibleForDoneSignal;
};

nsHTTPDownloadEvent::nsHTTPDownloadEvent()
:mResponsibleForDoneSignal(PR_TRUE)
{
}

nsHTTPDownloadEvent::~nsHTTPDownloadEvent()
{
  if (mResponsibleForDoneSignal && mListener)
    mListener->send_done_signal();

  mRequestSession->Release();
}

NS_IMETHODIMP
nsHTTPDownloadEvent::Run()
{
  if (!mListener)
    return NS_OK;

  nsresult rv;

  nsCOMPtr<nsIIOService> ios = do_GetIOService();
  NS_ENSURE_STATE(ios);

  nsCOMPtr<nsIChannel> chan;
  ios->NewChannel(mRequestSession->mURL, nsnull, nsnull, getter_AddRefs(chan));
  NS_ENSURE_STATE(chan);

  // Create a loadgroup for this new channel.  This way if the channel
  // is redirected, we'll have a way to cancel the resulting channel.
  nsCOMPtr<nsILoadGroup> lg = do_CreateInstance(NS_LOADGROUP_CONTRACTID);
  chan->SetLoadGroup(lg);

  if (mRequestSession->mHasPostData)
  {
    nsCOMPtr<nsIInputStream> uploadStream;
    rv = NS_NewPostDataStream(getter_AddRefs(uploadStream),
                              PR_FALSE,
                              mRequestSession->mPostData,
                              0, ios);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIUploadChannel> uploadChannel(do_QueryInterface(chan));
    NS_ENSURE_STATE(uploadChannel);

    rv = uploadChannel->SetUploadStream(uploadStream, 
                                        mRequestSession->mPostContentType,
                                        -1);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsIHttpChannel> hchan = do_QueryInterface(chan);
  NS_ENSURE_STATE(hchan);

  rv = hchan->SetRequestMethod(mRequestSession->mRequestMethod);
  NS_ENSURE_SUCCESS(rv, rv);

  mResponsibleForDoneSignal = PR_FALSE;
  mListener->mResponsibleForDoneSignal = PR_TRUE;

  mListener->mLoadGroup = lg.get();
  NS_ADDREF(mListener->mLoadGroup);
  mListener->mLoadGroupOwnerThread = PR_GetCurrentThread();

  rv = NS_NewStreamLoader(getter_AddRefs(mListener->mLoader), 
                          mListener);

  if (NS_SUCCEEDED(rv))
    rv = hchan->AsyncOpen(mListener->mLoader, nsnull);

  if (NS_FAILED(rv)) {
    mListener->mResponsibleForDoneSignal = PR_FALSE;
    mResponsibleForDoneSignal = PR_TRUE;

    NS_RELEASE(mListener->mLoadGroup);
    mListener->mLoadGroup = nsnull;
    mListener->mLoadGroupOwnerThread = nsnull;
  }

  return NS_OK;
}

struct nsCancelHTTPDownloadEvent : nsRunnable {
  nsCOMPtr<nsHTTPListener> mListener;

  NS_IMETHOD Run() {
    mListener->FreeLoadGroup(PR_TRUE);
    mListener = nsnull;
    return NS_OK;
  }
};

SECStatus nsNSSHttpServerSession::createSessionFcn(const char *host,
                                                   PRUint16 portnum,
                                                   SEC_HTTP_SERVER_SESSION *pSession)
{
  if (!host || !pSession)
    return SECFailure;

  nsNSSHttpServerSession *hss = new nsNSSHttpServerSession;
  if (!hss)
    return SECFailure;

  hss->mHost = host;
  hss->mPort = portnum;

  *pSession = hss;
  return SECSuccess;
}

SECStatus nsNSSHttpRequestSession::createFcn(SEC_HTTP_SERVER_SESSION session,
                                             const char *http_protocol_variant,
                                             const char *path_and_query_string,
                                             const char *http_request_method, 
                                             const PRIntervalTime timeout, 
                                             SEC_HTTP_REQUEST_SESSION *pRequest)
{
  if (!session || !http_protocol_variant || !path_and_query_string || 
      !http_request_method || !pRequest)
    return SECFailure;

  nsNSSHttpServerSession* hss = static_cast<nsNSSHttpServerSession*>(session);
  if (!hss)
    return SECFailure;

  nsNSSHttpRequestSession *rs = new nsNSSHttpRequestSession;
  if (!rs)
    return SECFailure;

  rs->mTimeoutInterval = timeout;

  // Use a maximum timeout value of 10 seconds because of bug 404059.
  // FIXME: Use a better approach once 406120 is ready.
  PRUint32 maxBug404059Timeout = PR_TicksPerSecond() * 10;
  if (timeout > maxBug404059Timeout) {
    rs->mTimeoutInterval = maxBug404059Timeout;
  }

  rs->mURL.Assign(http_protocol_variant);
  rs->mURL.AppendLiteral("://");
  rs->mURL.Append(hss->mHost);
  rs->mURL.AppendLiteral(":");
  rs->mURL.AppendInt(hss->mPort);
  rs->mURL.Append(path_and_query_string);

  rs->mRequestMethod = http_request_method;

  *pRequest = (void*)rs;
  return SECSuccess;
}

SECStatus nsNSSHttpRequestSession::setPostDataFcn(const char *http_data, 
                                                  const PRUint32 http_data_len,
                                                  const char *http_content_type)
{
  mHasPostData = PR_TRUE;
  mPostData.Assign(http_data, http_data_len);
  mPostContentType.Assign(http_content_type);

  return SECSuccess;
}

SECStatus nsNSSHttpRequestSession::addHeaderFcn(const char *http_header_name, 
                                                const char *http_header_value)
{
  return SECFailure; // not yet implemented

  // All http code needs to be postponed to the UI thread.
  // Once this gets implemented, we need to add a string list member to
  // nsNSSHttpRequestSession and queue up the headers,
  // so they can be added in HandleHTTPDownloadPLEvent.
  //
  // The header will need to be set using 
  //   mHttpChannel->SetRequestHeader(nsDependentCString(http_header_name), 
  //                                  nsDependentCString(http_header_value), 
  //                                  PR_FALSE)));
}

SECStatus nsNSSHttpRequestSession::trySendAndReceiveFcn(PRPollDesc **pPollDesc,
                                                        PRUint16 *http_response_code, 
                                                        const char **http_response_content_type, 
                                                        const char **http_response_headers, 
                                                        const char **http_response_data, 
                                                        PRUint32 *http_response_data_len)
{
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG,
         ("nsNSSHttpRequestSession::trySendAndReceiveFcn to %s\n", mURL.get()));

  const int max_retries = 2;
  int retry_count = 0;
  PRBool retryable_error = PR_FALSE;
  SECStatus result_sec_status = SECFailure;

  do
  {
    if (retry_count > 0)
    {
      if (retryable_error)
      {
        PR_LOG(gPIPNSSLog, PR_LOG_DEBUG,
               ("nsNSSHttpRequestSession::trySendAndReceiveFcn - sleeping and retrying: %d of %d\n",
                retry_count, max_retries));
      }

      PR_Sleep( PR_MillisecondsToInterval(300) * retry_count );
    }

    ++retry_count;
    retryable_error = PR_FALSE;

    result_sec_status =
      internal_send_receive_attempt(retryable_error, pPollDesc, http_response_code,
                                    http_response_content_type, http_response_headers,
                                    http_response_data, http_response_data_len);
  }
  while (retryable_error &&
         retry_count < max_retries);

#ifdef PR_LOGGING
  if (retry_count > 1)
  {
    if (retryable_error)
      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG,
             ("nsNSSHttpRequestSession::trySendAndReceiveFcn - still failing, giving up...\n"));
    else
      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG,
             ("nsNSSHttpRequestSession::trySendAndReceiveFcn - success at attempt %d\n",
              retry_count));
  }
#endif

  return result_sec_status;
}

void
nsNSSHttpRequestSession::AddRef()
{
  NS_AtomicIncrementRefcnt(mRefCount);
}

void
nsNSSHttpRequestSession::Release()
{
  PRInt32 newRefCount = NS_AtomicDecrementRefcnt(mRefCount);
  if (!newRefCount) {
    delete this;
  }
}

SECStatus
nsNSSHttpRequestSession::internal_send_receive_attempt(PRBool &retryable_error,
                                                       PRPollDesc **pPollDesc,
                                                       PRUint16 *http_response_code,
                                                       const char **http_response_content_type,
                                                       const char **http_response_headers,
                                                       const char **http_response_data,
                                                       PRUint32 *http_response_data_len)
{
  if (pPollDesc) *pPollDesc = nsnull;
  if (http_response_code) *http_response_code = 0;
  if (http_response_content_type) *http_response_content_type = 0;
  if (http_response_headers) *http_response_headers = 0;
  if (http_response_data) *http_response_data = 0;

  PRUint32 acceptableResultSize = 0;

  if (http_response_data_len)
  {
    acceptableResultSize = *http_response_data_len;
    *http_response_data_len = 0;
  }
  
  if (!mListener)
    return SECFailure;

  Mutex& waitLock = mListener->mLock;
  CondVar& waitCondition = mListener->mCondition;
  volatile PRBool &waitFlag = mListener->mWaitFlag;
  waitFlag = PR_TRUE;

  nsRefPtr<nsHTTPDownloadEvent> event = new nsHTTPDownloadEvent;
  if (!event)
    return SECFailure;

  event->mListener = mListener;
  this->AddRef();
  event->mRequestSession = this;

  nsresult rv = NS_DispatchToMainThread(event);
  if (NS_FAILED(rv))
  {
    event->mResponsibleForDoneSignal = PR_FALSE;
    return SECFailure;
  }

  PRBool request_canceled = PR_FALSE;

  {
    MutexAutoLock locker(waitLock);

    const PRIntervalTime start_time = PR_IntervalNow();
    PRIntervalTime wait_interval;

    PRBool running_on_main_thread = NS_IsMainThread();
    if (running_on_main_thread)
    {
      // let's process events quickly
      wait_interval = PR_MicrosecondsToInterval(50);
    }
    else
    { 
      // On a secondary thread, it's fine to wait some more for
      // for the condition variable.
      wait_interval = PR_MillisecondsToInterval(250);
    }

    while (waitFlag)
    {
      if (running_on_main_thread)
      {
        // Networking runs on the main thread, which we happen to block here.
        // Processing events will allow the OCSP networking to run while we 
        // are waiting. Thanks a lot to Darin Fisher for rewriting the 
        // thread manager. Thanks a lot to Christian Biesinger who
        // made me aware of this possibility. (kaie)

        MutexAutoUnlock unlock(waitLock);
        NS_ProcessNextEvent(nsnull);
      }

      waitCondition.Wait(wait_interval);
      
      if (!waitFlag)
        break;

      if (!request_canceled)
      {
        PRBool wantExit = nsSSLThread::exitRequested();
        PRBool timeout = 
          (PRIntervalTime)(PR_IntervalNow() - start_time) > mTimeoutInterval;

        if (wantExit || timeout)
        {
          request_canceled = PR_TRUE;

          nsRefPtr<nsCancelHTTPDownloadEvent> cancelevent = new nsCancelHTTPDownloadEvent;
          cancelevent->mListener = mListener;
          rv = NS_DispatchToMainThread(cancelevent);
          if (NS_FAILED(rv)) {
            NS_WARNING("cannot post cancel event");
          }
          break;
        }
      }
    }
  }

  if (request_canceled)
    return SECFailure;

  if (NS_FAILED(mListener->mResultCode))
  {
    if (mListener->mResultCode == NS_ERROR_CONNECTION_REFUSED
        ||
        mListener->mResultCode == NS_ERROR_NET_RESET)
    {
      retryable_error = PR_TRUE;
    }
    return SECFailure;
  }

  if (http_response_code)
    *http_response_code = mListener->mHttpResponseCode;

  if (mListener->mHttpRequestSucceeded && http_response_data && http_response_data_len) {

    *http_response_data_len = mListener->mResultLen;
  
    // acceptableResultSize == 0 means: any size is acceptable
    if (acceptableResultSize != 0
        &&
        acceptableResultSize < mListener->mResultLen)
    {
      return SECFailure;
    }

    // return data by reference, result data will be valid 
    // until "this" gets destroyed by NSS
    *http_response_data = (const char*)mListener->mResultData;
  }

  if (mListener->mHttpRequestSucceeded && http_response_content_type) {
    if (mListener->mHttpResponseContentType.Length()) {
      *http_response_content_type = mListener->mHttpResponseContentType.get();
    }
  }

  return SECSuccess;
}

SECStatus nsNSSHttpRequestSession::cancelFcn()
{
  // As of today, only the blocking variant of the http interface
  // has been implemented. Implementing cancelFcn will be necessary
  // as soon as we implement the nonblocking variant.
  return SECSuccess;
}

SECStatus nsNSSHttpRequestSession::freeFcn()
{
  Release();
  return SECSuccess;
}

nsNSSHttpRequestSession::nsNSSHttpRequestSession()
: mRefCount(1),
  mHasPostData(PR_FALSE),
  mTimeoutInterval(0),
  mListener(new nsHTTPListener)
{
}

nsNSSHttpRequestSession::~nsNSSHttpRequestSession()
{
}

SEC_HttpClientFcn nsNSSHttpInterface::sNSSInterfaceTable;

void nsNSSHttpInterface::initTable()
{
  sNSSInterfaceTable.version = 1;
  SEC_HttpClientFcnV1 &v1 = sNSSInterfaceTable.fcnTable.ftable1;
  v1.createSessionFcn = createSessionFcn;
  v1.keepAliveSessionFcn = keepAliveFcn;
  v1.freeSessionFcn = freeSessionFcn;
  v1.createFcn = createFcn;
  v1.setPostDataFcn = setPostDataFcn;
  v1.addHeaderFcn = addHeaderFcn;
  v1.trySendAndReceiveFcn = trySendAndReceiveFcn;
  v1.cancelFcn = cancelFcn;
  v1.freeFcn = freeFcn;
}

void nsNSSHttpInterface::registerHttpClient()
{
  SEC_RegisterDefaultHttpClient(&sNSSInterfaceTable);
}

void nsNSSHttpInterface::unregisterHttpClient()
{
  SEC_RegisterDefaultHttpClient(nsnull);
}

nsHTTPListener::nsHTTPListener()
: mResultData(nsnull),
  mResultLen(0),
  mLock("nsHTTPListener.mLock"),
  mCondition(mLock, "nsHTTPListener.mCondition"),
  mWaitFlag(PR_TRUE),
  mResponsibleForDoneSignal(PR_FALSE),
  mLoadGroup(nsnull),
  mLoadGroupOwnerThread(nsnull)
{
}

nsHTTPListener::~nsHTTPListener()
{
  if (mResponsibleForDoneSignal)
    send_done_signal();

  if (mLoader) {
    nsCOMPtr<nsIThread> mainThread(do_GetMainThread());
    NS_ProxyRelease(mainThread, mLoader);
  }
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsHTTPListener, nsIStreamLoaderObserver)

void
nsHTTPListener::FreeLoadGroup(PRBool aCancelLoad)
{
  nsILoadGroup *lg = nsnull;

  MutexAutoLock locker(mLock);

  if (mLoadGroup) {
    if (mLoadGroupOwnerThread != PR_GetCurrentThread()) {
      NS_ASSERTION(PR_FALSE,
                   "attempt to access nsHTTPDownloadEvent::mLoadGroup on multiple threads, leaking it!");
    }
    else {
      lg = mLoadGroup;
      mLoadGroup = nsnull;
    }
  }

  if (lg) {
    if (aCancelLoad) {
      lg->Cancel(NS_ERROR_ABORT);
    }
    NS_RELEASE(lg);
  }
}

NS_IMETHODIMP
nsHTTPListener::OnStreamComplete(nsIStreamLoader* aLoader,
                                 nsISupports* aContext,
                                 nsresult aStatus,
                                 PRUint32 stringLen,
                                 const PRUint8* string)
{
  mResultCode = aStatus;

  FreeLoadGroup(PR_FALSE);

  nsCOMPtr<nsIRequest> req;
  nsCOMPtr<nsIHttpChannel> hchan;

  nsresult rv = aLoader->GetRequest(getter_AddRefs(req));
  
#ifdef PR_LOGGING
  if (NS_FAILED(aStatus))
  {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG,
           ("nsHTTPListener::OnStreamComplete status failed %d", aStatus));
  }
#endif

  if (NS_SUCCEEDED(rv))
    hchan = do_QueryInterface(req, &rv);

  if (NS_SUCCEEDED(rv))
  {
    rv = hchan->GetRequestSucceeded(&mHttpRequestSucceeded);
    if (NS_FAILED(rv))
      mHttpRequestSucceeded = PR_FALSE;

    mResultLen = stringLen;
    mResultData = string; // reference. Make sure loader lives as long as this

    unsigned int rcode;
    rv = hchan->GetResponseStatus(&rcode);
    if (NS_FAILED(rv))
      mHttpResponseCode = 500;
    else
      mHttpResponseCode = rcode;

    hchan->GetResponseHeader(NS_LITERAL_CSTRING("Content-Type"), 
                                    mHttpResponseContentType);
  }

  if (mResponsibleForDoneSignal)
    send_done_signal();
  
  return aStatus;
}

void nsHTTPListener::send_done_signal()
{
  mResponsibleForDoneSignal = PR_FALSE;

  {
    MutexAutoLock locker(mLock);
    mWaitFlag = PR_FALSE;
    mCondition.NotifyAll();
  }
}

static char*
ShowProtectedAuthPrompt(PK11SlotInfo* slot, nsIInterfaceRequestor *ir)
{
  char* protAuthRetVal = nsnull;

  // Get protected auth dialogs
  nsITokenDialogs* dialogs = 0;
  nsresult nsrv = getNSSDialogs((void**)&dialogs, 
                                NS_GET_IID(nsITokenDialogs), 
                                NS_TOKENDIALOGS_CONTRACTID);
  if (NS_SUCCEEDED(nsrv))
  {
    nsProtectedAuthThread* protectedAuthRunnable = new nsProtectedAuthThread();
    if (protectedAuthRunnable)
    {
      NS_ADDREF(protectedAuthRunnable);

      protectedAuthRunnable->SetParams(slot);
      
      nsCOMPtr<nsIProtectedAuthThread> runnable = do_QueryInterface(protectedAuthRunnable);
      if (runnable)
      {
        nsrv = dialogs->DisplayProtectedAuth(ir, runnable);
              
        // We call join on the thread,
        // so we can be sure that no simultaneous access will happen.
        protectedAuthRunnable->Join();
              
        if (NS_SUCCEEDED(nsrv))
        {
          SECStatus rv = protectedAuthRunnable->GetResult();
          switch (rv)
          {
              case SECSuccess:
                  protAuthRetVal = ToNewCString(nsDependentCString(PK11_PW_AUTHENTICATED));
                  break;
              case SECWouldBlock:
                  protAuthRetVal = ToNewCString(nsDependentCString(PK11_PW_RETRY));
                  break;
              default:
                  protAuthRetVal = nsnull;
                  break;
              
          }
        }
      }

      NS_RELEASE(protectedAuthRunnable);
    }

    NS_RELEASE(dialogs);
  }

  return protAuthRetVal;
}
  
char* PR_CALLBACK
PK11PasswordPrompt(PK11SlotInfo* slot, PRBool retry, void* arg) {
  nsNSSShutDownPreventionLock locker;
  nsresult rv = NS_OK;
  PRUnichar *password = nsnull;
  PRBool value = PR_FALSE;
  nsIInterfaceRequestor *ir = static_cast<nsIInterfaceRequestor*>(arg);
  nsCOMPtr<nsIPrompt> proxyPrompt;

  /* TODO: Retry should generate a different dialog message */
/*
  if (retry)
    return nsnull;
*/

  if (!ir)
  {
    nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
    if (!wwatch)
      return nsnull;

    nsCOMPtr<nsIPrompt> prompter;
    wwatch->GetNewPrompter(0, getter_AddRefs(prompter));
    if (!prompter)
      return nsnull;

    NS_GetProxyForObject(NS_PROXY_TO_MAIN_THREAD,
                         NS_GET_IID(nsIPrompt),
                         prompter, NS_PROXY_SYNC,
                         getter_AddRefs(proxyPrompt));
    if (!proxyPrompt)
      return nsnull;
  }
  else
  {
    // The interface requestor object may not be safe, so
    // proxy the call to get the nsIPrompt.
  
    nsCOMPtr<nsIInterfaceRequestor> proxiedCallbacks;
    NS_GetProxyForObject(NS_PROXY_TO_MAIN_THREAD,
                         NS_GET_IID(nsIInterfaceRequestor),
                         ir,
                         NS_PROXY_SYNC,
                         getter_AddRefs(proxiedCallbacks));
  
    // Get the desired interface
    nsCOMPtr<nsIPrompt> prompt(do_GetInterface(proxiedCallbacks));
    if (!prompt) {
      NS_ASSERTION(PR_FALSE, "callbacks does not implement nsIPrompt");
      return nsnull;
    }
  
    // Finally, get a proxy for the nsIPrompt
    NS_GetProxyForObject(NS_PROXY_TO_MAIN_THREAD,
                         NS_GET_IID(nsIPrompt),
                         prompt,
                         NS_PROXY_SYNC,
                         getter_AddRefs(proxyPrompt));
  }

  if (PK11_ProtectedAuthenticationPath(slot))
    return ShowProtectedAuthPrompt(slot, ir);

  nsAutoString promptString;
  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));

  if (NS_FAILED(rv))
    return nsnull; 

  const PRUnichar* formatStrings[1] = { ToNewUnicode(NS_ConvertUTF8toUTF16(PK11_GetTokenName(slot))) };
  rv = nssComponent->PIPBundleFormatStringFromName("CertPassPrompt",
                                      formatStrings, 1,
                                      promptString);
  nsMemory::Free(const_cast<PRUnichar*>(formatStrings[0]));

  if (NS_FAILED(rv))
    return nsnull;

  {
    nsPSMUITracker tracker;
    if (tracker.isUIForbidden()) {
      rv = NS_ERROR_NOT_AVAILABLE;
    }
    else {
      PRBool checkState;
      rv = proxyPrompt->PromptPassword(nsnull, promptString.get(),
                                       &password, nsnull, &checkState, &value);
    }
  }
  
  if (NS_SUCCEEDED(rv) && value) {
    char* str = ToNewUTF8String(nsDependentString(password));
    NS_Free(password);
    return str;
  }

  return nsnull;
}

void PR_CALLBACK HandshakeCallback(PRFileDesc* fd, void* client_data) {
  nsNSSShutDownPreventionLock locker;
  PRInt32 sslStatus;
  char* signer = nsnull;
  char* cipherName = nsnull;
  PRInt32 keyLength;
  nsresult rv;
  PRInt32 encryptBits;

  if (SECSuccess != SSL_SecurityStatus(fd, &sslStatus, &cipherName, &keyLength,
                                       &encryptBits, &signer, nsnull)) {
    return;
  }

  PRInt32 secStatus;
  if (sslStatus == SSL_SECURITY_STATUS_OFF)
    secStatus = nsIWebProgressListener::STATE_IS_BROKEN;
  else if (encryptBits >= 90)
    secStatus = (nsIWebProgressListener::STATE_IS_SECURE |
                 nsIWebProgressListener::STATE_SECURE_HIGH);
  else
    secStatus = (nsIWebProgressListener::STATE_IS_SECURE |
                 nsIWebProgressListener::STATE_SECURE_LOW);

  PRBool siteSupportsSafeRenego;
  if (SSL_HandshakeNegotiatedExtension(fd, ssl_renegotiation_info_xtn, &siteSupportsSafeRenego) != SECSuccess
      || !siteSupportsSafeRenego) {

    PRBool wantWarning = (nsSSLIOLayerHelpers::getWarnLevelMissingRFC5746() > 0);

    nsNSSSocketInfo* infoObject = (nsNSSSocketInfo*) fd->higher->secret;
    nsCOMPtr<nsIConsoleService> console;
    if (infoObject && wantWarning) {
      console = do_GetService(NS_CONSOLESERVICE_CONTRACTID);
      if (console) {
        nsXPIDLCString hostName;
        infoObject->GetHostName(getter_Copies(hostName));

        nsAutoString msg;
        msg.Append(NS_ConvertASCIItoUTF16(hostName));
        msg.Append(NS_LITERAL_STRING(" : server does not support RFC 5746, see CVE-2009-3555"));

        console->LogStringMessage(msg.get());
      }
    }
    if (nsSSLIOLayerHelpers::treatUnsafeNegotiationAsBroken()) {
      secStatus = nsIWebProgressListener::STATE_IS_BROKEN;
    }
  }


  CERTCertificate *peerCert = SSL_PeerCertificate(fd);
  const char* caName = nsnull; // caName is a pointer only, no ownership
  char* certOrgName = CERT_GetOrgName(&peerCert->issuer);
  CERT_DestroyCertificate(peerCert);
  caName = certOrgName ? certOrgName : signer;

  const char* verisignName = "Verisign, Inc.";
  // If the CA name is RSA Data Security, then change the name to the real
  // name of the company i.e. VeriSign, Inc.
  if (nsCRT::strcmp((const char*)caName, "RSA Data Security, Inc.") == 0) {
    caName = verisignName;
  }

  nsAutoString shortDesc;
  const PRUnichar* formatStrings[1] = { ToNewUnicode(NS_ConvertUTF8toUTF16(caName)) };
  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));
  if (NS_SUCCEEDED(rv)) {
    rv = nssComponent->PIPBundleFormatStringFromName("SignedBy",
                                                   formatStrings, 1,
                                                   shortDesc);

    nsMemory::Free(const_cast<PRUnichar*>(formatStrings[0]));

    nsNSSSocketInfo* infoObject = (nsNSSSocketInfo*) fd->higher->secret;
    infoObject->SetSecurityState(secStatus);
    infoObject->SetShortSecurityDescription(shortDesc.get());

    /* Set the SSL Status information */
    nsRefPtr<nsSSLStatus> status = infoObject->SSLStatus();
    if (!status) {
      status = new nsSSLStatus();
      infoObject->SetSSLStatus(status);
    }

    nsSSLIOLayerHelpers::mHostsWithCertErrors->LookupCertErrorBits(
      infoObject, status);

    CERTCertificate *serverCert = SSL_PeerCertificate(fd);
    if (serverCert) {
      nsRefPtr<nsNSSCertificate> nssc = nsNSSCertificate::Create(serverCert);
      CERT_DestroyCertificate(serverCert);
      serverCert = nsnull;

      nsCOMPtr<nsIX509Cert> prevcert;
      infoObject->GetPreviousCert(getter_AddRefs(prevcert));

      PRBool equals_previous = PR_FALSE;
      if (prevcert && nssc) {
        nsresult rv = nssc->Equals(prevcert, &equals_previous);
        if (NS_FAILED(rv)) {
          equals_previous = PR_FALSE;
        }
      }

      if (equals_previous) {
        PR_LOG(gPIPNSSLog, PR_LOG_DEBUG,
               ("HandshakeCallback using PREV cert %p\n", prevcert.get()));
        infoObject->SetCert(prevcert);
        status->mServerCert = prevcert;
      }
      else {
        if (status->mServerCert) {
          PR_LOG(gPIPNSSLog, PR_LOG_DEBUG,
                 ("HandshakeCallback KEEPING cert %p\n", status->mServerCert.get()));
          infoObject->SetCert(status->mServerCert);
        }
        else {
          PR_LOG(gPIPNSSLog, PR_LOG_DEBUG,
                 ("HandshakeCallback using NEW cert %p\n", nssc.get()));
          infoObject->SetCert(nssc);
          status->mServerCert = nssc;
        }
      }
    }

    status->mHaveKeyLengthAndCipher = PR_TRUE;
    status->mKeyLength = keyLength;
    status->mSecretKeyLength = encryptBits;
    status->mCipherName.Assign(cipherName);
  }

  PORT_Free(cipherName);
  PR_FREEIF(certOrgName);
  PR_Free(signer);
}

struct nsSerialBinaryBlacklistEntry
{
  unsigned int len;
  const char *binary_serial;
};

// bug 642395
static struct nsSerialBinaryBlacklistEntry myUTNBlacklistEntries[] = {
  { 17, "\x00\x92\x39\xd5\x34\x8f\x40\xd1\x69\x5a\x74\x54\x70\xe1\xf2\x3f\x43" },
  { 17, "\x00\xd8\xf3\x5f\x4e\xb7\x87\x2b\x2d\xab\x06\x92\xe3\x15\x38\x2f\xb0" },
  { 16, "\x72\x03\x21\x05\xc5\x0c\x08\x57\x3d\x8e\xa5\x30\x4e\xfe\xe8\xb0" },
  { 17, "\x00\xb0\xb7\x13\x3e\xd0\x96\xf9\xb5\x6f\xae\x91\xc8\x74\xbd\x3a\xc0" },
  { 16, "\x39\x2a\x43\x4f\x0e\x07\xdf\x1f\x8a\xa3\x05\xde\x34\xe0\xc2\x29" },
  { 16, "\x3e\x75\xce\xd4\x6b\x69\x30\x21\x21\x88\x30\xae\x86\xa8\x2a\x71" },
  { 17, "\x00\xe9\x02\x8b\x95\x78\xe4\x15\xdc\x1a\x71\x0a\x2b\x88\x15\x44\x47" },
  { 17, "\x00\xd7\x55\x8f\xda\xf5\xf1\x10\x5b\xb2\x13\x28\x2b\x70\x77\x29\xa3" },
  { 16, "\x04\x7e\xcb\xe9\xfc\xa5\x5f\x7b\xd0\x9e\xae\x36\xe1\x0c\xae\x1e" },
  { 17, "\x00\xf5\xc8\x6a\xf3\x61\x62\xf1\x3a\x64\xf5\x4f\x6d\xc9\x58\x7c\x06" },
  { 0, 0 } // end marker
};

SECStatus PR_CALLBACK AuthCertificateCallback(void* client_data, PRFileDesc* fd,
                                              PRBool checksig, PRBool isServer) {
  nsNSSShutDownPreventionLock locker;

  CERTCertificate *serverCert = SSL_PeerCertificate(fd);
  CERTCertificateCleaner serverCertCleaner(serverCert);

  if (serverCert && 
      serverCert->serialNumber.data &&
      serverCert->issuerName &&
      !strcmp(serverCert->issuerName, 
        "CN=UTN-USERFirst-Hardware,OU=http://www.usertrust.com,O=The USERTRUST Network,L=Salt Lake City,ST=UT,C=US")) {

    unsigned char *server_cert_comparison_start = (unsigned char*)serverCert->serialNumber.data;
    unsigned int server_cert_comparison_len = serverCert->serialNumber.len;

    while (server_cert_comparison_len) {
      if (*server_cert_comparison_start != 0)
        break;

      ++server_cert_comparison_start;
      --server_cert_comparison_len;
    }

    nsSerialBinaryBlacklistEntry *walk = myUTNBlacklistEntries;
    for ( ; walk && walk->len; ++walk) {

      unsigned char *locked_cert_comparison_start = (unsigned char*)walk->binary_serial;
      unsigned int locked_cert_comparison_len = walk->len;
      
      while (locked_cert_comparison_len) {
        if (*locked_cert_comparison_start != 0)
          break;
        
        ++locked_cert_comparison_start;
        --locked_cert_comparison_len;
      }

      if (server_cert_comparison_len == locked_cert_comparison_len &&
          !memcmp(server_cert_comparison_start, locked_cert_comparison_start, locked_cert_comparison_len)) {
        PR_SetError(SEC_ERROR_REVOKED_CERTIFICATE, 0);
        return SECFailure;
      }
    }
  }
  
  // first the default action
  SECStatus rv = SSL_AuthCertificate(CERT_GetDefaultCertDB(), fd, checksig, isServer);

  // We want to remember the CA certs in the temp db, so that the application can find the
  // complete chain at any time it might need it.
  // But we keep only those CA certs in the temp db, that we didn't already know.

  if (serverCert) {
    nsNSSSocketInfo* infoObject = (nsNSSSocketInfo*) fd->higher->secret;
    nsRefPtr<nsSSLStatus> status = infoObject->SSLStatus();
    nsRefPtr<nsNSSCertificate> nsc;

    if (!status || !status->mServerCert) {
      nsc = nsNSSCertificate::Create(serverCert);
    }

    if (SECSuccess == rv) {
      if (nsc) {
        PRBool dummyIsEV;
        nsc->GetIsExtendedValidation(&dummyIsEV); // the nsc object will cache the status
      }
    
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
        char* nickname = nsNSSCertificate::defaultServerNickname(node->cert);
        if (nickname && *nickname) {
          PK11SlotInfo *slot = PK11_GetInternalKeySlot();
          if (slot) {
            PK11_ImportCert(slot, node->cert, CK_INVALID_HANDLE, 
                            nickname, PR_FALSE);
            PK11_FreeSlot(slot);
          }
        }
        PR_FREEIF(nickname);
      }

      CERT_DestroyCertList(certList);
    }

    // The connection may get terminated, for example, if the server requires
    // a client cert. Let's provide a minimal SSLStatus
    // to the caller that contains at least the cert and its status.
    if (!status) {
      status = new nsSSLStatus();
      infoObject->SetSSLStatus(status);
    }

    if (rv == SECSuccess) {
      // Certificate verification succeeded delete any potential record
      // of certificate error bits.
      nsSSLIOLayerHelpers::mHostsWithCertErrors->RememberCertHasError(
        infoObject, nsnull, rv);
    }
    else {
      // Certificate verification failed, update the status' bits.
      nsSSLIOLayerHelpers::mHostsWithCertErrors->LookupCertErrorBits(
        infoObject, status);
    }

    if (status && !status->mServerCert) {
      status->mServerCert = nsc;
      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG,
             ("AuthCertificateCallback setting NEW cert %p\n", status->mServerCert.get()));
    }
  }

  return rv;
}

struct OCSPDefaultResponders {
    const char *issuerName_string;
    CERTName *issuerName;
    const char *issuerKeyID_base64;
    SECItem *issuerKeyID;
    const char *ocspUrl;
};

static struct OCSPDefaultResponders myDefaultOCSPResponders[] = {
  /* COMODO */
  {
    "CN=AddTrust External CA Root,OU=AddTrust External TTP Network,O=AddTrust AB,C=SE",
    nsnull, "rb2YejS0Jvf6xCZU7wO94CTLVBo=", nsnull,
    "http://ocsp.comodoca.com"
  },
  {
    "CN=COMODO Certification Authority,O=COMODO CA Limited,L=Salford,ST=Greater Manchester,C=GB",
    nsnull, "C1jli8ZMFTekQKkwqSG+RzZaVv8=", nsnull,
    "http://ocsp.comodoca.com"
  },
  {
    "CN=COMODO EV SGC CA,O=COMODO CA Limited,L=Salford,ST=Greater Manchester,C=GB",
    nsnull, "f/ZMNigUrs0eN6/eWvJbw6CsK/4=", nsnull,
    "http://ocsp.comodoca.com"
  },
  {
    "CN=COMODO EV SSL CA,O=COMODO CA Limited,L=Salford,ST=Greater Manchester,C=GB",
    nsnull, "aRZJ7LZ1ZFrpAyNgL1RipTRcPuI=", nsnull,
    "http://ocsp.comodoca.com"
  },
  {
    "CN=UTN - DATACorp SGC,OU=http://www.usertrust.com,O=The USERTRUST Network,L=Salt Lake City,ST=UT,C=US",
    nsnull, "UzLRs89/+uDxoF2FTpLSnkUdtE8=", nsnull,
    "http://ocsp.usertrust.com"
  },
  {
    "CN=UTN-USERFirst-Hardware,OU=http://www.usertrust.com,O=The USERTRUST Network,L=Salt Lake City,ST=UT,C=US",
    nsnull, "oXJfJhsomEOVXQc31YWWnUvSw0U=", nsnull,
    "http://ocsp.usertrust.com"
  },
  /* Network Solutions */
  {
    "CN=Network Solutions Certificate Authority,O=Network Solutions L.L.C.,C=US",
    nsnull, "ITDJ+wDXTpjah6oq0KcusUAxp0w=", nsnull,
    "http://ocsp.netsolssl.com"
  },
  {
    "CN=Network Solutions EV SSL CA,O=Network Solutions L.L.C.,C=US",
    nsnull, "tk6FnYQfGx3UUolOB5Yt+d7xj8w=", nsnull,
    "http://ocsp.netsolssl.com"
  },
  /* GlobalSign */
  {
    "CN=GlobalSign Root CA,OU=Root CA,O=GlobalSign nv-sa,C=BE",
    nsnull, "YHtmGkUNl8qJUC99BM00qP/8/Us=", nsnull,
    "http://ocsp.globalsign.com/ExtendedSSLCACross"
  },
  {
    "CN=GlobalSign,O=GlobalSign,OU=GlobalSign Root CA - R2",
    nsnull, "m+IHV2ccHsBqBt5ZtJot39wZhi4=", nsnull,
    "http://ocsp.globalsign.com/ExtendedSSLCA"
  },
  {
    "CN=GlobalSign Extended Validation CA,O=GlobalSign,OU=Extended Validation CA",
    nsnull, "NLH5yYxrNUTMCGkK7uOjuVy/FuA=", nsnull,
    "http://ocsp.globalsign.com/ExtendedSSL"
  },
  /* Trustwave */
  {
    "CN=SecureTrust CA,O=SecureTrust Corporation,C=US",
    nsnull, "QjK2FvoE/f5dS3rD/fdMQB1aQ68=", nsnull,
    "http://ocsp.trustwave.com"
  }
};

static const unsigned int numResponders =
    (sizeof myDefaultOCSPResponders) / (sizeof myDefaultOCSPResponders[0]);

static CERT_StringFromCertFcn oldOCSPAIAInfoCallback = nsnull;

/*
 * See if we have a hard-coded default responder for this certificate's
 * issuer (unless this certificate is a root certificate).
 *
 * The result needs to be freed (PORT_Free) when no longer in use.
 */
char* PR_CALLBACK MyAlternateOCSPAIAInfoCallback(CERTCertificate *cert) {
  if (cert && !cert->isRoot) {
    unsigned int i;
    for (i=0; i < numResponders; i++) {
      if (!(myDefaultOCSPResponders[i].issuerName));
      else if (!(myDefaultOCSPResponders[i].issuerKeyID));
      else if (!(cert->authKeyID));
      else if (CERT_CompareName(myDefaultOCSPResponders[i].issuerName,
                                &(cert->issuer)) != SECEqual);
      else if (SECITEM_CompareItem(myDefaultOCSPResponders[i].issuerKeyID,
                                   &(cert->authKeyID->keyID)) != SECEqual);
      else        // Issuer Name and Key Identifier match, so use this OCSP URL.
        return PORT_Strdup(myDefaultOCSPResponders[i].ocspUrl);
    }
  }

  // If we've not found a hard-coded default responder, chain to the old
  // callback function (if there is one).
  if (oldOCSPAIAInfoCallback)
    return (*oldOCSPAIAInfoCallback)(cert);

  return nsnull;
}

void cleanUpMyDefaultOCSPResponders() {
  unsigned int i;

  for (i=0; i < numResponders; i++) {
    if (myDefaultOCSPResponders[i].issuerName) {
      CERT_DestroyName(myDefaultOCSPResponders[i].issuerName);
      myDefaultOCSPResponders[i].issuerName = nsnull;
    }
    if (myDefaultOCSPResponders[i].issuerKeyID) {
      SECITEM_FreeItem(myDefaultOCSPResponders[i].issuerKeyID, PR_TRUE);
      myDefaultOCSPResponders[i].issuerKeyID = nsnull;
    }
  }
}

SECStatus RegisterMyOCSPAIAInfoCallback() {
  // Prevent multiple registrations.
  if (myDefaultOCSPResponders[0].issuerName)
    return SECSuccess;                 // Already registered ok.

  // Populate various fields in the myDefaultOCSPResponders[] array.
  SECStatus rv = SECFailure;
  unsigned int i;
  for (i=0; i < numResponders; i++) {
    // Create a CERTName structure from the issuer name string.
    myDefaultOCSPResponders[i].issuerName = CERT_AsciiToName(
      const_cast<char*>(myDefaultOCSPResponders[i].issuerName_string));
    if (!(myDefaultOCSPResponders[i].issuerName))
      goto loser;
    // Create a SECItem from the Base64 authority key identifier keyID.
    myDefaultOCSPResponders[i].issuerKeyID = NSSBase64_DecodeBuffer(nsnull,
          nsnull, myDefaultOCSPResponders[i].issuerKeyID_base64,
          (PRUint32)PORT_Strlen(myDefaultOCSPResponders[i].issuerKeyID_base64));
    if (!(myDefaultOCSPResponders[i].issuerKeyID))
      goto loser;
  }

  // Register our alternate OCSP Responder URL lookup function.
  rv = CERT_RegisterAlternateOCSPAIAInfoCallBack(MyAlternateOCSPAIAInfoCallback,
                                                 &oldOCSPAIAInfoCallback);
  if (rv != SECSuccess)
    goto loser;

  return SECSuccess;

loser:
  cleanUpMyDefaultOCSPResponders();
  return rv;
}

SECStatus UnregisterMyOCSPAIAInfoCallback() {
  SECStatus rv;

  // Only allow unregistration if we're already registered.
  if (!(myDefaultOCSPResponders[0].issuerName))
    return SECFailure;

  // Unregister our alternate OCSP Responder URL lookup function.
  rv = CERT_RegisterAlternateOCSPAIAInfoCallBack(oldOCSPAIAInfoCallback,
                                                 nsnull);
  if (rv != SECSuccess)
    return rv;

  // Tidy up.
  oldOCSPAIAInfoCallback = nsnull;
  cleanUpMyDefaultOCSPResponders();
  return SECSuccess;
}
