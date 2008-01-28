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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dave Camp <dcamp@mozilla.com>
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

#ifndef nsUrlClassifierHashCompleter_h_
#define nsUrlClassifierHashCompleter_h_

#include "nsIUrlClassifierHashCompleter.h"

#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsIChannel.h"
#include "nsIObserver.h"
#include "nsIRunnable.h"
#include "nsIStreamListener.h"
#include "nsIURI.h"
#include "nsTArray.h"
#include "nsString.h"
#include "nsWeakReference.h"

class nsUrlClassifierHashCompleterRequest : public nsIStreamListener
                                          , public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIOBSERVER

  nsUrlClassifierHashCompleterRequest(nsIURI *uri)
    : mShuttingDown(PR_FALSE)
    , mURI(uri) { }
  ~nsUrlClassifierHashCompleterRequest() { }

  nsresult Begin();
  nsresult Add(const nsACString &partialHash,
               nsIUrlClassifierHashCompleterCallback *c);

private:
  nsresult OpenChannel();
  nsresult BuildRequest(nsCAutoString &request);
  nsresult AddRequestBody(const nsACString &requestBody);
  nsresult HandleItem(const nsACString &item,
                      const nsACString &table,
                      PRUint32 item);
  nsresult HandleTable(const nsACString &response,
                       nsACString::const_iterator &begin);
  nsresult HandleResponse();
  void NotifySuccess();
  void NotifyFailure(nsresult status);

  PRBool mShuttingDown;
  nsCOMPtr<nsIURI> mURI;
  nsCOMPtr<nsIChannel> mChannel;
  nsCString mResponse;

  struct Response {
    nsCString completeHash;
    nsCString tableName;
    PRUint32 chunkId;
  };

  struct Request {
    nsCString partialHash;
    nsTArray<Response> responses;
    nsCOMPtr<nsIUrlClassifierHashCompleterCallback> callback;
  };

  nsTArray<Request> mRequests;
};

class nsUrlClassifierHashCompleter : public nsIUrlClassifierHashCompleter
                                   , public nsIRunnable
                                   , public nsIObserver
                                   , public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIURLCLASSIFIERHASHCOMPLETER
  NS_DECL_NSIRUNNABLE
  NS_DECL_NSIOBSERVER

  nsUrlClassifierHashCompleter() : mShuttingDown(PR_FALSE) {}
  ~nsUrlClassifierHashCompleter() {}

  nsresult Init();

private:
  nsRefPtr<nsUrlClassifierHashCompleterRequest> mRequest;
  nsCOMPtr<nsIURI> mURI;
  PRBool mShuttingDown;

};

#endif // nsUrlClassifierHashCompleter_h_
