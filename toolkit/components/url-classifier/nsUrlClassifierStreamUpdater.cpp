//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Components.h"
#include "nsCRT.h"
#include "nsIHttpChannel.h"
#include "nsIObserverService.h"
#include "nsIStringStream.h"
#include "nsIUploadChannel.h"
#include "nsIURI.h"
#include "nsIUrlClassifierDBService.h"
#include "nsIUrlClassifierRemoteSettingsService.h"
#include "nsUrlClassifierUtils.h"
#include "nsNetUtil.h"
#include "nsStreamUtils.h"
#include "nsStringStream.h"
#include "nsUrlClassifierStreamUpdater.h"
#include "mozilla/ErrorNames.h"
#include "mozilla/Logging.h"
#include "nsIInterfaceRequestor.h"
#include "mozilla/Telemetry.h"
#include "nsContentUtils.h"
#include "nsIURLFormatter.h"
#include "Classifier.h"
#include "UrlClassifierTelemetryUtils.h"
#include "mozilla/StaticPrefs_urlclassifier.h"

using namespace mozilla::safebrowsing;
using namespace mozilla;

#define MIN_TIMEOUT_MS (60 * 1000)

static const char* gQuitApplicationMessage = "quit-application";

// Limit the list file size to 32mb
const uint32_t MAX_FILE_SIZE = (32 * 1024 * 1024);

// Retry delay when we failed to DownloadUpdate() if due to
// DBService busy.
const uint32_t FETCH_NEXT_REQUEST_RETRY_DELAY_MS = 1000;

#undef LOG

// MOZ_LOG=UrlClassifierStreamUpdater:5
static mozilla::LazyLogModule gUrlClassifierStreamUpdaterLog(
    "UrlClassifierStreamUpdater");
#define LOG(args) TrimAndLog args
#define LOG_ENABLED() \
  MOZ_LOG_TEST(gUrlClassifierStreamUpdaterLog, mozilla::LogLevel::Debug)

// Calls nsIURLFormatter::TrimSensitiveURLs to remove sensitive
// info from the logging message.
static MOZ_FORMAT_PRINTF(1, 2) void TrimAndLog(const char* aFmt, ...) {
  nsString raw;

  va_list ap;
  va_start(ap, aFmt);
  raw.AppendVprintf(aFmt, ap);
  va_end(ap);

  nsCOMPtr<nsIURLFormatter> urlFormatter =
      do_GetService("@mozilla.org/toolkit/URLFormatterService;1");

  nsString trimmed;
  nsresult rv = urlFormatter->TrimSensitiveURLs(raw, trimmed);
  if (NS_FAILED(rv)) {
    trimmed.Truncate();
  }

  // Use %s so we aren't exposing random strings to printf interpolation.
  MOZ_LOG(gUrlClassifierStreamUpdaterLog, mozilla::LogLevel::Debug,
          ("%s", NS_ConvertUTF16toUTF8(trimmed).get()));
}

// This class does absolutely nothing, except pass requests onto the DBService.

///////////////////////////////////////////////////////////////////////////////
// nsIUrlClassiferStreamUpdater implementation
// Handles creating/running the stream listener

nsUrlClassifierStreamUpdater::nsUrlClassifierStreamUpdater()
    : mIsUpdating(false),
      mInitialized(false),
      mDownloadError(false),
      mBeganStream(false),
      mChannel(nullptr),
      mTelemetryClockStart(0) {
  LOG(("nsUrlClassifierStreamUpdater init [this=%p]", this));
}

NS_IMPL_ISUPPORTS(nsUrlClassifierStreamUpdater, nsIUrlClassifierStreamUpdater,
                  nsIUrlClassifierUpdateObserver, nsIRequestObserver,
                  nsIStreamListener, nsIObserver, nsIInterfaceRequestor,
                  nsITimerCallback, nsINamed)

/**
 * Clear out the update.
 */
void nsUrlClassifierStreamUpdater::DownloadDone() {
  LOG(("nsUrlClassifierStreamUpdater::DownloadDone [this=%p]", this));
  mIsUpdating = false;

  mPendingUpdates.Clear();
  mDownloadError = false;
  mCurrentRequest = nullptr;
}

///////////////////////////////////////////////////////////////////////////////
// nsIUrlClassifierStreamUpdater implementation

nsresult nsUrlClassifierStreamUpdater::FetchUpdate(
    nsIURI* aUpdateUrl, const nsACString& aRequestPayload, bool aIsPostRequest,
    const nsACString& aStreamTable) {
  mBeganStream = false;
  nsresult rv;
  // moz-sbrs is a customed scheme used by Safe Browsing. When the scheme is
  // present in the update url, we'll fetch the data from
  // UrlClassifierRemoteSettingsService.
  if (aUpdateUrl->SchemeIs("moz-sbrs")) {
#ifdef DEBUG
    LOG(("Fetching update %s from RemoteSettings", aRequestPayload.Data()));
#endif
    nsCOMPtr<nsIUrlClassifierRemoteSettingsService> rsService =
        do_GetService("@mozilla.org/url-classifier/list-service;1");
    if (!rsService) {
      return NS_ERROR_FAILURE;
    }

    rv = rsService->FetchList(aRequestPayload, this);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else {
#ifdef DEBUG
    LOG(("Fetching update %s from %s", aRequestPayload.Data(),
         aUpdateUrl->GetSpecOrDefault().get()));
#endif
    uint32_t loadFlags = nsIChannel::INHIBIT_CACHING |
                         nsIChannel::LOAD_BYPASS_CACHE |
                         nsIChannel::LOAD_BYPASS_URL_CLASSIFIER;

    // SafeBrowsing update request should never be classified to make sure
    // we can recover from a bad SafeBrowsing database.
    rv = NS_NewChannel(getter_AddRefs(mChannel), aUpdateUrl,
                       nsContentUtils::GetSystemPrincipal(),
                       nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
                       nsIContentPolicy::TYPE_OTHER,
                       nullptr,  // nsICookieJarSettings
                       nullptr,  // aPerformanceStorage
                       nullptr,  // aLoadGroup
                       this,     // aInterfaceRequestor
                       loadFlags);

    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsILoadInfo> loadInfo = mChannel->LoadInfo();
    mozilla::OriginAttributes attrs;
    attrs.mFirstPartyDomain.AssignLiteral(
        NECKO_SAFEBROWSING_FIRST_PARTY_DOMAIN);
    loadInfo->SetOriginAttributes(attrs);
    // allow deprecated HTTP request from SystemPrincipal
    loadInfo->SetAllowDeprecatedSystemRequests(true);

    if (!aIsPostRequest) {
      // We use POST method to send our request in v2. In v4, the request
      // needs to be embedded to the URL and use GET method to send.
      // However, from the Chromium source code, a extended HTTP header has
      // to be sent along with the request to make the request succeed.
      // The following description is from Chromium source code:
      //
      // "The following header informs the envelope server (which sits in
      // front of Google's stubby server) that the received GET request should
      // be interpreted as a POST."
      //
      nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(mChannel, &rv);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = httpChannel->SetRequestHeader("X-HTTP-Method-Override"_ns, "POST"_ns,
                                         false);
      NS_ENSURE_SUCCESS(rv, rv);
    } else if (!aRequestPayload.IsEmpty()) {
      rv = AddRequestBody(aRequestPayload);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    // Set the appropriate content type for file/data URIs, for unit testing
    // purposes.
    // This is only used for testing and should be deleted.
    if (aUpdateUrl->SchemeIs("file") || aUpdateUrl->SchemeIs("data")) {
      mChannel->SetContentType("application/vnd.google.safebrowsing-update"_ns);
    } else {
      // We assume everything else is an HTTP request.

      // Disable keepalive.
      nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(mChannel, &rv);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = httpChannel->SetRequestHeader("Connection"_ns, "close"_ns, false);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    // Make the request.
    rv = mChannel->AsyncOpen(this);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mTelemetryClockStart = PR_IntervalNow();
  mStreamTable = aStreamTable;

  if (StaticPrefs::urlclassifier_update_response_timeout_ms() >
      StaticPrefs::urlclassifier_update_timeout_ms()) {
    NS_WARNING(
        "Safe Browsing response timeout is greater than the general "
        "timeout. Disabling these update timeouts.");
    return NS_OK;
  }
  MOZ_TRY_VAR(mResponseTimeoutTimer,
              NS_NewTimerWithCallback(
                  this, StaticPrefs::urlclassifier_update_response_timeout_ms(),
                  nsITimer::TYPE_ONE_SHOT));

  MOZ_TRY_VAR(mTimeoutTimer,
              NS_NewTimerWithCallback(
                  this, StaticPrefs::urlclassifier_update_timeout_ms(),
                  nsITimer::TYPE_ONE_SHOT));

  if (StaticPrefs::urlclassifier_update_timeout_ms() < MIN_TIMEOUT_MS) {
    LOG(("Download update timeout %d ms (< %d ms) would be too small",
         StaticPrefs::urlclassifier_update_timeout_ms(), MIN_TIMEOUT_MS));
  }

  return NS_OK;
}

nsresult nsUrlClassifierStreamUpdater::FetchUpdate(
    const nsACString& aUpdateUrl, const nsACString& aRequestPayload,
    bool aIsPostRequest, const nsACString& aStreamTable) {
  LOG(("(pre) Fetching update from %s\n",
       PromiseFlatCString(aUpdateUrl).get()));

  nsCString updateUrl(aUpdateUrl);
  if (!aIsPostRequest) {
    updateUrl.AppendPrintf("&$req=%s", nsCString(aRequestPayload).get());
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), updateUrl);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString urlSpec;
  uri->GetAsciiSpec(urlSpec);

  LOG(("(post) Fetching update from %s\n", urlSpec.get()));

  return FetchUpdate(uri, aRequestPayload, aIsPostRequest, aStreamTable);
}

NS_IMETHODIMP
nsUrlClassifierStreamUpdater::DownloadUpdates(
    const nsACString& aRequestTables, const nsACString& aRequestPayload,
    bool aIsPostRequest, const nsACString& aUpdateUrl,
    nsIUrlClassifierCallback* aSuccessCallback,
    nsIUrlClassifierCallback* aUpdateErrorCallback,
    nsIUrlClassifierCallback* aDownloadErrorCallback, bool* _retval) {
  NS_ENSURE_ARG(aSuccessCallback);
  NS_ENSURE_ARG(aUpdateErrorCallback);
  NS_ENSURE_ARG(aDownloadErrorCallback);

  if (mIsUpdating) {
    LOG(("Already updating, queueing update %s from %s", aRequestPayload.Data(),
         aUpdateUrl.Data()));
    *_retval = false;
    UpdateRequest* request = mPendingRequests.AppendElement(fallible);
    if (!request) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    BuildUpdateRequest(aRequestTables, aRequestPayload, aIsPostRequest,
                       aUpdateUrl, aSuccessCallback, aUpdateErrorCallback,
                       aDownloadErrorCallback, request);
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
    if (!observerService) return NS_ERROR_FAILURE;

    observerService->AddObserver(this, gQuitApplicationMessage, false);

    mDBService = mozilla::components::UrlClassifierDB::Service(&rv);
    NS_ENSURE_SUCCESS(rv, rv);

    mInitialized = true;
  }

  rv = mDBService->BeginUpdate(this, aRequestTables);
  if (rv == NS_ERROR_NOT_AVAILABLE) {
    LOG(("Service busy, already updating, queuing update %s from %s",
         aRequestPayload.Data(), aUpdateUrl.Data()));
    *_retval = false;
    UpdateRequest* request = mPendingRequests.AppendElement(fallible);
    if (!request) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    BuildUpdateRequest(aRequestTables, aRequestPayload, aIsPostRequest,
                       aUpdateUrl, aSuccessCallback, aUpdateErrorCallback,
                       aDownloadErrorCallback, request);

    // We cannot guarantee that we will be notified when DBService is done
    // processing the current update, so we fire a retry timer on our own.
    MOZ_TRY_VAR(mFetchNextRequestTimer,
                NS_NewTimerWithCallback(this, FETCH_NEXT_REQUEST_RETRY_DELAY_MS,
                                        nsITimer::TYPE_ONE_SHOT));

    return NS_OK;
  }

  if (NS_FAILED(rv)) {
    return rv;
  }

  nsUrlClassifierUtils* urlUtil = nsUrlClassifierUtils::GetInstance();
  if (NS_WARN_IF(!urlUtil)) {
    return NS_ERROR_FAILURE;
  }

  nsTArray<nsCString> tables;
  mozilla::safebrowsing::Classifier::SplitTables(aRequestTables, tables);
  urlUtil->GetTelemetryProvider(tables.SafeElementAt(0, ""_ns),
                                mTelemetryProvider);

  mCurrentRequest = MakeUnique<UpdateRequest>();
  BuildUpdateRequest(aRequestTables, aRequestPayload, aIsPostRequest,
                     aUpdateUrl, aSuccessCallback, aUpdateErrorCallback,
                     aDownloadErrorCallback, mCurrentRequest.get());

  mIsUpdating = true;
  *_retval = true;

  LOG(("FetchUpdate: %s", mCurrentRequest->mUrl.Data()));

  return FetchUpdate(aUpdateUrl, aRequestPayload, aIsPostRequest, ""_ns);
}

///////////////////////////////////////////////////////////////////////////////
// nsIUrlClassifierUpdateObserver implementation

NS_IMETHODIMP
nsUrlClassifierStreamUpdater::UpdateUrlRequested(const nsACString& aUrl,
                                                 const nsACString& aTable) {
  LOG(("Queuing requested update from %s\n", PromiseFlatCString(aUrl).get()));

  PendingUpdate* update = mPendingUpdates.AppendElement(fallible);
  if (!update) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Allow data: and file: urls for unit testing purposes, otherwise assume http
  if (StringBeginsWith(aUrl, "data:"_ns) ||
      StringBeginsWith(aUrl, "file:"_ns)) {
    update->mUrl = aUrl;
  } else {
    // For unittesting update urls to localhost should use http, not https
    // (otherwise the connection will fail silently, since there will be no
    // cert available).
    if (!StringBeginsWith(aUrl, "localhost"_ns)) {
      update->mUrl = "https://"_ns + aUrl;
    } else {
      update->mUrl = "http://"_ns + aUrl;
    }
  }
  update->mTable = aTable;

  return NS_OK;
}

nsresult nsUrlClassifierStreamUpdater::FetchNext() {
  if (mPendingUpdates.Length() == 0) {
    return NS_OK;
  }

  PendingUpdate& update = mPendingUpdates[0];
  LOG(("Fetching update url: %s\n", update.mUrl.get()));
  nsresult rv =
      FetchUpdate(update.mUrl, ""_ns,
                  true,  // This method is for v2 and v2 is always a POST.
                  update.mTable);
  if (NS_FAILED(rv)) {
    nsAutoCString errorName;
    mozilla::GetErrorName(rv, errorName);
    LOG(("Error (%s) fetching update url: %s\n", errorName.get(),
         update.mUrl.get()));
    // We can commit the urls that we've applied so far.  This is
    // probably a transient server problem, so trigger backoff.
    mDownloadError = true;
    mDBService->FinishUpdate();
    return rv;
  }

  mPendingUpdates.RemoveElementAt(0);

  return NS_OK;
}

nsresult nsUrlClassifierStreamUpdater::FetchNextRequest() {
  if (mPendingRequests.Length() == 0) {
    LOG(("No more requests, returning"));
    return NS_OK;
  }

  UpdateRequest request = mPendingRequests[0];
  mPendingRequests.RemoveElementAt(0);
  LOG(("Stream updater: fetching next request: %s, %s", request.mTables.get(),
       request.mUrl.get()));
  bool dummy;
  DownloadUpdates(request.mTables, request.mRequestPayload,
                  request.mIsPostRequest, request.mUrl,
                  request.mSuccessCallback, request.mUpdateErrorCallback,
                  request.mDownloadErrorCallback, &dummy);
  return NS_OK;
}

void nsUrlClassifierStreamUpdater::BuildUpdateRequest(
    const nsACString& aRequestTables, const nsACString& aRequestPayload,
    bool aIsPostRequest, const nsACString& aUpdateUrl,
    nsIUrlClassifierCallback* aSuccessCallback,
    nsIUrlClassifierCallback* aUpdateErrorCallback,
    nsIUrlClassifierCallback* aDownloadErrorCallback, UpdateRequest* aRequest) {
  MOZ_ASSERT(aRequest);

  aRequest->mTables = aRequestTables;
  aRequest->mRequestPayload = aRequestPayload;
  aRequest->mIsPostRequest = aIsPostRequest;
  aRequest->mUrl = aUpdateUrl;
  aRequest->mSuccessCallback = aSuccessCallback;
  aRequest->mUpdateErrorCallback = aUpdateErrorCallback;
  aRequest->mDownloadErrorCallback = aDownloadErrorCallback;
}

NS_IMETHODIMP
nsUrlClassifierStreamUpdater::StreamFinished(nsresult status,
                                             uint32_t requestedDelay) {
  // We are a service and may not be reset with Init between calls, so reset
  // mBeganStream manually.
  mBeganStream = false;
  if (LOG_ENABLED()) {
    nsAutoCString errorName;
    mozilla::GetErrorName(status, errorName);
    LOG(("nsUrlClassifierStreamUpdater::StreamFinished [%s, %d]",
         errorName.get(), requestedDelay));
  }
  if (NS_FAILED(status) || mPendingUpdates.Length() == 0) {
    // We're done.
    LOG(("nsUrlClassifierStreamUpdater::Done [this=%p]", this));
    mDBService->FinishUpdate();
    return NS_OK;
  }

  // This timer is for fetching indirect updates ("forwards") from any "u:"
  // lines that we encountered while processing the server response. It is NOT
  // for scheduling the next time we pull the list from the server. That's a
  // different timer in listmanager.js (see bug 1110891).
  nsresult rv;
  rv = NS_NewTimerWithCallback(getter_AddRefs(mFetchIndirectUpdatesTimer), this,
                               requestedDelay, nsITimer::TYPE_ONE_SHOT);

  if (NS_FAILED(rv)) {
    NS_WARNING(
        "Unable to initialize timer, fetching next safebrowsing item "
        "immediately");
    return FetchNext();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierStreamUpdater::UpdateSuccess(uint32_t requestedTimeout) {
  LOG(("nsUrlClassifierStreamUpdater::UpdateSuccess [this=%p]", this));
  if (mPendingUpdates.Length() != 0) {
    NS_WARNING("Didn't fetch all safebrowsing update redirects");
  }

  // DownloadDone() clears mSuccessCallback, so we save it off here.
  nsCOMPtr<nsIUrlClassifierCallback> successCallback =
      mDownloadError ? nullptr : mCurrentRequest->mSuccessCallback.get();
  nsCOMPtr<nsIUrlClassifierCallback> downloadErrorCallback =
      mDownloadError ? mCurrentRequest->mDownloadErrorCallback.get() : nullptr;

  DownloadDone();

  nsAutoCString strTimeout;
  strTimeout.AppendInt(requestedTimeout);
  if (successCallback) {
    LOG(("nsUrlClassifierStreamUpdater::UpdateSuccess callback [this=%p]",
         this));
    successCallback->HandleEvent(strTimeout);
  } else if (downloadErrorCallback) {
    downloadErrorCallback->HandleEvent(mDownloadErrorStatusStr);
    mDownloadErrorStatusStr.Truncate();
    LOG(("Notify download error callback in UpdateSuccess [this=%p]", this));
  }
  // Now fetch the next request
  LOG(("stream updater: calling into fetch next request"));
  FetchNextRequest();

  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierStreamUpdater::UpdateError(nsresult result) {
  LOG(("nsUrlClassifierStreamUpdater::UpdateError [this=%p]", this));

  // DownloadDone() clears mUpdateErrorCallback, so we save it off here.
  nsCOMPtr<nsIUrlClassifierCallback> errorCallback =
      mDownloadError ? nullptr : mCurrentRequest->mUpdateErrorCallback.get();
  nsCOMPtr<nsIUrlClassifierCallback> downloadErrorCallback =
      mDownloadError ? mCurrentRequest->mDownloadErrorCallback.get() : nullptr;
  DownloadDone();

  if (errorCallback) {
    nsAutoCString strResult;
    mozilla::GetErrorName(result, strResult);
    errorCallback->HandleEvent(strResult);
  } else if (downloadErrorCallback) {
    LOG(("Notify download error callback in UpdateError [this=%p]", this));
    downloadErrorCallback->HandleEvent(mDownloadErrorStatusStr);
    mDownloadErrorStatusStr.Truncate();
  }

  return NS_OK;
}

nsresult nsUrlClassifierStreamUpdater::AddRequestBody(
    const nsACString& aRequestBody) {
  nsresult rv;
  nsCOMPtr<nsIStringInputStream> strStream =
      do_CreateInstance(NS_STRINGINPUTSTREAM_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = strStream->SetData(aRequestBody.BeginReading(), aRequestBody.Length());
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIUploadChannel> uploadChannel = do_QueryInterface(mChannel, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = uploadChannel->SetUploadStream(strStream, "text/plain"_ns, -1);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(mChannel, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = httpChannel->SetRequestMethod("POST"_ns);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// nsIStreamListenerObserver implementation

NS_IMETHODIMP
nsUrlClassifierStreamUpdater::OnStartRequest(nsIRequest* request) {
  nsresult rv;
  bool downloadError = false;
  nsAutoCString strStatus;
  nsresult status = NS_OK;

  // Only update if we got http success header
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(request);
  if (httpChannel) {
    rv = httpChannel->GetStatus(&status);
    NS_ENSURE_SUCCESS(rv, rv);

    if (LOG_ENABLED()) {
      nsAutoCString errorName, spec;
      mozilla::GetErrorName(status, errorName);
      nsCOMPtr<nsIURI> uri;
      rv = httpChannel->GetURI(getter_AddRefs(uri));
      if (NS_SUCCEEDED(rv) && uri) {
        uri->GetAsciiSpec(spec);
      }
      LOG(
          ("nsUrlClassifierStreamUpdater::OnStartRequest "
           "(status=%s, uri=%s, this=%p)",
           errorName.get(), spec.get(), this));
    }
    if (mTelemetryClockStart > 0) {
      uint32_t msecs =
          PR_IntervalToMilliseconds(PR_IntervalNow() - mTelemetryClockStart);
      mozilla::Telemetry::Accumulate(
          mozilla::Telemetry::URLCLASSIFIER_UPDATE_SERVER_RESPONSE_TIME,
          mTelemetryProvider, msecs);
    }

    if (mResponseTimeoutTimer) {
      mResponseTimeoutTimer->Cancel();
      mResponseTimeoutTimer = nullptr;
    }

    uint8_t netErrCode = NS_FAILED(status) ? NetworkErrorToBucket(status) : 0;
    mozilla::Telemetry::Accumulate(
        mozilla::Telemetry::URLCLASSIFIER_UPDATE_REMOTE_NETWORK_ERROR,
        mTelemetryProvider, netErrCode);

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
      mozilla::Telemetry::Accumulate(
          mozilla::Telemetry::URLCLASSIFIER_UPDATE_REMOTE_STATUS2,
          mTelemetryProvider, HTTPStatusToBucket(requestStatus));
      if (requestStatus == 400) {
        printf_stderr(
            "Safe Browsing server returned a 400 during update:"
            "request url = %s, payload = %s\n",
            mCurrentRequest->mUrl.get(),
            mCurrentRequest->mRequestPayload.get());
      }

      LOG(("nsUrlClassifierStreamUpdater::OnStartRequest %s (%d)",
           succeeded ? "succeeded" : "failed", requestStatus));
      if (!succeeded) {
        // 404 or other error, pass error status back
        strStatus.AppendInt(requestStatus);
        downloadError = true;
      }
    }
  }

  if (downloadError) {
    LOG(("nsUrlClassifierStreamUpdater::Download error [this=%p]", this));
    mDownloadError = true;
    mDownloadErrorStatusStr = strStatus;
    status = NS_ERROR_ABORT;
  } else if (NS_SUCCEEDED(status)) {
    MOZ_ASSERT(mCurrentRequest->mDownloadErrorCallback);
    mBeganStream = true;
    LOG(("nsUrlClassifierStreamUpdater::Beginning stream [this=%p]", this));
    rv = mDBService->BeginStream(mStreamTable);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mStreamTable.Truncate();

  return status;
}

NS_IMETHODIMP
nsUrlClassifierStreamUpdater::OnDataAvailable(nsIRequest* request,
                                              nsIInputStream* aIStream,
                                              uint64_t aSourceOffset,
                                              uint32_t aLength) {
  if (!mDBService) return NS_ERROR_NOT_INITIALIZED;

  LOG(("OnDataAvailable (%d bytes)", aLength));

  if (aSourceOffset > MAX_FILE_SIZE) {
    LOG((
        "OnDataAvailable::Abort because exceeded the maximum file size(%" PRIu64
        ")",
        aSourceOffset));
    return NS_ERROR_FILE_TOO_BIG;
  }

  nsresult rv;

  // Copy the data into a nsCString
  nsCString chunk;
  rv = NS_ConsumeStream(aIStream, aLength, chunk);
  NS_ENSURE_SUCCESS(rv, rv);

  // LOG(("Chunk (%d): %s\n\n", chunk.Length(), chunk.get()));
  rv = mDBService->UpdateStream(chunk);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierStreamUpdater::OnStopRequest(nsIRequest* request,
                                            nsresult aStatus) {
  if (!mDBService) return NS_ERROR_NOT_INITIALIZED;

  if (LOG_ENABLED()) {
    nsAutoCString errorName;
    mozilla::GetErrorName(aStatus, errorName);
    LOG(("OnStopRequest (status %s, beganStream %s, this=%p)", errorName.get(),
         mBeganStream ? "true" : "false", this));
  }

  nsresult rv;

  if (NS_SUCCEEDED(aStatus)) {
    // Success, finish this stream and move on to the next.
    rv = mDBService->FinishStream();
  } else if (mBeganStream) {
    LOG(("OnStopRequest::Canceling update [this=%p]", this));
    // We began this stream and couldn't finish it.  We have to cancel the
    // update, it's not in a consistent state.
    mDownloadError = true;
    rv = mDBService->CancelUpdate();
  } else {
    LOG(("OnStopRequest::Finishing update [this=%p]", this));
    // The fetch failed, but we didn't start the stream (probably a
    // server or connection error).  We can commit what we've applied
    // so far, and request again later.
    rv = mDBService->FinishUpdate();
  }

  if (mResponseTimeoutTimer) {
    mResponseTimeoutTimer->Cancel();
    mResponseTimeoutTimer = nullptr;
  }

  // mResponseTimeoutTimer may be cleared in OnStartRequest, so we check
  // mTimeoutTimer to see whether the update was has timed out
  if (mTimeoutTimer) {
    mozilla::Telemetry::Accumulate(
        mozilla::Telemetry::URLCLASSIFIER_UPDATE_TIMEOUT, mTelemetryProvider,
        static_cast<uint8_t>(eNoTimeout));
    mTimeoutTimer->Cancel();
    mTimeoutTimer = nullptr;
  }

  mTelemetryProvider.Truncate();
  mTelemetryClockStart = 0;
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
nsUrlClassifierStreamUpdater::Observe(nsISupports* aSubject, const char* aTopic,
                                      const char16_t* aData) {
  if (nsCRT::strcmp(aTopic, gQuitApplicationMessage) == 0) {
    if (mIsUpdating && mChannel) {
      LOG(("Cancel download"));
      nsresult rv;
      rv = mChannel->Cancel(NS_ERROR_ABORT);
      NS_ENSURE_SUCCESS(rv, rv);
      mIsUpdating = false;
      mChannel = nullptr;
      mTelemetryClockStart = 0;
    }
    if (mFetchIndirectUpdatesTimer) {
      mFetchIndirectUpdatesTimer->Cancel();
      mFetchIndirectUpdatesTimer = nullptr;
    }
    if (mFetchNextRequestTimer) {
      mFetchNextRequestTimer->Cancel();
      mFetchNextRequestTimer = nullptr;
    }
    if (mResponseTimeoutTimer) {
      mResponseTimeoutTimer->Cancel();
      mResponseTimeoutTimer = nullptr;
    }
    if (mTimeoutTimer) {
      mTimeoutTimer->Cancel();
      mTimeoutTimer = nullptr;
    }
  }
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// nsIInterfaceRequestor implementation

NS_IMETHODIMP
nsUrlClassifierStreamUpdater::GetInterface(const nsIID& eventSinkIID,
                                           void** _retval) {
  return QueryInterface(eventSinkIID, _retval);
}

///////////////////////////////////////////////////////////////////////////////
// nsITimerCallback implementation
NS_IMETHODIMP
nsUrlClassifierStreamUpdater::Notify(nsITimer* timer) {
  LOG(("nsUrlClassifierStreamUpdater::Notify [%p]", this));

  if (timer == mFetchNextRequestTimer) {
    mFetchNextRequestTimer = nullptr;
    FetchNextRequest();
    return NS_OK;
  }

  if (timer == mFetchIndirectUpdatesTimer) {
    mFetchIndirectUpdatesTimer = nullptr;
    // Start the update process up again.
    FetchNext();
    return NS_OK;
  }

  bool updateFailed = false;
  if (timer == mResponseTimeoutTimer) {
    mResponseTimeoutTimer = nullptr;
    if (mTimeoutTimer) {
      mTimeoutTimer->Cancel();
      mTimeoutTimer = nullptr;
    }
    mDownloadError = true;  // Trigger backoff
    updateFailed = true;
    MOZ_LOG(gUrlClassifierStreamUpdaterLog, mozilla::LogLevel::Error,
            ("Safe Browsing timed out while waiting for the update server to "
             "respond."));
    mozilla::Telemetry::Accumulate(
        mozilla::Telemetry::URLCLASSIFIER_UPDATE_TIMEOUT, mTelemetryProvider,
        static_cast<uint8_t>(eResponseTimeout));
  }

  if (timer == mTimeoutTimer) {
    mTimeoutTimer = nullptr;
    // No backoff since the connection may just be temporarily slow.
    updateFailed = true;
    MOZ_LOG(gUrlClassifierStreamUpdaterLog, mozilla::LogLevel::Error,
            ("Safe Browsing timed out while waiting for the update server to "
             "finish."));
    mozilla::Telemetry::Accumulate(
        mozilla::Telemetry::URLCLASSIFIER_UPDATE_TIMEOUT, mTelemetryProvider,
        static_cast<uint8_t>(eDownloadTimeout));
  }

  if (updateFailed) {
    // Cancelling the channel will trigger OnStopRequest.
    if (mChannel) {
      mozilla::Unused << mChannel->Cancel(NS_ERROR_ABORT);
      mChannel = nullptr;
    }
    mTelemetryClockStart = 0;

    if (mFetchIndirectUpdatesTimer) {
      mFetchIndirectUpdatesTimer->Cancel();
      mFetchIndirectUpdatesTimer = nullptr;
    }
    if (mFetchNextRequestTimer) {
      mFetchNextRequestTimer->Cancel();
      mFetchNextRequestTimer = nullptr;
    }

    return NS_OK;
  }

  MOZ_ASSERT_UNREACHABLE("A timer is fired from nowhere.");
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
//// nsINamed

NS_IMETHODIMP
nsUrlClassifierStreamUpdater::GetName(nsACString& aName) {
  aName.AssignLiteral("nsUrlClassifierStreamUpdater");
  return NS_OK;
}
