//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUrlClassifierStreamUpdater_h_
#define nsUrlClassifierStreamUpdater_h_

#include <nsISupportsUtils.h>

#include "nsCOMPtr.h"
#include "nsIObserver.h"
#include "nsIUrlClassifierStreamUpdater.h"
#include "nsIStreamListener.h"
#include "nsNetUtil.h"
#include "nsTArray.h"
#include "nsIBadCertListener2.h"
#include "nsISSLErrorListener.h"
#include "nsITimer.h"

// Forward declare pointers
class nsIURI;

class nsUrlClassifierStreamUpdater : public nsIUrlClassifierStreamUpdater,
                                     public nsIUrlClassifierUpdateObserver,
                                     public nsIStreamListener,
                                     public nsIObserver,
                                     public nsIBadCertListener2,
                                     public nsISSLErrorListener,
                                     public nsIInterfaceRequestor,
                                     public nsITimerCallback
{
public:
  nsUrlClassifierStreamUpdater();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIURLCLASSIFIERSTREAMUPDATER
  NS_DECL_NSIURLCLASSIFIERUPDATEOBSERVER
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIBADCERTLISTENER2
  NS_DECL_NSISSLERRORLISTENER
  NS_DECL_NSIOBSERVER
  NS_DECL_NSITIMERCALLBACK

private:
  // No subclassing
  ~nsUrlClassifierStreamUpdater() {}

  // When the dbservice sends an UpdateComplete or UpdateFailure, we call this
  // to reset the stream updater.
  void DownloadDone();

  // Disallow copy constructor
  nsUrlClassifierStreamUpdater(nsUrlClassifierStreamUpdater&);

  nsresult AddRequestBody(const nsACString &aRequestBody);

  nsresult FetchUpdate(nsIURI *aURI,
                       const nsACString &aRequestBody,
                       const nsACString &aTable,
                       const nsACString &aServerMAC);
  nsresult FetchUpdate(const nsACString &aURI,
                       const nsACString &aRequestBody,
                       const nsACString &aTable,
                       const nsACString &aServerMAC);

  nsresult FetchNext();

  bool mIsUpdating;
  bool mInitialized;
  bool mDownloadError;
  bool mBeganStream;
  nsCOMPtr<nsIURI> mUpdateUrl;
  nsCString mStreamTable;
  nsCString mServerMAC;
  nsCOMPtr<nsIChannel> mChannel;
  nsCOMPtr<nsIUrlClassifierDBService> mDBService;
  nsCOMPtr<nsITimer> mTimer;

  struct PendingUpdate {
    nsCString mUrl;
    nsCString mTable;
    nsCString mServerMAC;
  };
  nsTArray<PendingUpdate> mPendingUpdates;

  nsCOMPtr<nsIUrlClassifierCallback> mSuccessCallback;
  nsCOMPtr<nsIUrlClassifierCallback> mUpdateErrorCallback;
  nsCOMPtr<nsIUrlClassifierCallback> mDownloadErrorCallback;
};

#endif // nsUrlClassifierStreamUpdater_h_
