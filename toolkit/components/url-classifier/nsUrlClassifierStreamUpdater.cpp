//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Url Classifier code
 *
 * The Initial Developer of the Original Code is
 * Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Tony Chang <tony@ponderer.org> (original author)
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

#include "nsCRT.h"
#include "nsIHttpChannel.h"
#include "nsIObserverService.h"
#include "nsIStringStream.h"
#include "nsIUploadChannel.h"
#include "nsIURI.h"
#include "nsIUrlClassifierDBService.h"
#include "nsStreamUtils.h"
#include "nsStringStream.h"
#include "nsToolkitCompsCID.h"
#include "nsUrlClassifierStreamUpdater.h"
#include "prlog.h"

static const char* gQuitApplicationMessage = "quit-application";

// NSPR_LOG_MODULES=UrlClassifierStreamUpdater:5
#if defined(PR_LOGGING)
static const PRLogModuleInfo *gUrlClassifierStreamUpdaterLog = nsnull;
#define LOG(args) PR_LOG(gUrlClassifierStreamUpdaterLog, PR_LOG_DEBUG, args)
#else
#define LOG(args)
#endif


///////////////////////////////////////////////////////////////////////////////
// nsIUrlClassiferStreamUpdater implementation
// Handles creating/running the stream listener

nsUrlClassifierStreamUpdater::nsUrlClassifierStreamUpdater()
  : mIsUpdating(false), mInitialized(false), mDownloadError(false),
    mBeganStream(false), mUpdateUrl(nsnull), mChannel(nsnull)
{
#if defined(PR_LOGGING)
  if (!gUrlClassifierStreamUpdaterLog)
    gUrlClassifierStreamUpdaterLog = PR_NewLogModule("UrlClassifierStreamUpdater");
#endif

}

NS_IMPL_THREADSAFE_ISUPPORTS9(nsUrlClassifierStreamUpdater,
                              nsIUrlClassifierStreamUpdater,
                              nsIUrlClassifierUpdateObserver,
                              nsIRequestObserver,
                              nsIStreamListener,
                              nsIObserver,
                              nsIBadCertListener2,
                              nsISSLErrorListener,
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
  mSuccessCallback = nsnull;
  mUpdateErrorCallback = nsnull;
  mDownloadErrorCallback = nsnull;
}

///////////////////////////////////////////////////////////////////////////////
// nsIUrlClassifierStreamUpdater implementation

NS_IMETHODIMP
nsUrlClassifierStreamUpdater::GetUpdateUrl(nsACString & aUpdateUrl)
{
  if (mUpdateUrl) {
    mUpdateUrl->GetSpec(aUpdateUrl);
  } else {
    aUpdateUrl.Truncate();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierStreamUpdater::SetUpdateUrl(const nsACString & aUpdateUrl)
{
  LOG(("Update URL is %s\n", PromiseFlatCString(aUpdateUrl).get()));

  nsresult rv = NS_NewURI(getter_AddRefs(mUpdateUrl), aUpdateUrl);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsUrlClassifierStreamUpdater::FetchUpdate(nsIURI *aUpdateUrl,
                                          const nsACString & aRequestBody,
                                          const nsACString & aStreamTable,
                                          const nsACString & aServerMAC)
{
  nsresult rv;
  PRUint32 loadFlags = nsIChannel::INHIBIT_CACHING |
                       nsIChannel::LOAD_BYPASS_CACHE;
  rv = NS_NewChannel(getter_AddRefs(mChannel), aUpdateUrl, nsnull, nsnull, this,
                     loadFlags);
  NS_ENSURE_SUCCESS(rv, rv);

  mBeganStream = false;

  if (!aRequestBody.IsEmpty()) {
    rv = AddRequestBody(aRequestBody);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Set the appropriate content type for file/data URIs, for unit testing
  // purposes.
  bool match;
  if ((NS_SUCCEEDED(aUpdateUrl->SchemeIs("file", &match)) && match) ||
      (NS_SUCCEEDED(aUpdateUrl->SchemeIs("data", &match)) && match)) {
    mChannel->SetContentType(NS_LITERAL_CSTRING("application/vnd.google.safebrowsing-update"));
  }

  // Make the request
  rv = mChannel->AsyncOpen(this, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  mStreamTable = aStreamTable;
  mServerMAC = aServerMAC;

  return NS_OK;
}

nsresult
nsUrlClassifierStreamUpdater::FetchUpdate(const nsACString & aUpdateUrl,
                                          const nsACString & aRequestBody,
                                          const nsACString & aStreamTable,
                                          const nsACString & aServerMAC)
{
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aUpdateUrl);
  NS_ENSURE_SUCCESS(rv, rv);

  LOG(("Fetching update from %s\n", PromiseFlatCString(aUpdateUrl).get()));

  return FetchUpdate(uri, aRequestBody, aStreamTable, aServerMAC);
}

NS_IMETHODIMP
nsUrlClassifierStreamUpdater::DownloadUpdates(
                                const nsACString &aRequestTables,
                                const nsACString &aRequestBody,
                                const nsACString &aClientKey,
                                nsIUrlClassifierCallback *aSuccessCallback,
                                nsIUrlClassifierCallback *aUpdateErrorCallback,
                                nsIUrlClassifierCallback *aDownloadErrorCallback,
                                bool *_retval)
{
  NS_ENSURE_ARG(aSuccessCallback);
  NS_ENSURE_ARG(aUpdateErrorCallback);
  NS_ENSURE_ARG(aDownloadErrorCallback);

  if (mIsUpdating) {
    LOG(("already updating, skipping update"));
    *_retval = false;
    return NS_OK;
  }

  if (!mUpdateUrl) {
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

  rv = mDBService->BeginUpdate(this, aRequestTables, aClientKey);
  if (rv == NS_ERROR_NOT_AVAILABLE) {
    LOG(("already updating, skipping update"));
    *_retval = false;
    return NS_OK;
  } else if (NS_FAILED(rv)) {
    return rv;
  }

  mSuccessCallback = aSuccessCallback;
  mUpdateErrorCallback = aUpdateErrorCallback;
  mDownloadErrorCallback = aDownloadErrorCallback;

  mIsUpdating = true;
  *_retval = true;


  return FetchUpdate(mUpdateUrl, aRequestBody, EmptyCString(), EmptyCString());
}

///////////////////////////////////////////////////////////////////////////////
// nsIUrlClassifierUpdateObserver implementation

NS_IMETHODIMP
nsUrlClassifierStreamUpdater::UpdateUrlRequested(const nsACString &aUrl,
                                                 const nsACString &aTable,
                                                 const nsACString &aServerMAC)
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
    update->mUrl = NS_LITERAL_CSTRING("http://") + aUrl;
  }
  update->mTable = aTable;
  update->mServerMAC = aServerMAC;

  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierStreamUpdater::RekeyRequested()
{
  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();

  if (!observerService)
    return NS_ERROR_FAILURE;

  return observerService->NotifyObservers(static_cast<nsIUrlClassifierStreamUpdater*>(this),
                                          "url-classifier-rekey-requested",
                                          nsnull);
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
                            update.mTable, update.mServerMAC);
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

NS_IMETHODIMP
nsUrlClassifierStreamUpdater::StreamFinished(nsresult status,
                                             PRUint32 requestedDelay)
{
  LOG(("nsUrlClassifierStreamUpdater::StreamFinished [%x, %d]", status, requestedDelay));
  if (NS_FAILED(status) || mPendingUpdates.Length() == 0) {
    // We're done.
    mDBService->FinishUpdate();
    return NS_OK;
  }

  // Wait the requested amount of time before starting a new stream.
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
nsUrlClassifierStreamUpdater::UpdateSuccess(PRUint32 requestedTimeout)
{
  LOG(("nsUrlClassifierStreamUpdater::UpdateSuccess [this=%p]", this));
  if (mPendingUpdates.Length() != 0) {
    NS_WARNING("Didn't fetch all safebrowsing update redirects");
  }

  // DownloadDone() clears mSuccessCallback, so we save it off here.
  nsCOMPtr<nsIUrlClassifierCallback> successCallback = mDownloadError ? nsnull : mSuccessCallback.get();
  DownloadDone();

  nsCAutoString strTimeout;
  strTimeout.AppendInt(requestedTimeout);
  if (successCallback) {
    successCallback->HandleEvent(strTimeout);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierStreamUpdater::UpdateError(PRUint32 result)
{
  LOG(("nsUrlClassifierStreamUpdater::UpdateError [this=%p]", this));

  // DownloadDone() clears mUpdateErrorCallback, so we save it off here.
  nsCOMPtr<nsIUrlClassifierCallback> errorCallback = mDownloadError ? nsnull : mUpdateErrorCallback.get();

  DownloadDone();

  nsCAutoString strResult;
  strResult.AppendInt(result);
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
  nsCAutoString strStatus;
  nsresult status = NS_OK;

  // Only update if we got http success header
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(request);
  if (httpChannel) {
    rv = httpChannel->GetStatus(&status);
    NS_ENSURE_SUCCESS(rv, rv);

    if (NS_ERROR_CONNECTION_REFUSED == status ||
        NS_ERROR_NET_TIMEOUT == status) {
      // Assume we're overloading the server and trigger backoff.
      downloadError = true;
    }

    if (NS_SUCCEEDED(status)) {
      bool succeeded = false;
      rv = httpChannel->GetRequestSucceeded(&succeeded);
      NS_ENSURE_SUCCESS(rv, rv);

      if (!succeeded) {
        // 404 or other error, pass error status back
        LOG(("HTTP request returned failure code."));

        rv = httpChannel->GetResponseStatus(&status);
        NS_ENSURE_SUCCESS(rv, rv);

        strStatus.AppendInt(status);
        downloadError = true;
      }
    }
  }

  if (downloadError) {
    mDownloadErrorCallback->HandleEvent(strStatus);
    mDownloadError = true;
    status = NS_ERROR_ABORT;
  } else if (NS_SUCCEEDED(status)) {
    mBeganStream = true;
    rv = mDBService->BeginStream(mStreamTable, mServerMAC);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mStreamTable.Truncate();
  mServerMAC.Truncate();

  return status;
}

NS_IMETHODIMP
nsUrlClassifierStreamUpdater::OnDataAvailable(nsIRequest *request,
                                              nsISupports* context,
                                              nsIInputStream *aIStream,
                                              PRUint32 aSourceOffset,
                                              PRUint32 aLength)
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

  LOG(("OnStopRequest (status %x)", aStatus));

  nsresult rv;

  if (NS_SUCCEEDED(aStatus)) {
    // Success, finish this stream and move on to the next.
    rv = mDBService->FinishStream();
  } else if (mBeganStream) {
    // We began this stream and couldn't finish it.  We have to cancel the
    // update, it's not in a consistent state.
    rv = mDBService->CancelUpdate();
  } else {
    // The fetch failed, but we didn't start the stream (probably a
    // server or connection error).  We can commit what we've applied
    // so far, and request again later.
    rv = mDBService->FinishUpdate();
  }

  mChannel = nsnull;

  return rv;
}

///////////////////////////////////////////////////////////////////////////////
// nsIObserver implementation

NS_IMETHODIMP
nsUrlClassifierStreamUpdater::Observe(nsISupports *aSubject, const char *aTopic,
                                      const PRUnichar *aData)
{
  if (nsCRT::strcmp(aTopic, gQuitApplicationMessage) == 0) {
    if (mIsUpdating && mChannel) {
      LOG(("Cancel download"));
      nsresult rv;
      rv = mChannel->Cancel(NS_ERROR_ABORT);
      NS_ENSURE_SUCCESS(rv, rv);
      mIsUpdating = false;
      mChannel = nsnull;
    }
    if (mTimer) {
      mTimer->Cancel();
      mTimer = nsnull;
    }
  }
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// nsIBadCertListener2 implementation

NS_IMETHODIMP
nsUrlClassifierStreamUpdater::NotifyCertProblem(nsIInterfaceRequestor *socketInfo, 
                                                nsISSLStatus *status, 
                                                const nsACString &targetSite, 
                                                bool *_retval)
{
  *_retval = true;
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// nsISSLErrorListener implementation

NS_IMETHODIMP
nsUrlClassifierStreamUpdater::NotifySSLError(nsIInterfaceRequestor *socketInfo, 
                                             PRInt32 error, 
                                             const nsACString &targetSite, 
                                             bool *_retval)
{
  *_retval = true;
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

  mTimer = nsnull;

  // Start the update process up again.
  FetchNext();

  return NS_OK;
}

