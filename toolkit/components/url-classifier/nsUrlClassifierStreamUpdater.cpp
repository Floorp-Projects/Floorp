//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCRT.h"
#include "nsIHttpChannel.h"
#include "nsIObserverService.h"
#include "nsIStringStream.h"
#include "nsIUploadChannel.h"
#include "nsIURI.h"
#include "nsIUrlClassifierDBService.h"
#include "nsNetUtil.h"
#include "nsStreamUtils.h"
#include "nsStringStream.h"
#include "nsToolkitCompsCID.h"
#include "nsUrlClassifierStreamUpdater.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/ErrorNames.h"
#include "mozilla/Logging.h"
#include "nsIInterfaceRequestor.h"
#include "mozilla/LoadContext.h"
#include "nsContentUtils.h"

static const char* gQuitApplicationMessage = "quit-application";

#undef LOG

// NSPR_LOG_MODULES=UrlClassifierStreamUpdater:5
static const PRLogModuleInfo *gUrlClassifierStreamUpdaterLog = nullptr;
#define LOG(args) MOZ_LOG(gUrlClassifierStreamUpdaterLog, mozilla::LogLevel::Debug, args)

// This class does absolutely nothing, except pass requests onto the DBService.

///////////////////////////////////////////////////////////////////////////////
// nsIUrlClassiferStreamUpdater implementation
// Handles creating/running the stream listener

nsUrlClassifierStreamUpdater::nsUrlClassifierStreamUpdater()
  : mIsUpdating(false), mInitialized(false), mDownloadError(false),
    mBeganStream(false), mChannel(nullptr)
{
  if (!gUrlClassifierStreamUpdaterLog)
    gUrlClassifierStreamUpdaterLog = PR_NewLogModule("UrlClassifierStreamUpdater");
  LOG(("nsUrlClassifierStreamUpdater init [this=%p]", this));
}

NS_IMPL_ISUPPORTS(nsUrlClassifierStreamUpdater,
                  nsIUrlClassifierStreamUpdater,
                  nsIUrlClassifierUpdateObserver,
                  nsIRequestObserver,
                  nsIStreamListener,
                  nsIObserver,
                  nsIInterfaceRequestor,
                  nsITimerCallback)

/**
 * Clear out the update.
 */
void
nsUrlClassifierStreamUpdater::DownloadDone()
{
  LOG(("nsUrlClassifierStreamUpdater::DownloadDone [this=%p]", this));
  mIsUpdating = false;

  mPendingUpdates.Clear();
  mDownloadError = false;
  mSuccessCallback = nullptr;
  mUpdateErrorCallback = nullptr;
  mDownloadErrorCallback = nullptr;
}

///////////////////////////////////////////////////////////////////////////////
// nsIUrlClassifierStreamUpdater implementation

nsresult
nsUrlClassifierStreamUpdater::FetchUpdate(nsIURI *aUpdateUrl,
                                          const nsACString & aRequestBody,
                                          const nsACString & aStreamTable)
{

#ifdef DEBUG
  {
    nsCString spec;
    aUpdateUrl->GetSpec(spec);
    LOG(("Fetching update %s from %s", aRequestBody.Data(), spec.get()));
  }
#endif

  nsresult rv;
  uint32_t loadFlags = nsIChannel::INHIBIT_CACHING |
                       nsIChannel::LOAD_BYPASS_CACHE;
  rv = NS_NewChannel(getter_AddRefs(mChannel),
                     aUpdateUrl,
                     nsContentUtils::GetSystemPrincipal(),
                     nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
                     nsIContentPolicy::TYPE_OTHER,
                     nullptr,  // aLoadGroup
                     this,     // aInterfaceRequestor
                     loadFlags);

  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsILoadInfo> loadInfo = mChannel->GetLoadInfo();
  loadInfo->SetOriginAttributes(mozilla::OriginAttributes(NECKO_SAFEBROWSING_APP_ID, false));

  mBeganStream = false;

  // If aRequestBody is empty, construct it for the test.
  if (!aRequestBody.IsEmpty()) {
    rv = AddRequestBody(aRequestBody);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Set the appropriate content type for file/data URIs, for unit testing
  // purposes.
  // This is only used for testing and should be deleted.
  bool match;
  if ((NS_SUCCEEDED(aUpdateUrl->SchemeIs("file", &match)) && match) ||
      (NS_SUCCEEDED(aUpdateUrl->SchemeIs("data", &match)) && match)) {
    mChannel->SetContentType(NS_LITERAL_CSTRING("application/vnd.google.safebrowsing-update"));
  } else {
    // We assume everything else is an HTTP request.

    // Disable keepalive.
    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(mChannel, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = httpChannel->SetRequestHeader(NS_LITERAL_CSTRING("Connection"), NS_LITERAL_CSTRING("close"), false);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Create a custom LoadContext for SafeBrowsing, so we can use callbacks on
  // the channel to query the appId which allows separation of safebrowsing
  // cookies in a separate jar.
  nsCOMPtr<nsIInterfaceRequestor> sbContext =
    new mozilla::LoadContext(NECKO_SAFEBROWSING_APP_ID);
  rv = mChannel->SetNotificationCallbacks(sbContext);
  NS_ENSURE_SUCCESS(rv, rv);

  // Make the request.
  rv = mChannel->AsyncOpen2(this);
  NS_ENSURE_SUCCESS(rv, rv);

  mStreamTable = aStreamTable;

  return NS_OK;
}

nsresult
nsUrlClassifierStreamUpdater::FetchUpdate(const nsACString & aUpdateUrl,
                                          const nsACString & aRequestBody,
                                          const nsACString & aStreamTable)
{
  LOG(("(pre) Fetching update from %s\n", PromiseFlatCString(aUpdateUrl).get()));

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aUpdateUrl);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString urlSpec;
  uri->GetAsciiSpec(urlSpec);

  LOG(("(post) Fetching update from %s\n", urlSpec.get()));

  return FetchUpdate(uri, aRequestBody, aStreamTable);
}

NS_IMETHODIMP
nsUrlClassifierStreamUpdater::DownloadUpdates(
  const nsACString &aRequestTables,
  const nsACString &aRequestBody,
  const nsACString &aUpdateUrl,
  nsIUrlClassifierCallback *aSuccessCallback,
  nsIUrlClassifierCallback *aUpdateErrorCallback,
  nsIUrlClassifierCallback *aDownloadErrorCallback,
  bool *_retval)
{
  NS_ENSURE_ARG(aSuccessCallback);
  NS_ENSURE_ARG(aUpdateErrorCallback);
  NS_ENSURE_ARG(aDownloadErrorCallback);

  if (mIsUpdating) {
    LOG(("Already updating, queueing update %s from %s", aRequestBody.Data(),
         aUpdateUrl.Data()));
    *_retval = false;
    PendingRequest *request = mPendingRequests.AppendElement();
    request->mTables = aRequestTables;
    request->mRequest = aRequestBody;
    request->mUrl = aUpdateUrl;
    request->mSuccessCallback = aSuccessCallback;
    request->mUpdateErrorCallback = aUpdateErrorCallback;
    request->mDownloadErrorCallback = aDownloadErrorCallback;
    return NS_OK;
  }

  if (aUpdateUrl.IsEmpty()) {
    NS_ERROR("updateUrl not set");
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsresult rv;

  if (!mInitialized) {
    // Add an observer for shutdown so we can cancel any pending list
    // downloads.  quit-application is the same event that the download
    // manager listens for and uses to cancel pending downloads.
    nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
    if (!observerService)
      return NS_ERROR_FAILURE;

    observerService->AddObserver(this, gQuitApplicationMessage, false);

    mDBService = do_GetService(NS_URLCLASSIFIERDBSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    mInitialized = true;
  }

  rv = mDBService->BeginUpdate(this, aRequestTables);
  if (rv == NS_ERROR_NOT_AVAILABLE) {
    LOG(("Service busy, already updating, queuing update %s from %s",
         aRequestBody.Data(), aUpdateUrl.Data()));
    *_retval = false;
    PendingRequest *request = mPendingRequests.AppendElement();
    request->mTables = aRequestTables;
    request->mRequest = aRequestBody;
    request->mUrl = aUpdateUrl;
    request->mSuccessCallback = aSuccessCallback;
    request->mUpdateErrorCallback = aUpdateErrorCallback;
    request->mDownloadErrorCallback = aDownloadErrorCallback;
    return NS_OK;
  }

  if (NS_FAILED(rv)) {
    return rv;
  }

  mSuccessCallback = aSuccessCallback;
  mUpdateErrorCallback = aUpdateErrorCallback;
  mDownloadErrorCallback = aDownloadErrorCallback;

  mIsUpdating = true;
  *_retval = true;

  LOG(("FetchUpdate: %s", aUpdateUrl.Data()));
  //LOG(("requestBody: %s", aRequestBody.Data()));

  return FetchUpdate(aUpdateUrl, aRequestBody, EmptyCString());
}

///////////////////////////////////////////////////////////////////////////////
// nsIUrlClassifierUpdateObserver implementation

NS_IMETHODIMP
nsUrlClassifierStreamUpdater::UpdateUrlRequested(const nsACString &aUrl,
                                                 const nsACString &aTable)
{
  LOG(("Queuing requested update from %s\n", PromiseFlatCString(aUrl).get()));

  PendingUpdate *update = mPendingUpdates.AppendElement();
  if (!update)
    return NS_ERROR_OUT_OF_MEMORY;

  // Allow data: and file: urls for unit testing purposes, otherwise assume http
  if (StringBeginsWith(aUrl, NS_LITERAL_CSTRING("data:")) ||
      StringBeginsWith(aUrl, NS_LITERAL_CSTRING("file:"))) {
    update->mUrl = aUrl;
  } else {
    // For unittesting update urls to localhost should use http, not https
    // (otherwise the connection will fail silently, since there will be no
    // cert available).
    if (!StringBeginsWith(aUrl, NS_LITERAL_CSTRING("localhost"))) {
      update->mUrl = NS_LITERAL_CSTRING("https://") + aUrl;
    } else {
      update->mUrl = NS_LITERAL_CSTRING("http://") + aUrl;
    }
  }
  update->mTable = aTable;

  return NS_OK;
}

nsresult
nsUrlClassifierStreamUpdater::FetchNext()
{
  if (mPendingUpdates.Length() == 0) {
    return NS_OK;
  }

  PendingUpdate &update = mPendingUpdates[0];
  LOG(("Fetching update url: %s\n", update.mUrl.get()));
  nsresult rv = FetchUpdate(update.mUrl, EmptyCString(),
                            update.mTable);
  if (NS_FAILED(rv)) {
    LOG(("Error fetching update url: %s\n", update.mUrl.get()));
    // We can commit the urls that we've applied so far.  This is
    // probably a transient server problem, so trigger backoff.
    mDownloadErrorCallback->HandleEvent(EmptyCString());
    mDownloadError = true;
    mDBService->FinishUpdate();
    return rv;
  }

  mPendingUpdates.RemoveElementAt(0);

  return NS_OK;
}

nsresult
nsUrlClassifierStreamUpdater::FetchNextRequest()
{
  if (mPendingRequests.Length() == 0) {
    LOG(("No more requests, returning"));
    return NS_OK;
  }

  PendingRequest &request = mPendingRequests[0];
  LOG(("Stream updater: fetching next request: %s, %s",
       request.mTables.get(), request.mUrl.get()));
  bool dummy;
  DownloadUpdates(
    request.mTables,
    request.mRequest,
    request.mUrl,
    request.mSuccessCallback,
    request.mUpdateErrorCallback,
    request.mDownloadErrorCallback,
    &dummy);
  request.mSuccessCallback = nullptr;
  request.mUpdateErrorCallback = nullptr;
  request.mDownloadErrorCallback = nullptr;
  mPendingRequests.RemoveElementAt(0);

  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierStreamUpdater::StreamFinished(nsresult status,
                                             uint32_t requestedDelay)
{
  // We are a service and may not be reset with Init between calls, so reset
  // mBeganStream manually.
  mBeganStream = false;
  LOG(("nsUrlClassifierStreamUpdater::StreamFinished [%x, %d]", status, requestedDelay));
  if (NS_FAILED(status) || mPendingUpdates.Length() == 0) {
    // We're done.
    LOG(("nsUrlClassifierStreamUpdater::Done [this=%p]", this));
    mDBService->FinishUpdate();
    return NS_OK;
  }

  // Wait the requested amount of time before starting a new stream.
  // This appears to be a duplicate timer (see bug 1110891)
  nsresult rv;
  mTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
  if (NS_SUCCEEDED(rv)) {
    rv = mTimer->InitWithCallback(this, requestedDelay,
                                  nsITimer::TYPE_ONE_SHOT);
  }

  if (NS_FAILED(rv)) {
    NS_WARNING("Unable to initialize timer, fetching next safebrowsing item immediately");
    return FetchNext();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierStreamUpdater::UpdateSuccess(uint32_t requestedTimeout)
{
  LOG(("nsUrlClassifierStreamUpdater::UpdateSuccess [this=%p]", this));
  if (mPendingUpdates.Length() != 0) {
    NS_WARNING("Didn't fetch all safebrowsing update redirects");
  }

  // DownloadDone() clears mSuccessCallback, so we save it off here.
  nsCOMPtr<nsIUrlClassifierCallback> successCallback = mDownloadError ? nullptr : mSuccessCallback.get();
  DownloadDone();

  nsAutoCString strTimeout;
  strTimeout.AppendInt(requestedTimeout);
  if (successCallback) {
    LOG(("nsUrlClassifierStreamUpdater::UpdateSuccess callback [this=%p]",
         this));
    successCallback->HandleEvent(strTimeout);
  } else {
    LOG(("nsUrlClassifierStreamUpdater::UpdateSuccess skipping callback [this=%p]",
         this));
  }
  // Now fetch the next request
  LOG(("stream updater: calling into fetch next request"));
  FetchNextRequest();

  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierStreamUpdater::UpdateError(nsresult result)
{
  LOG(("nsUrlClassifierStreamUpdater::UpdateError [this=%p]", this));

  // DownloadDone() clears mUpdateErrorCallback, so we save it off here.
  nsCOMPtr<nsIUrlClassifierCallback> errorCallback = mDownloadError ? nullptr : mUpdateErrorCallback.get();

  DownloadDone();

  nsAutoCString strResult;
  strResult.AppendInt(static_cast<uint32_t>(result));
  if (errorCallback) {
    errorCallback->HandleEvent(strResult);
  }

  return NS_OK;
}

nsresult
nsUrlClassifierStreamUpdater::AddRequestBody(const nsACString &aRequestBody)
{
  nsresult rv;
  nsCOMPtr<nsIStringInputStream> strStream =
    do_CreateInstance(NS_STRINGINPUTSTREAM_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = strStream->SetData(aRequestBody.BeginReading(),
                          aRequestBody.Length());
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIUploadChannel> uploadChannel = do_QueryInterface(mChannel, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = uploadChannel->SetUploadStream(strStream,
                                      NS_LITERAL_CSTRING("text/plain"),
                                      -1);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(mChannel, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = httpChannel->SetRequestMethod(NS_LITERAL_CSTRING("POST"));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////////
// nsIStreamListenerObserver implementation

NS_IMETHODIMP
nsUrlClassifierStreamUpdater::OnStartRequest(nsIRequest *request,
                                             nsISupports* context)
{
  nsresult rv;
  bool downloadError = false;
  nsAutoCString strStatus;
  nsresult status = NS_OK;

  // Only update if we got http success header
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(request);
  if (httpChannel) {
    rv = httpChannel->GetStatus(&status);
    NS_ENSURE_SUCCESS(rv, rv);

    if (MOZ_LOG_TEST(gUrlClassifierStreamUpdaterLog, mozilla::LogLevel::Debug)) {
      nsAutoCString errorName, spec;
      mozilla::GetErrorName(status, errorName);
      nsCOMPtr<nsIURI> uri;
      rv = httpChannel->GetURI(getter_AddRefs(uri));
      if (NS_SUCCEEDED(rv) && uri) {
        uri->GetAsciiSpec(spec);
      }
      LOG(("nsUrlClassifierStreamUpdater::OnStartRequest "
           "(status=%s, uri=%s, this=%p)", errorName.get(),
           spec.get(), this));
    }

    if (NS_FAILED(status)) {
      // Assume we're overloading the server and trigger backoff.
      downloadError = true;
    } else {
      bool succeeded = false;
      rv = httpChannel->GetRequestSucceeded(&succeeded);
      NS_ENSURE_SUCCESS(rv, rv);

      uint32_t requestStatus;
      rv = httpChannel->GetResponseStatus(&requestStatus);
      NS_ENSURE_SUCCESS(rv, rv);
      LOG(("nsUrlClassifierStreamUpdater::OnStartRequest %s (%d)", succeeded ?
           "succeeded" : "failed", requestStatus));
      if (!succeeded) {
        // 404 or other error, pass error status back
        strStatus.AppendInt(requestStatus);
        downloadError = true;
      }
    }
  }

  if (downloadError) {
    LOG(("nsUrlClassifierStreamUpdater::Download error [this=%p]", this));

    // It's possible for mDownloadErrorCallback to be null on shutdown.
    if (mDownloadErrorCallback) {
      mDownloadErrorCallback->HandleEvent(strStatus);
    }

    mDownloadError = true;
    status = NS_ERROR_ABORT;
  } else if (NS_SUCCEEDED(status)) {
    MOZ_ASSERT(mDownloadErrorCallback);
    mBeganStream = true;
    LOG(("nsUrlClassifierStreamUpdater::Beginning stream [this=%p]", this));
    rv = mDBService->BeginStream(mStreamTable);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mStreamTable.Truncate();

  return status;
}

NS_IMETHODIMP
nsUrlClassifierStreamUpdater::OnDataAvailable(nsIRequest *request,
                                              nsISupports* context,
                                              nsIInputStream *aIStream,
                                              uint64_t aSourceOffset,
                                              uint32_t aLength)
{
  if (!mDBService)
    return NS_ERROR_NOT_INITIALIZED;

  LOG(("OnDataAvailable (%d bytes)", aLength));

  nsresult rv;

  // Copy the data into a nsCString
  nsCString chunk;
  rv = NS_ConsumeStream(aIStream, aLength, chunk);
  NS_ENSURE_SUCCESS(rv, rv);

  //LOG(("Chunk (%d): %s\n\n", chunk.Length(), chunk.get()));
  rv = mDBService->UpdateStream(chunk);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierStreamUpdater::OnStopRequest(nsIRequest *request, nsISupports* context,
                                            nsresult aStatus)
{
  if (!mDBService)
    return NS_ERROR_NOT_INITIALIZED;

  LOG(("OnStopRequest (status %x, beganStream %s, this=%p)", aStatus,
       mBeganStream ? "true" : "false", this));

  nsresult rv;

  if (NS_SUCCEEDED(aStatus)) {
    // Success, finish this stream and move on to the next.
    rv = mDBService->FinishStream();
  } else if (mBeganStream) {
    LOG(("OnStopRequest::Canceling update [this=%p]", this));
    // We began this stream and couldn't finish it.  We have to cancel the
    // update, it's not in a consistent state.
    rv = mDBService->CancelUpdate();
  } else {
    LOG(("OnStopRequest::Finishing update [this=%p]", this));
    // The fetch failed, but we didn't start the stream (probably a
    // server or connection error).  We can commit what we've applied
    // so far, and request again later.
    rv = mDBService->FinishUpdate();
  }

  mChannel = nullptr;

  // If the fetch failed, return the network status rather than NS_OK, the
  // result of finishing a possibly-empty update
  if (NS_SUCCEEDED(aStatus)) {
    return rv;
  }
  return aStatus;
}

///////////////////////////////////////////////////////////////////////////////
// nsIObserver implementation

NS_IMETHODIMP
nsUrlClassifierStreamUpdater::Observe(nsISupports *aSubject, const char *aTopic,
                                      const char16_t *aData)
{
  if (nsCRT::strcmp(aTopic, gQuitApplicationMessage) == 0) {
    if (mIsUpdating && mChannel) {
      LOG(("Cancel download"));
      nsresult rv;
      rv = mChannel->Cancel(NS_ERROR_ABORT);
      NS_ENSURE_SUCCESS(rv, rv);
      mIsUpdating = false;
      mChannel = nullptr;
    }
    if (mTimer) {
      mTimer->Cancel();
      mTimer = nullptr;
    }
  }
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// nsIInterfaceRequestor implementation

NS_IMETHODIMP
nsUrlClassifierStreamUpdater::GetInterface(const nsIID & eventSinkIID, void* *_retval)
{
  return QueryInterface(eventSinkIID, _retval);
}


///////////////////////////////////////////////////////////////////////////////
// nsITimerCallback implementation
NS_IMETHODIMP
nsUrlClassifierStreamUpdater::Notify(nsITimer *timer)
{
  LOG(("nsUrlClassifierStreamUpdater::Notify [%p]", this));

  mTimer = nullptr;

  // Start the update process up again.
  FetchNext();

  return NS_OK;
}

