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
#include "nsIObserverService.h"
#include "nsIURI.h"
#include "nsIUrlClassifierDBService.h"
#include "nsStreamUtils.h"
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
// Stream listener for incremental download of url tables.

class nsUrlClassifierStreamUpdater;

class TableUpdateListener : public nsIStreamListener
{
public:
  TableUpdateListener(nsIUrlClassifierCallback *aTableCallback,
                      nsIUrlClassifierCallback *aErrorCallback,
                      nsUrlClassifierStreamUpdater* aStreamUpdater);
  nsCOMPtr<nsIUrlClassifierDBService> mDBService;

  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER

private:
  ~TableUpdateListener() {}

  // Callback when table updates complete.
  nsCOMPtr<nsIUrlClassifierCallback> mTableCallback;
  nsCOMPtr<nsIUrlClassifierCallback> mErrorCallback;

  // Reference to the stream updater that created this.
  nsUrlClassifierStreamUpdater *mStreamUpdater;
};

TableUpdateListener::TableUpdateListener(
                                nsIUrlClassifierCallback *aTableCallback,
                                nsIUrlClassifierCallback *aErrorCallback,
                                nsUrlClassifierStreamUpdater* aStreamUpdater)
{
  mTableCallback = aTableCallback;
  mErrorCallback = aErrorCallback;
  mStreamUpdater = aStreamUpdater;
}

NS_IMPL_ISUPPORTS2(TableUpdateListener, nsIStreamListener, nsIRequestObserver)

NS_IMETHODIMP
TableUpdateListener::OnStartRequest(nsIRequest *request, nsISupports* context)
{
  nsresult rv;
  if (!mDBService) {
    mDBService = do_GetService(NS_URLCLASSIFIERDBSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(request);
  NS_ENSURE_STATE(httpChannel);

  nsresult status;
  rv = httpChannel->GetStatus(&status);
  NS_ENSURE_SUCCESS(rv, rv);
  if (NS_ERROR_CONNECTION_REFUSED == status ||
      NS_ERROR_NET_TIMEOUT == status) {
    // Assume that we're overloading the server and trigger backoff.
    mErrorCallback->HandleEvent(nsCString());
    return NS_ERROR_ABORT;
  }

  return NS_OK;
}

NS_IMETHODIMP
TableUpdateListener::OnDataAvailable(nsIRequest *request,
                                     nsISupports* context,
                                     nsIInputStream *aIStream,
                                     PRUint32 aSourceOffset,
                                     PRUint32 aLength)
{
  if (!mDBService)
    return NS_ERROR_NOT_INITIALIZED;

  LOG(("OnDataAvailable (%d bytes)", aLength));

  // Only update if we got http success header
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(request);
  NS_ENSURE_STATE(httpChannel);

  nsresult rv;
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
    mErrorCallback->HandleEvent(strStatus);
    return NS_ERROR_ABORT;
  }

  // Copy the data into a nsCString
  nsCString chunk;
  rv = NS_ConsumeStream(aIStream, aLength, chunk);
  NS_ENSURE_SUCCESS(rv, rv);

  //LOG(("Chunk (%d): %s\n\n", chunk.Length(), chunk.get()));

  rv = mDBService->Update(chunk);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
TableUpdateListener::OnStopRequest(nsIRequest *request, nsISupports* context,
                                   nsresult aStatus)
{
  if (!mDBService)
    return NS_ERROR_NOT_INITIALIZED;

  LOG(("OnStopRequest status: %d", aStatus));

  // If we got the whole stream, call Finish to commit the changes.
  // Otherwise, call Cancel to rollback the changes.
  if (NS_SUCCEEDED(aStatus))
    mDBService->Finish(mTableCallback);
  else
    mDBService->CancelStream();

  mStreamUpdater->DownloadDone();

  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// nsIUrlClassiferStreamUpdater implementation
// Handles creating/running the stream listener

nsUrlClassifierStreamUpdater::nsUrlClassifierStreamUpdater()
  : mIsUpdating(PR_FALSE), mInitialized(PR_FALSE), mUpdateUrl(nsnull),
   mListener(nsnull), mChannel(nsnull)
{
#if defined(PR_LOGGING)
  if (!gUrlClassifierStreamUpdaterLog)
    gUrlClassifierStreamUpdaterLog = PR_NewLogModule("UrlClassifierStreamUpdater");
#endif

}

NS_IMPL_ISUPPORTS2(nsUrlClassifierStreamUpdater,
                   nsIUrlClassifierStreamUpdater,
                   nsIObserver)

/**
 * Drop our reference to mChannel if we have one.
 */
void
nsUrlClassifierStreamUpdater::DownloadDone()
{
  mIsUpdating = PR_FALSE;
  mChannel = nsnull;
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
  nsresult rv = NS_NewURI(getter_AddRefs(mUpdateUrl), aUpdateUrl);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierStreamUpdater::DownloadUpdates(
                                nsIUrlClassifierCallback *aTableCallback,
                                nsIUrlClassifierCallback *aErrorCallback,
                                PRBool *_retval)
{
  if (mIsUpdating) {
    LOG(("already updating, skipping update"));
    *_retval = PR_FALSE;
    return NS_OK;
  }

  if (!mUpdateUrl) {
    NS_ERROR("updateUrl not set");
    return NS_ERROR_NOT_INITIALIZED;
  }

  if (!mInitialized) {
    // Add an observer for shutdown so we can cancel any pending list
    // downloads.  quit-application is the same event that the download
    // manager listens for and uses to cancel pending downloads.
    nsCOMPtr<nsIObserverService> observerService =
        do_GetService("@mozilla.org/observer-service;1");
    if (!observerService)
      return NS_ERROR_FAILURE;

    observerService->AddObserver(this, gQuitApplicationMessage, PR_FALSE);
    mInitialized = PR_TRUE;
  }

  // Ok, try to create the download channel.
  nsresult rv;
  rv = NS_NewChannel(getter_AddRefs(mChannel), mUpdateUrl);
  NS_ENSURE_SUCCESS(rv, rv);

  // Bind to a different callback each time we invoke this method.
  mListener = new TableUpdateListener(aTableCallback, aErrorCallback, this);

  // Make the request
  rv = mChannel->AsyncOpen(mListener.get(), nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  mIsUpdating = PR_TRUE;
  *_retval = PR_TRUE;

  return NS_OK;
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
