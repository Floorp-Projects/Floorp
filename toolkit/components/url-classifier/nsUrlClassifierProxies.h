/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUrlClassifierProxies_h
#define nsUrlClassifierProxies_h

#include "nsIUrlClassifierDBService.h"
#include "nsThreadUtils.h"
#include "mozilla/Attributes.h"
#include "nsIPrincipal.h"

/**
 * Thread proxy from the main thread to the worker thread.
 */
class UrlClassifierDBServiceWorkerProxy MOZ_FINAL :
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
                   nsIPrincipal* aPrincipal,
                   nsIUrlClassifierCallback* aCB)
      : mTarget(aTarget)
      , mPrincipal(aPrincipal)
      , mCB(aCB)
    { }

    NS_DECL_NSIRUNNABLE

  private:
    nsCOMPtr<nsIUrlClassifierDBServiceWorker> mTarget;
    nsCOMPtr<nsIPrincipal> mPrincipal;
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

class UrlClassifierLookupCallbackProxy MOZ_FINAL :
  public nsIUrlClassifierLookupCallback
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

class UrlClassifierCallbackProxy MOZ_FINAL : public nsIUrlClassifierCallback
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

class UrlClassifierUpdateObserverProxy MOZ_FINAL :
  public nsIUrlClassifierUpdateObserver
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
