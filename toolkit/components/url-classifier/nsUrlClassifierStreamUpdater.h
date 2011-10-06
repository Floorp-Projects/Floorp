//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-/
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
