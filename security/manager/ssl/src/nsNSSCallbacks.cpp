/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNSSCallbacks.h"

#include "mozilla/Telemetry.h"
#include "mozilla/TimeStamp.h"
#include "nsNSSComponent.h"
#include "nsNSSIOLayer.h"
#include "nsIWebProgressListener.h"
#include "nsProtectedAuthThread.h"
#include "nsITokenDialogs.h"
#include "nsIUploadChannel.h"
#include "nsIPrompt.h"
#include "nsProxyRelease.h"
#include "PSMRunnable.h"
#include "ScopedNSSTypes.h"
#include "nsContentUtils.h"
#include "nsIHttpChannelInternal.h"
#include "nsNetUtil.h"
#include "SharedSSLState.h"
#include "ssl.h"
#include "sslproto.h"

using namespace mozilla;
using namespace mozilla::psm;

static NS_DEFINE_CID(kNSSComponentCID, NS_NSSCOMPONENT_CID);

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
  bool mResponsibleForDoneSignal;
  TimeStamp mStartTime;
};

nsHTTPDownloadEvent::nsHTTPDownloadEvent()
:mResponsibleForDoneSignal(true)
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
  ios->NewChannel(mRequestSession->mURL, nullptr, nullptr, getter_AddRefs(chan));
  NS_ENSURE_STATE(chan);

  chan->SetLoadFlags(nsIRequest::LOAD_ANONYMOUS);

  // Create a loadgroup for this new channel.  This way if the channel
  // is redirected, we'll have a way to cancel the resulting channel.
  nsCOMPtr<nsILoadGroup> lg = do_CreateInstance(NS_LOADGROUP_CONTRACTID);
  chan->SetLoadGroup(lg);

  if (mRequestSession->mHasPostData)
  {
    nsCOMPtr<nsIInputStream> uploadStream;
    rv = NS_NewPostDataStream(getter_AddRefs(uploadStream),
                              false,
                              mRequestSession->mPostData);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIUploadChannel> uploadChannel(do_QueryInterface(chan));
    NS_ENSURE_STATE(uploadChannel);

    rv = uploadChannel->SetUploadStream(uploadStream, 
                                        mRequestSession->mPostContentType,
                                        -1);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Do not use SPDY for internal security operations. It could result
  // in the silent upgrade to ssl, which in turn could require an SSL
  // operation to fufill something like a CRL fetch, which is an
  // endless loop.
  nsCOMPtr<nsIHttpChannelInternal> internalChannel = do_QueryInterface(chan);
  if (internalChannel) {
    rv = internalChannel->SetAllowSpdy(false);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsIHttpChannel> hchan = do_QueryInterface(chan);
  NS_ENSURE_STATE(hchan);

  rv = hchan->SetRequestMethod(mRequestSession->mRequestMethod);
  NS_ENSURE_SUCCESS(rv, rv);

  mResponsibleForDoneSignal = false;
  mListener->mResponsibleForDoneSignal = true;

  mListener->mLoadGroup = lg.get();
  NS_ADDREF(mListener->mLoadGroup);
  mListener->mLoadGroupOwnerThread = PR_GetCurrentThread();

  rv = NS_NewStreamLoader(getter_AddRefs(mListener->mLoader), 
                          mListener);

  if (NS_SUCCEEDED(rv)) {
    mStartTime = TimeStamp::Now();
    rv = hchan->AsyncOpen(mListener->mLoader, nullptr);
  }

  if (NS_FAILED(rv)) {
    mListener->mResponsibleForDoneSignal = false;
    mResponsibleForDoneSignal = true;

    NS_RELEASE(mListener->mLoadGroup);
    mListener->mLoadGroup = nullptr;
    mListener->mLoadGroupOwnerThread = nullptr;
  }

  return NS_OK;
}

struct nsCancelHTTPDownloadEvent : nsRunnable {
  nsCOMPtr<nsHTTPListener> mListener;

  NS_IMETHOD Run() {
    mListener->FreeLoadGroup(true);
    mListener = nullptr;
    return NS_OK;
  }
};

SECStatus nsNSSHttpServerSession::createSessionFcn(const char *host,
                                                   uint16_t portnum,
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
  uint32_t maxBug404059Timeout = PR_TicksPerSecond() * 10;
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
                                                  const uint32_t http_data_len,
                                                  const char *http_content_type)
{
  mHasPostData = true;
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
  //                                  false)));
}

SECStatus nsNSSHttpRequestSession::trySendAndReceiveFcn(PRPollDesc **pPollDesc,
                                                        uint16_t *http_response_code, 
                                                        const char **http_response_content_type, 
                                                        const char **http_response_headers, 
                                                        const char **http_response_data, 
                                                        uint32_t *http_response_data_len)
{
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG,
         ("nsNSSHttpRequestSession::trySendAndReceiveFcn to %s\n", mURL.get()));

  bool onSTSThread;
  nsresult nrv;
  nsCOMPtr<nsIEventTarget> sts
    = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &nrv);
  if (NS_FAILED(nrv)) {
    NS_ERROR("Could not get STS service");
    PR_SetError(PR_INVALID_STATE_ERROR, 0);
    return SECFailure;
  }

  nrv = sts->IsOnCurrentThread(&onSTSThread);
  if (NS_FAILED(nrv)) {
    NS_ERROR("IsOnCurrentThread failed");
    PR_SetError(PR_INVALID_STATE_ERROR, 0);
    return SECFailure;
  }

  if (onSTSThread) {
    NS_ERROR("nsNSSHttpRequestSession::trySendAndReceiveFcn called on socket "
             "thread; this will not work.");
    PR_SetError(PR_INVALID_STATE_ERROR, 0);
    return SECFailure;
  }

  const int max_retries = 2;
  int retry_count = 0;
  bool retryable_error = false;
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
    retryable_error = false;

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
  ++mRefCount;
}

void
nsNSSHttpRequestSession::Release()
{
  int32_t newRefCount = --mRefCount;
  if (!newRefCount) {
    delete this;
  }
}

SECStatus
nsNSSHttpRequestSession::internal_send_receive_attempt(bool &retryable_error,
                                                       PRPollDesc **pPollDesc,
                                                       uint16_t *http_response_code,
                                                       const char **http_response_content_type,
                                                       const char **http_response_headers,
                                                       const char **http_response_data,
                                                       uint32_t *http_response_data_len)
{
  if (pPollDesc) *pPollDesc = nullptr;
  if (http_response_code) *http_response_code = 0;
  if (http_response_content_type) *http_response_content_type = 0;
  if (http_response_headers) *http_response_headers = 0;
  if (http_response_data) *http_response_data = 0;

  uint32_t acceptableResultSize = 0;

  if (http_response_data_len)
  {
    acceptableResultSize = *http_response_data_len;
    *http_response_data_len = 0;
  }
  
  if (!mListener)
    return SECFailure;

  Mutex& waitLock = mListener->mLock;
  CondVar& waitCondition = mListener->mCondition;
  volatile bool &waitFlag = mListener->mWaitFlag;
  waitFlag = true;

  RefPtr<nsHTTPDownloadEvent> event(new nsHTTPDownloadEvent);
  if (!event)
    return SECFailure;

  event->mListener = mListener;
  this->AddRef();
  event->mRequestSession = this;

  nsresult rv = NS_DispatchToMainThread(event);
  if (NS_FAILED(rv))
  {
    event->mResponsibleForDoneSignal = false;
    return SECFailure;
  }

  bool request_canceled = false;

  {
    MutexAutoLock locker(waitLock);

    const PRIntervalTime start_time = PR_IntervalNow();
    PRIntervalTime wait_interval;

    bool running_on_main_thread = NS_IsMainThread();
    if (running_on_main_thread)
    {
      // The result of running this on the main thread
      // is a series of small timeouts mixed with spinning the
      // event loop - this is always dangerous as there is so much main
      // thread code that does not expect to be called re-entrantly. Your
      // app really shouldn't do that.
      NS_WARNING("Security network blocking I/O on Main Thread");

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
        NS_ProcessNextEvent(nullptr);
      }

      waitCondition.Wait(wait_interval);
      
      if (!waitFlag)
        break;

      if (!request_canceled)
      {
        bool timeout = 
          (PRIntervalTime)(PR_IntervalNow() - start_time) > mTimeoutInterval;
 
        if (timeout)
        {
          request_canceled = true;

          RefPtr<nsCancelHTTPDownloadEvent> cancelevent(
            new nsCancelHTTPDownloadEvent);
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

  if (!event->mStartTime.IsNull()) {
    if (request_canceled) {
      Telemetry::Accumulate(Telemetry::CERT_VALIDATION_HTTP_REQUEST_RESULT, 0);
      Telemetry::AccumulateTimeDelta(
        Telemetry::CERT_VALIDATION_HTTP_REQUEST_CANCELED_TIME,
        event->mStartTime, TimeStamp::Now());
    }
    else if (NS_SUCCEEDED(mListener->mResultCode) &&
             mListener->mHttpResponseCode == 200) {
      Telemetry::Accumulate(Telemetry::CERT_VALIDATION_HTTP_REQUEST_RESULT, 1);
      Telemetry::AccumulateTimeDelta(
        Telemetry::CERT_VALIDATION_HTTP_REQUEST_SUCCEEDED_TIME,
        event->mStartTime, TimeStamp::Now());
    }
    else {
      Telemetry::Accumulate(Telemetry::CERT_VALIDATION_HTTP_REQUEST_RESULT, 2);
      Telemetry::AccumulateTimeDelta(
        Telemetry::CERT_VALIDATION_HTTP_REQUEST_FAILED_TIME,
        event->mStartTime, TimeStamp::Now());
    }
  }
  else {
    Telemetry::Accumulate(Telemetry::CERT_VALIDATION_HTTP_REQUEST_RESULT, 3);
  }

  if (request_canceled)
    return SECFailure;

  if (NS_FAILED(mListener->mResultCode))
  {
    if (mListener->mResultCode == NS_ERROR_CONNECTION_REFUSED
        ||
        mListener->mResultCode == NS_ERROR_NET_RESET)
    {
      retryable_error = true;
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
  mHasPostData(false),
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
  SEC_RegisterDefaultHttpClient(nullptr);
}

nsHTTPListener::nsHTTPListener()
: mResultData(nullptr),
  mResultLen(0),
  mLock("nsHTTPListener.mLock"),
  mCondition(mLock, "nsHTTPListener.mCondition"),
  mWaitFlag(true),
  mResponsibleForDoneSignal(false),
  mLoadGroup(nullptr),
  mLoadGroupOwnerThread(nullptr)
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

NS_IMPL_ISUPPORTS1(nsHTTPListener, nsIStreamLoaderObserver)

void
nsHTTPListener::FreeLoadGroup(bool aCancelLoad)
{
  nsILoadGroup *lg = nullptr;

  MutexAutoLock locker(mLock);

  if (mLoadGroup) {
    if (mLoadGroupOwnerThread != PR_GetCurrentThread()) {
      NS_ASSERTION(false,
                   "attempt to access nsHTTPDownloadEvent::mLoadGroup on multiple threads, leaking it!");
    }
    else {
      lg = mLoadGroup;
      mLoadGroup = nullptr;
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
                                 uint32_t stringLen,
                                 const uint8_t* string)
{
  mResultCode = aStatus;

  FreeLoadGroup(false);

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
      mHttpRequestSucceeded = false;

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
  mResponsibleForDoneSignal = false;

  {
    MutexAutoLock locker(mLock);
    mWaitFlag = false;
    mCondition.NotifyAll();
  }
}

static char*
ShowProtectedAuthPrompt(PK11SlotInfo* slot, nsIInterfaceRequestor *ir)
{
  if (!NS_IsMainThread()) {
    NS_ERROR("ShowProtectedAuthPrompt called off the main thread");
    return nullptr;
  }

  char* protAuthRetVal = nullptr;

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
                  protAuthRetVal = nullptr;
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

class PK11PasswordPromptRunnable : public SyncRunnableBase
{
public:
  PK11PasswordPromptRunnable(PK11SlotInfo* slot, 
                             nsIInterfaceRequestor* ir)
    : mResult(nullptr),
      mSlot(slot),
      mIR(ir)
  {
  }
  char * mResult; // out
  virtual void RunOnTargetThread();
private:
  PK11SlotInfo* const mSlot; // in
  nsIInterfaceRequestor* const mIR; // in
};

void PK11PasswordPromptRunnable::RunOnTargetThread()
{
  nsNSSShutDownPreventionLock locker;
  nsresult rv = NS_OK;
  PRUnichar *password = nullptr;
  bool value = false;
  nsCOMPtr<nsIPrompt> prompt;

  /* TODO: Retry should generate a different dialog message */
/*
  if (retry)
    return nullptr;
*/

  if (!mIR)
  {
    nsNSSComponent::GetNewPrompter(getter_AddRefs(prompt));
  }
  else
  {
    prompt = do_GetInterface(mIR);
    NS_ASSERTION(prompt, "callbacks does not implement nsIPrompt");
  }

  if (!prompt)
    return;

  if (PK11_ProtectedAuthenticationPath(mSlot)) {
    mResult = ShowProtectedAuthPrompt(mSlot, mIR);
    return;
  }

  nsAutoString promptString;
  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));

  if (NS_FAILED(rv))
    return; 

  const PRUnichar* formatStrings[1] = { 
    ToNewUnicode(NS_ConvertUTF8toUTF16(PK11_GetTokenName(mSlot)))
  };
  rv = nssComponent->PIPBundleFormatStringFromName("CertPassPrompt",
                                      formatStrings, 1,
                                      promptString);
  nsMemory::Free(const_cast<PRUnichar*>(formatStrings[0]));

  if (NS_FAILED(rv))
    return;

  {
    nsPSMUITracker tracker;
    if (tracker.isUIForbidden()) {
      rv = NS_ERROR_NOT_AVAILABLE;
    }
    else {
      // Although the exact value is ignored, we must not pass invalid
      // bool values through XPConnect.
      bool checkState = false;
      rv = prompt->PromptPassword(nullptr, promptString.get(),
                                  &password, nullptr, &checkState, &value);
    }
  }
  
  if (NS_SUCCEEDED(rv) && value) {
    mResult = ToNewUTF8String(nsDependentString(password));
    NS_Free(password);
  }
}

char*
PK11PasswordPrompt(PK11SlotInfo* slot, PRBool retry, void* arg)
{
  RefPtr<PK11PasswordPromptRunnable> runnable(
    new PK11PasswordPromptRunnable(slot,
                                   static_cast<nsIInterfaceRequestor*>(arg)));
  runnable->DispatchToMainThreadAndWait();
  return runnable->mResult;
}

// call with shutdown prevention lock held
static void
PreliminaryHandshakeDone(PRFileDesc* fd)
{
  nsNSSSocketInfo* infoObject = (nsNSSSocketInfo*) fd->higher->secret;
  if (!infoObject)
    return;

  if (infoObject->IsPreliminaryHandshakeDone())
    return;

  infoObject->SetPreliminaryHandshakeDone();
  infoObject->SetFirstServerHelloReceived();

  // Get the NPN value.
  SSLNextProtoState state;
  unsigned char npnbuf[256];
  unsigned int npnlen;

  if (SSL_GetNextProto(fd, &state, npnbuf, &npnlen, 256) == SECSuccess) {
    if (state == SSL_NEXT_PROTO_NEGOTIATED) {
      infoObject->SetNegotiatedNPN(reinterpret_cast<char *>(npnbuf), npnlen);
    }
    else {
      infoObject->SetNegotiatedNPN(nullptr, 0);
    }
    mozilla::Telemetry::Accumulate(Telemetry::SSL_NPN_TYPE, state);
  }
  else {
    infoObject->SetNegotiatedNPN(nullptr, 0);
  }
}

SECStatus
CanFalseStartCallback(PRFileDesc* fd, void* client_data, PRBool *canFalseStart)
{
  *canFalseStart = false;

  nsNSSShutDownPreventionLock locker;

  nsNSSSocketInfo* infoObject = (nsNSSSocketInfo*) fd->higher->secret;
  if (!infoObject) {
    PR_SetError(PR_INVALID_STATE_ERROR, 0);
    return SECFailure;
  }

  if (infoObject->isAlreadyShutDown()) {
    MOZ_CRASH("SSL socket used after NSS shut down");
    PR_SetError(PR_INVALID_STATE_ERROR, 0);
    return SECFailure;
  }

  PreliminaryHandshakeDone(fd);

  SSLChannelInfo channelInfo;
  if (SSL_GetChannelInfo(fd, &channelInfo, sizeof(channelInfo)) != SECSuccess) {
    return SECSuccess;
  }

  SSLCipherSuiteInfo cipherInfo;
  if (SSL_GetCipherSuiteInfo(channelInfo.cipherSuite, &cipherInfo,
                             sizeof (cipherInfo)) != SECSuccess) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("CanFalseStartCallback [%p] failed - "
                                      " KEA %d\n", fd,
                                      static_cast<int32_t>(cipherInfo.keaType)));
    return SECSuccess;
  }

  if (channelInfo.protocolVersion < SSL_LIBRARY_VERSION_TLS_1_0) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("CanFalseStartCallback [%p] failed - "
                                      "SSL Version must be >= TLS1 %x\n", fd,
                                      static_cast<int32_t>(channelInfo.protocolVersion)));
    return SECSuccess;
  }

  // never do false start without one of these key exchange algorithms
  if (cipherInfo.keaType != ssl_kea_rsa &&
      cipherInfo.keaType != ssl_kea_dh &&
      cipherInfo.keaType != ssl_kea_ecdh) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("CanFalseStartCallback [%p] failed - "
                                      "unsupported KEA %d\n", fd,
                                      static_cast<int32_t>(cipherInfo.keaType)));
    return SECSuccess;
  }

  // never do false start without at least 80 bits of key material. This should
  // be redundant to an NSS precondition
  if (cipherInfo.effectiveKeyBits < 80) {
    MOZ_CRASH("NSS is not enforcing the precondition that the effective "
              "key size must be >= 80 bits for false start");
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("CanFalseStartCallback [%p] failed - "
                                      "key too small %d\n", fd,
                                      static_cast<int32_t>(cipherInfo.effectiveKeyBits)));
    PR_SetError(PR_INVALID_STATE_ERROR, 0);
    return SECFailure;
  }

  // XXX: An attacker can choose which protocols are advertised in the
  // NPN extension. TODO(Bug 861311): We should restrict the ability
  // of an attacker leverage this capability by restricting false start
  // to the same protocol we previously saw for the server, after the
  // first successful connection to the server.

  // Enforce NPN to do false start if policy requires it. Do this as an
  // indicator if server compatibility.
  nsSSLIOLayerHelpers& helpers = infoObject->SharedState().IOLayerHelpers();
  if (helpers.mFalseStartRequireNPN) {
    nsAutoCString negotiatedNPN;
    if (NS_FAILED(infoObject->GetNegotiatedNPN(negotiatedNPN)) ||
        !negotiatedNPN.Length()) {
      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("CanFalseStartCallback [%p] failed - "
                                        "NPN cannot be verified\n", fd));
      return SECSuccess;
    }
  }

  // If we're not using eliptical curve kea then make sure we've seen the
  // same kea from this host in the past, to limit the potential for downgrade
  // attacks.
  if (cipherInfo.keaType != ssl_kea_ecdh) {

    if (helpers.mFalseStartRequireForwardSecrecy) {
      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("CanFalseStartCallback [%p] failed - "
                                        "KEA used is %d, but "
                                        "require-forward-secrecy configured.\n",
                                        fd, static_cast<int32_t>(cipherInfo.keaType)));
      return SECSuccess;
    }

    int16_t expected = infoObject->GetKEAExpected();
    if (cipherInfo.keaType != expected) {
      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("CanFalseStartCallback [%p] failed - "
                                        "KEA used is %d, expected %d\n", fd,
                                        static_cast<int32_t>(cipherInfo.keaType),
                                        static_cast<int32_t>(expected)));
      return SECSuccess;
    }

    // whitelist the expected key exchange algorithms that are
    // acceptable for false start when seen before.
    if (expected != ssl_kea_rsa && expected != ssl_kea_dh &&
        expected != ssl_kea_ecdh) {
      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("CanFalseStartCallback [%p] failed - "
                                        "KEA used, %d, "
                                        "is not supported with False Start.\n",
                                        fd, static_cast<int32_t>(expected)));
      return SECSuccess;
    }
  }

  // If we're not using AES then verify that this is the historically expected
  // symmetrical cipher for this host, to limit potential for downgrade attacks.
  if (cipherInfo.symCipher != ssl_calg_aes) {
    int16_t expected = infoObject->GetSymmetricCipherExpected();
    if (cipherInfo.symCipher != expected) {
      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("CanFalseStartCallback [%p] failed - "
                                        "Symmetric cipher used is %d, expected %d\n",
                                        fd, static_cast<int32_t>(cipherInfo.symCipher),
                                        static_cast<int32_t>(expected)));
      return SECSuccess;
    }

    // whitelist the expected ciphers that are
    // acceptable for false start when seen before.
    if ((expected != ssl_calg_rc4) && (expected != ssl_calg_3des) &&
        (expected != ssl_calg_aes)) {
      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("CanFalseStartCallback [%p] failed - "
                                        "Symmetric cipher used, %d, "
                                        "is not supported with False Start.\n",
                                        fd, static_cast<int32_t>(expected)));
      return SECSuccess;
    }
  }

  infoObject->NoteTimeUntilReady();
  *canFalseStart = true;
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("CanFalseStartCallback [%p] ok\n", fd));
  return SECSuccess;
}

void HandshakeCallback(PRFileDesc* fd, void* client_data) {
  nsNSSShutDownPreventionLock locker;
  SECStatus rv;

  nsNSSSocketInfo* infoObject = (nsNSSSocketInfo*) fd->higher->secret;

  // certificate validation sets FirstServerHelloReceived, so if that flag
  // is absent at handshake time we have a resumed session. Check this before
  // PreliminaryHandshakeDone() because that function also sets that flag.
  bool isResumedSession = !(infoObject->GetFirstServerHelloReceived());

  // Do the bookkeeping that needs to be done after the
  // server's ServerHello...ServerHelloDone have been processed, but that doesn't
  // need the handshake to be completed.
  PreliminaryHandshakeDone(fd);

  // If the handshake completed, then we know the site is TLS tolerant (if this
  // was a TLS connection).
  nsSSLIOLayerHelpers& ioLayerHelpers = infoObject->SharedState().IOLayerHelpers();
  ioLayerHelpers.rememberTolerantSite(infoObject);

  PRBool siteSupportsSafeRenego;
  rv = SSL_HandshakeNegotiatedExtension(fd, ssl_renegotiation_info_xtn,
                                        &siteSupportsSafeRenego);
  MOZ_ASSERT(rv == SECSuccess);
  if (rv != SECSuccess) {
    siteSupportsSafeRenego = false;
  }

  if (siteSupportsSafeRenego ||
      !ioLayerHelpers.treatUnsafeNegotiationAsBroken()) {
    infoObject->SetSecurityState(nsIWebProgressListener::STATE_IS_SECURE |
                                 nsIWebProgressListener::STATE_SECURE_HIGH);
  } else {
    infoObject->SetSecurityState(nsIWebProgressListener::STATE_IS_BROKEN);
  }

  // XXX Bug 883674: We shouldn't be formatting messages here in PSM; instead,
  // we should set a flag on the channel that higher (UI) level code can check
  // to log the warning. In particular, these warnings should go to the web
  // console instead of to the error console. Also, the warning is not
  // localized.
  if (!siteSupportsSafeRenego &&
      ioLayerHelpers.getWarnLevelMissingRFC5746() > 0) {
    nsXPIDLCString hostName;
    infoObject->GetHostName(getter_Copies(hostName));

    nsAutoString msg;
    msg.Append(NS_ConvertASCIItoUTF16(hostName));
    msg.Append(NS_LITERAL_STRING(" : server does not support RFC 5746, see CVE-2009-3555"));

    nsContentUtils::LogSimpleConsoleError(msg, "SSL");
  }

  ScopedCERTCertificate serverCert(SSL_PeerCertificate(fd));

  /* Set the SSL Status information */
  RefPtr<nsSSLStatus> status(infoObject->SSLStatus());
  if (!status) {
    status = new nsSSLStatus();
    infoObject->SetSSLStatus(status);
  }

  RememberCertErrorsTable::GetInstance().LookupCertErrorBits(infoObject,
                                                             status);

  RefPtr<nsNSSCertificate> nssc(nsNSSCertificate::Create(serverCert));
  nsCOMPtr<nsIX509Cert> prevcert;
  infoObject->GetPreviousCert(getter_AddRefs(prevcert));

  bool equals_previous = false;
  if (prevcert && nssc) {
    nsresult rv = nssc->Equals(prevcert, &equals_previous);
    if (NS_FAILED(rv)) {
      equals_previous = false;
    }
  }

  if (equals_previous) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG,
            ("HandshakeCallback using PREV cert %p\n", prevcert.get()));
    status->mServerCert = prevcert;
  }
  else {
    if (status->mServerCert) {
      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG,
              ("HandshakeCallback KEEPING cert %p\n", status->mServerCert.get()));
    }
    else {
      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG,
              ("HandshakeCallback using NEW cert %p\n", nssc.get()));
      status->mServerCert = nssc;
    }
  }

  SSLChannelInfo channelInfo;
  rv = SSL_GetChannelInfo(fd, &channelInfo, sizeof(channelInfo));
  MOZ_ASSERT(rv == SECSuccess);
  if (rv == SECSuccess) {
    // Get the protocol version for telemetry
    // 0=ssl3, 1=tls1, 2=tls1.1, 3=tls1.2
    unsigned int versionEnum = channelInfo.protocolVersion & 0xFF;
    Telemetry::Accumulate(Telemetry::SSL_HANDSHAKE_VERSION, versionEnum);

    SSLCipherSuiteInfo cipherInfo;
    rv = SSL_GetCipherSuiteInfo(channelInfo.cipherSuite, &cipherInfo,
                                sizeof cipherInfo);
    MOZ_ASSERT(rv == SECSuccess);
    if (rv == SECSuccess) {
      status->mHaveKeyLengthAndCipher = true;
      status->mKeyLength = cipherInfo.symKeyBits;
      status->mSecretKeyLength = cipherInfo.effectiveKeyBits;
      status->mCipherName.Assign(cipherInfo.cipherSuiteName);

      // keyExchange null=0, rsa=1, dh=2, fortezza=3, ecdh=4
      Telemetry::Accumulate(Telemetry::SSL_KEY_EXCHANGE_ALGORITHM,
                            cipherInfo.keaType);
      infoObject->SetKEAUsed(cipherInfo.keaType);
      infoObject->SetSymmetricCipherUsed(cipherInfo.symCipher);
    }
  }

  infoObject->NoteTimeUntilReady();
  infoObject->SetHandshakeCompleted(isResumedSession);
}
