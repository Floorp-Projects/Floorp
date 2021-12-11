//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUrlClassifierStreamUpdater_h_
#define nsUrlClassifierStreamUpdater_h_

#include <nsISupportsUtils.h>

#include "nsCOMPtr.h"
#include "nsINamed.h"
#include "nsIObserver.h"
#include "nsIUrlClassifierStreamUpdater.h"
#include "nsIStreamListener.h"
#include "nsIChannel.h"
#include "nsTArray.h"
#include "nsITimer.h"
#include "mozilla/Attributes.h"

// Forward declare pointers
class nsIURI;

class nsUrlClassifierStreamUpdater final
    : public nsIUrlClassifierStreamUpdater,
      public nsIUrlClassifierUpdateObserver,
      public nsIStreamListener,
      public nsIObserver,
      public nsIInterfaceRequestor,
      public nsITimerCallback,
      public nsINamed {
 public:
  nsUrlClassifierStreamUpdater();

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIURLCLASSIFIERSTREAMUPDATER
  NS_DECL_NSIURLCLASSIFIERUPDATEOBSERVER
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIOBSERVER
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSINAMED

 private:
  // No subclassing
  ~nsUrlClassifierStreamUpdater() = default;

  // When the dbservice sends an UpdateComplete or UpdateFailure, we call this
  // to reset the stream updater.
  void DownloadDone();

  // Disallow copy constructor
  nsUrlClassifierStreamUpdater(nsUrlClassifierStreamUpdater&);

  nsresult AddRequestBody(const nsACString& aRequestBody);

  // Fetches an update for a single table.
  nsresult FetchUpdate(nsIURI* aURI, const nsACString& aRequest,
                       bool aIsPostRequest, const nsACString& aTable);
  // Dumb wrapper so we don't have to create URIs.
  nsresult FetchUpdate(const nsACString& aURI, const nsACString& aRequest,
                       bool aIsPostRequest, const nsACString& aTable);

  // Fetches the next table, from mPendingUpdates.
  nsresult FetchNext();
  // Fetches the next request, from mPendingRequests
  nsresult FetchNextRequest();

  struct UpdateRequest {
    nsCString mTables;
    nsCString mRequestPayload;
    bool mIsPostRequest;
    nsCString mUrl;
    nsCOMPtr<nsIUrlClassifierCallback> mSuccessCallback;
    nsCOMPtr<nsIUrlClassifierCallback> mUpdateErrorCallback;
    nsCOMPtr<nsIUrlClassifierCallback> mDownloadErrorCallback;
  };
  // Utility function to create an update request.
  void BuildUpdateRequest(const nsACString& aRequestTables,
                          const nsACString& aRequestPayload,
                          bool aIsPostRequest, const nsACString& aUpdateUrl,
                          nsIUrlClassifierCallback* aSuccessCallback,
                          nsIUrlClassifierCallback* aUpdateErrorCallback,
                          nsIUrlClassifierCallback* aDownloadErrorCallback,
                          UpdateRequest* aRequest);

  bool mIsUpdating;
  bool mInitialized;
  bool mDownloadError;
  bool mBeganStream;

  nsCString mDownloadErrorStatusStr;

  // Note that mStreamTable is only used by v2, it is empty for v4 update.
  nsCString mStreamTable;

  nsCOMPtr<nsIChannel> mChannel;
  nsCOMPtr<nsIUrlClassifierDBService> mDBService;

  // In v2, a update response might contain redirection and this
  // timer is for fetching the redirected update.
  nsCOMPtr<nsITimer> mFetchIndirectUpdatesTimer;

  // When we DownloadUpdate(), the DBService might be busy on processing
  // request issused outside of StreamUpdater. We have to fire a timer to
  // retry on our own.
  nsCOMPtr<nsITimer> mFetchNextRequestTimer;

  // Timer to abort the download if the server takes too long to respond.
  nsCOMPtr<nsITimer> mResponseTimeoutTimer;

  // Timer to abort the download if it takes too long.
  nsCOMPtr<nsITimer> mTimeoutTimer;

  mozilla::UniquePtr<UpdateRequest> mCurrentRequest;
  nsTArray<UpdateRequest> mPendingRequests;

  struct PendingUpdate {
    nsCString mUrl;
    nsCString mTable;
  };
  nsTArray<PendingUpdate> mPendingUpdates;

  // The provider for current update request and should be only used by
  // telemetry since it would show up as "other" for any other providers.
  nsCString mTelemetryProvider;
  PRIntervalTime mTelemetryClockStart;
};

#endif  // nsUrlClassifierStreamUpdater_h_
