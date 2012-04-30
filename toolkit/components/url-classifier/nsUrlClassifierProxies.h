/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Firefox.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation <http://www.mozilla.org>.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef nsUrlClassifierProxies_h
#define nsUrlClassifierProxies_h

#include "nsIUrlClassifierDBService.h"
#include "nsThreadUtils.h"

/**
 * Thread proxy from the main thread to the worker thread.
 */
class UrlClassifierDBServiceWorkerProxy :
  public nsIUrlClassifierDBServiceWorker
{
public:
  UrlClassifierDBServiceWorkerProxy(nsIUrlClassifierDBServiceWorker* aTarget)
    : mTarget(aTarget)
  { }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIURLCLASSIFIERDBSERVICE
  NS_DECL_NSIURLCLASSIFIERDBSERVICEWORKER

  class LookupRunnable : public nsRunnable
  {
  public:
    LookupRunnable(nsIUrlClassifierDBServiceWorker* aTarget,
                   const nsACString& aSpec,
                   nsIUrlClassifierCallback* aCB)
      : mTarget(aTarget)
      , mSpec(aSpec)
      , mCB(aCB)
    { }

    NS_DECL_NSIRUNNABLE

  private:
    nsCOMPtr<nsIUrlClassifierDBServiceWorker> mTarget;
    nsCString mSpec;
    nsCOMPtr<nsIUrlClassifierCallback> mCB;
  };

  class GetTablesRunnable : public nsRunnable
  {
  public:
    GetTablesRunnable(nsIUrlClassifierDBServiceWorker* aTarget,
                      nsIUrlClassifierCallback* aCB)
      : mTarget(aTarget)
      , mCB(aCB)
    { }

    NS_DECL_NSIRUNNABLE

  private:
    nsCOMPtr<nsIUrlClassifierDBServiceWorker> mTarget;
    nsCOMPtr<nsIUrlClassifierCallback> mCB;
  };

  class BeginUpdateRunnable : public nsRunnable
  {
  public:
    BeginUpdateRunnable(nsIUrlClassifierDBServiceWorker* aTarget,
                        nsIUrlClassifierUpdateObserver* aUpdater,
                        const nsACString& aTables,
                        const nsACString& aClientKey)
      : mTarget(aTarget)
      , mUpdater(aUpdater)
      , mTables(aTables)
      , mClientKey(aClientKey)
    { }

    NS_DECL_NSIRUNNABLE

  private:
    nsCOMPtr<nsIUrlClassifierDBServiceWorker> mTarget;
    nsCOMPtr<nsIUrlClassifierUpdateObserver> mUpdater;
    nsCString mTables, mClientKey;
  };

  class BeginStreamRunnable : public nsRunnable
  {
  public:
    BeginStreamRunnable(nsIUrlClassifierDBServiceWorker* aTarget,
                        const nsACString& aTable,
                        const nsACString& aServerMAC)
      : mTarget(aTarget)
      , mTable(aTable)
      , mServerMAC(aServerMAC)
    { }

    NS_DECL_NSIRUNNABLE

  private:
    nsCOMPtr<nsIUrlClassifierDBServiceWorker> mTarget;
    nsCString mTable, mServerMAC;
  };

  class UpdateStreamRunnable : public nsRunnable
  {
  public:
    UpdateStreamRunnable(nsIUrlClassifierDBServiceWorker* aTarget,
                         const nsACString& aUpdateChunk)
      : mTarget(aTarget)
      , mUpdateChunk(aUpdateChunk)
    { }

    NS_DECL_NSIRUNNABLE

  private:
    nsCOMPtr<nsIUrlClassifierDBServiceWorker> mTarget;
    nsCString mUpdateChunk;
  };

  class CacheCompletionsRunnable : public nsRunnable
  {
  public:
    CacheCompletionsRunnable(nsIUrlClassifierDBServiceWorker* aTarget,
                             nsTArray<nsUrlClassifierLookupResult>* aEntries)
      : mTarget(aTarget)
      , mEntries(aEntries)
    { }

    NS_DECL_NSIRUNNABLE

  private:
    nsCOMPtr<nsIUrlClassifierDBServiceWorker> mTarget;
    nsTArray<nsUrlClassifierLookupResult>* mEntries;
  };

private:
  nsCOMPtr<nsIUrlClassifierDBServiceWorker> mTarget;
};

// The remaining classes here are all proxies to the main thread

class UrlClassifierLookupCallbackProxy : public nsIUrlClassifierLookupCallback
{
public:
  UrlClassifierLookupCallbackProxy(nsIUrlClassifierLookupCallback* aTarget)
    : mTarget(aTarget)
  { }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIURLCLASSIFIERLOOKUPCALLBACK

  class LookupCompleteRunnable : public nsRunnable
  {
  public:
    LookupCompleteRunnable(nsIUrlClassifierLookupCallback* aTarget,
                           nsTArray<nsUrlClassifierLookupResult>* aResults)
      : mTarget(aTarget)
      , mResults(aResults)
    { }

    NS_DECL_NSIRUNNABLE

  private:
    nsCOMPtr<nsIUrlClassifierLookupCallback> mTarget;
    nsTArray<nsUrlClassifierLookupResult>* mResults;
  };

private:
  nsCOMPtr<nsIUrlClassifierLookupCallback> mTarget;
};

class UrlClassifierCallbackProxy : public nsIUrlClassifierCallback
{
public:
  UrlClassifierCallbackProxy(nsIUrlClassifierCallback* aTarget)
    : mTarget(aTarget)
  { }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIURLCLASSIFIERCALLBACK

  class HandleEventRunnable : public nsRunnable
  {
  public:
    HandleEventRunnable(nsIUrlClassifierCallback* aTarget,
                        const nsACString& aValue)
      : mTarget(aTarget)
      , mValue(aValue)
    { }

    NS_DECL_NSIRUNNABLE

  private:
    nsCOMPtr<nsIUrlClassifierCallback> mTarget;
    nsCString mValue;
  };

private:
  nsCOMPtr<nsIUrlClassifierCallback> mTarget;
};

class UrlClassifierUpdateObserverProxy : public nsIUrlClassifierUpdateObserver
{
public:
  UrlClassifierUpdateObserverProxy(nsIUrlClassifierUpdateObserver* aTarget)
    : mTarget(aTarget)
  { }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIURLCLASSIFIERUPDATEOBSERVER

  class UpdateUrlRequestedRunnable : public nsRunnable
  {
  public:
    UpdateUrlRequestedRunnable(nsIUrlClassifierUpdateObserver* aTarget,
                               const nsACString& aURL,
                               const nsACString& aTable,
                               const nsACString& aServerMAC)
      : mTarget(aTarget)
      , mURL(aURL)
      , mTable(aTable)
      , mServerMAC(aServerMAC)
    { }

    NS_DECL_NSIRUNNABLE

  private:
    nsCOMPtr<nsIUrlClassifierUpdateObserver> mTarget;
    nsCString mURL, mTable, mServerMAC;
  };

  class StreamFinishedRunnable : public nsRunnable
  {
  public:
    StreamFinishedRunnable(nsIUrlClassifierUpdateObserver* aTarget,
                           nsresult aStatus, PRUint32 aDelay)
      : mTarget(aTarget)
      , mStatus(aStatus)
      , mDelay(aDelay)
    { }

    NS_DECL_NSIRUNNABLE

  private:
    nsCOMPtr<nsIUrlClassifierUpdateObserver> mTarget;
    nsresult mStatus;
    PRUint32 mDelay;
  };

  class UpdateErrorRunnable : public nsRunnable
  {
  public:
    UpdateErrorRunnable(nsIUrlClassifierUpdateObserver* aTarget,
                        nsresult aError)
      : mTarget(aTarget)
      , mError(aError)
    { }

    NS_DECL_NSIRUNNABLE

  private:
    nsCOMPtr<nsIUrlClassifierUpdateObserver> mTarget;
    nsresult mError;
  };

  class UpdateSuccessRunnable : public nsRunnable
  {
  public:
    UpdateSuccessRunnable(nsIUrlClassifierUpdateObserver* aTarget,
                          PRUint32 aRequestedTimeout)
      : mTarget(aTarget)
      , mRequestedTimeout(aRequestedTimeout)
    { }

    NS_DECL_NSIRUNNABLE

  private:
    nsCOMPtr<nsIUrlClassifierUpdateObserver> mTarget;
    PRUint32 mRequestedTimeout;
  };

private:
  nsCOMPtr<nsIUrlClassifierUpdateObserver> mTarget;
};

#endif // nsUrlClassifierProxies_h
