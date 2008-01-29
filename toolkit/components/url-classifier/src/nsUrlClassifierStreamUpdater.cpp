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
  : mIsUpdating(PR_FALSE), mInitialized(PR_FALSE), mUpdateUrl(nsnull),
    mChannel(nsnull)
{
#if defined(PR_LOGGING)
  if (!gUrlClassifierStreamUpdaterLog)
    gUrlClassifierStreamUpdaterLog = PR_NewLogModule("UrlClassifierStreamUpdater");
#endif

}

NS_IMPL_ISUPPORTS8(nsUrlClassifierStreamUpdater,
                   nsIUrlClassifierStreamUpdater,
                   nsIUrlClassifierUpdateObserver,
                   nsIRequestObserver,
                   nsIStreamListener,
                   nsIObserver,
                   nsIBadCertListener2,
                   nsISSLErrorListener,
                   nsIInterfaceRequestor)

/**
 * Clear out the update.
 */
void
nsUrlClassifierStreamUpdater::DownloadDone()
{
  LOG(("nsUrlClassifierStreamUpdater::DownloadDone [this=%p]", this));
  mIsUpdating = PR_FALSE;

  mPendingUpdateUrls.Clear();
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
                                          const nsACString & aRequestBody)
{
  nsresult rv;
  rv = NS_NewChannel(getter_AddRefs(mChannel), aUpdateUrl, nsnull, nsnull, this);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!aRequestBody.IsEmpty()) {
    rv = AddRequestBody(aRequestBody);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Make the request
  rv = mChannel->AsyncOpen(this, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsUrlClassifierStreamUpdater::FetchUpdate(const nsACString & aUpdateUrl,
                                          const nsACString & aRequestBody)
{
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aUpdateUrl);
  NS_ENSURE_SUCCESS(rv, rv);

  return FetchUpdate(uri, aRequestBody);
}

NS_IMETHODIMP
nsUrlClassifierStreamUpdater::DownloadUpdates(
                                const nsACString &aRequestBody,
                                nsIUrlClassifierCallback *aSuccessCallback,
                                nsIUrlClassifierCallback *aUpdateErrorCallback,
                                nsIUrlClassifierCallback *aDownloadErrorCallback,
                                PRBool *_retval)
{
  NS_ENSURE_ARG(aSuccessCallback);
  NS_ENSURE_ARG(aUpdateErrorCallback);
  NS_ENSURE_ARG(aDownloadErrorCallback);

  if (mIsUpdating) {
    LOG(("already updating, skipping update"));
    *_retval = PR_FALSE;
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
        do_GetService("@mozilla.org/observer-service;1");
    if (!observerService)
      return NS_ERROR_FAILURE;

    observerService->AddObserver(this, gQuitApplicationMessage, PR_FALSE);

    mDBService = do_GetService(NS_URLCLASSIFIERDBSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    mInitialized = PR_TRUE;
  }

  rv = mDBService->BeginUpdate(this);
  if (rv == NS_ERROR_NOT_AVAILABLE) {
    LOG(("already updating, skipping update"));
    *_retval = PR_FALSE;
    return NS_OK;
  } else if (NS_FAILED(rv)) {
    return rv;
  }

  mSuccessCallback = aSuccessCallback;
  mUpdateErrorCallback = aUpdateErrorCallback;
  mDownloadErrorCallback = aDownloadErrorCallback;

  mIsUpdating = PR_TRUE;
  *_retval = PR_TRUE;


  return FetchUpdate(mUpdateUrl, aRequestBody);
}

///////////////////////////////////////////////////////////////////////////////
// nsIUrlClassifierUpdateObserver implementation

NS_IMETHODIMP
nsUrlClassifierStreamUpdater::UpdateUrlRequested(const nsACString &aUrl)
{
  LOG(("Queuing requested update from %s\n", PromiseFlatCString(aUrl).get()));

  // Allow data: urls for unit testing purposes, otherwise assume http
  if (StringBeginsWith(aUrl, NS_LITERAL_CSTRING("data:"))) {
    mPendingUpdateUrls.AppendElement(aUrl);
  } else {
    mPendingUpdateUrls.AppendElement(NS_LITERAL_CSTRING("http://") + aUrl);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierStreamUpdater::StreamFinished()
{
  nsresult rv;

  // Pop off a pending URL and update it.
  if (mPendingUpdateUrls.Length() > 0) {
    rv = FetchUpdate(mPendingUpdateUrls[0], NS_LITERAL_CSTRING(""));
    if (NS_FAILED(rv)) {
      LOG(("Error fetching update url: %s\n", mPendingUpdateUrls[0].get()));
      mDBService->CancelUpdate();
      return rv;
    }

    mPendingUpdateUrls.RemoveElementAt(0);
  } else {
    mDBService->FinishUpdate();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierStreamUpdater::UpdateSuccess(PRUint32 requestedTimeout)
{
  LOG(("nsUrlClassifierStreamUpdater::UpdateSuccess [this=%p]", this));
  NS_ASSERTION(mPendingUpdateUrls.Length() == 0,
               "Didn't fetch all update URLs.");

  // DownloadDone() clears mSuccessCallback, so we save it off here.
  nsCOMPtr<nsIUrlClassifierCallback> successCallback = mSuccessCallback;
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
  nsCOMPtr<nsIUrlClassifierCallback> errorCallback = mUpdateErrorCallback;
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
nsUrlClassifierStreamUpdater::OnStartRequest(nsIRequest *request, nsISupports* context)
{
  nsresult rv;

  rv = mDBService->BeginStream();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(request);
  if (httpChannel) {
    nsresult status;
    rv = httpChannel->GetStatus(&status);
    NS_ENSURE_SUCCESS(rv, rv);

    LOG(("OnStartRequest (status %x)", status));

    if (NS_ERROR_CONNECTION_REFUSED == status ||
        NS_ERROR_NET_TIMEOUT == status) {
      // Assume that we're overloading the server and trigger backoff.
      mDownloadErrorCallback->HandleEvent(nsCString());
      return NS_ERROR_ABORT;
    }
  }

  return NS_OK;
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

  // Only update if we got http success header
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(request);
  if (httpChannel) {
    PRBool succeeded = PR_FALSE;
    rv = httpChannel->GetRequestSucceeded(&succeeded);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!succeeded) {
      // 404 or other error, pass error status back
      LOG(("HTTP request returned failure code."));

      PRUint32 status;
      rv = httpChannel->GetResponseStatus(&status);
      NS_ENSURE_SUCCESS(rv, rv);

      nsCAutoString strStatus;
      strStatus.AppendInt(status);
      mDownloadErrorCallback->HandleEvent(strStatus);
      return NS_ERROR_ABORT;
    }
  }

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

  // If we got the whole stream, call FinishStream to process the changes.
  // Otherwise, call CancelStream to rollback the changes.
  if (NS_SUCCEEDED(aStatus))
    rv = mDBService->FinishStream();
  else
    rv = mDBService->CancelUpdate();

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
      mIsUpdating = PR_FALSE;
      mChannel = nsnull;
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
                                                PRBool *_retval)
{
  *_retval = PR_TRUE;
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// nsISSLErrorListener implementation

NS_IMETHODIMP
nsUrlClassifierStreamUpdater::NotifySSLError(nsIInterfaceRequestor *socketInfo, 
                                             PRInt32 error, 
                                             const nsACString &targetSite, 
                                             PRBool *_retval)
{
  *_retval = PR_TRUE;
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// nsIInterfaceRequestor implementation

NS_IMETHODIMP
nsUrlClassifierStreamUpdater::GetInterface(const nsIID & eventSinkIID, void* *_retval)
{
  return QueryInterface(eventSinkIID, _retval);
}
