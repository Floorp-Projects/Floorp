/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUrlClassifierProxies_h
#define nsUrlClassifierProxies_h

#include "nsIUrlClassifierDBService.h"
#include "nsUrlClassifierDBService.h"
#include "nsProxyRelease.h"
#include "nsThreadUtils.h"
#include "mozilla/Attributes.h"
#include "nsIPrincipal.h"
#include "LookupCache.h"


/**
 * Thread proxy from the main thread to the worker thread.
 */
class UrlClassifierDBServiceWorkerProxy final : public nsIUrlClassifierDBService
{
public:
  explicit UrlClassifierDBServiceWorkerProxy(nsUrlClassifierDBServiceWorker* aTarget)
    : mTarget(aTarget)
  { }

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIURLCLASSIFIERDBSERVICE

  class LookupRunnable : public mozilla::Runnable
  {
  public:
    LookupRunnable(nsUrlClassifierDBServiceWorker* aTarget,
                   nsIPrincipal* aPrincipal,
                   const nsACString& aTables,
                   nsIUrlClassifierCallback* aCB)
      : mTarget(aTarget)
      , mPrincipal(aPrincipal)
      , mLookupTables(aTables)
      , mCB(aCB)
    { }

    NS_DECL_NSIRUNNABLE

  private:
    RefPtr<nsUrlClassifierDBServiceWorker> mTarget;
    nsCOMPtr<nsIPrincipal> mPrincipal;
    nsCString mLookupTables;
    nsCOMPtr<nsIUrlClassifierCallback> mCB;
  };

  class GetTablesRunnable : public mozilla::Runnable
  {
  public:
    GetTablesRunnable(nsUrlClassifierDBServiceWorker* aTarget,
                      nsIUrlClassifierCallback* aCB)
      : mTarget(aTarget)
      , mCB(aCB)
    { }

    NS_DECL_NSIRUNNABLE

  private:
    RefPtr<nsUrlClassifierDBServiceWorker> mTarget;
    nsCOMPtr<nsIUrlClassifierCallback> mCB;
  };

  class BeginUpdateRunnable : public mozilla::Runnable
  {
  public:
    BeginUpdateRunnable(nsUrlClassifierDBServiceWorker* aTarget,
                        nsIUrlClassifierUpdateObserver* aUpdater,
                        const nsACString& aTables)
      : mTarget(aTarget)
      , mUpdater(aUpdater)
      , mTables(aTables)
    { }

    NS_DECL_NSIRUNNABLE

  private:
    RefPtr<nsUrlClassifierDBServiceWorker> mTarget;
    nsCOMPtr<nsIUrlClassifierUpdateObserver> mUpdater;
    nsCString mTables;
  };

  class BeginStreamRunnable : public mozilla::Runnable
  {
  public:
    BeginStreamRunnable(nsUrlClassifierDBServiceWorker* aTarget,
                        const nsACString& aTable)
      : mTarget(aTarget)
      , mTable(aTable)
    { }

    NS_DECL_NSIRUNNABLE

  private:
    RefPtr<nsUrlClassifierDBServiceWorker> mTarget;
    nsCString mTable;
  };

  class UpdateStreamRunnable : public mozilla::Runnable
  {
  public:
    UpdateStreamRunnable(nsUrlClassifierDBServiceWorker* aTarget,
                         const nsACString& aUpdateChunk)
      : mTarget(aTarget)
      , mUpdateChunk(aUpdateChunk)
    { }

    NS_DECL_NSIRUNNABLE

  private:
    RefPtr<nsUrlClassifierDBServiceWorker> mTarget;
    nsCString mUpdateChunk;
  };

  class CacheCompletionsRunnable : public mozilla::Runnable
  {
  public:
    CacheCompletionsRunnable(nsUrlClassifierDBServiceWorker* aTarget,
                             mozilla::safebrowsing::CacheResultArray *aEntries)
      : mTarget(aTarget)
      , mEntries(aEntries)
    { }

    NS_DECL_NSIRUNNABLE

  private:
    RefPtr<nsUrlClassifierDBServiceWorker> mTarget;
     mozilla::safebrowsing::CacheResultArray *mEntries;
  };

  class DoLocalLookupRunnable : public mozilla::Runnable
  {
  public:
    DoLocalLookupRunnable(nsUrlClassifierDBServiceWorker* aTarget,
                          const nsACString& spec,
                          const nsACString& tables,
                          mozilla::safebrowsing::LookupResultArray* results)
      : mTarget(aTarget)
      , mSpec(spec)
      , mTables(tables)
      , mResults(results)
    { }

    NS_DECL_NSIRUNNABLE
  private:
    RefPtr<nsUrlClassifierDBServiceWorker> mTarget;

    nsCString mSpec;
    nsCString mTables;
    mozilla::safebrowsing::LookupResultArray* mResults;
  };

  class ClearLastResultsRunnable : public mozilla::Runnable
  {
  public:
    explicit ClearLastResultsRunnable(nsUrlClassifierDBServiceWorker* aTarget)
      : mTarget(aTarget)
    { }

    NS_DECL_NSIRUNNABLE
  private:
    RefPtr<nsUrlClassifierDBServiceWorker> mTarget;
  };

  class GetCacheInfoRunnable: public mozilla::Runnable
  {
  public:
    explicit GetCacheInfoRunnable(nsUrlClassifierDBServiceWorker* aTarget,
                                  const nsACString& aTable,
                                  nsIUrlClassifierCacheInfo** aCache)
      : mTarget(aTarget),
        mTable(aTable),
        mCache(aCache)
    { }

    NS_DECL_NSIRUNNABLE
  private:
    RefPtr<nsUrlClassifierDBServiceWorker> mTarget;
    nsCString mTable;
    nsIUrlClassifierCacheInfo** mCache;
  };

public:
  nsresult DoLocalLookup(const nsACString& spec,
                         const nsACString& tables,
                         mozilla::safebrowsing::LookupResultArray* results);

  nsresult OpenDb();
  nsresult CloseDb();

  nsresult CacheCompletions(mozilla::safebrowsing::CacheResultArray * aEntries);

  nsresult GetCacheInfo(const nsACString& aTable,
                        nsIUrlClassifierCacheInfo** aCache);
private:
  ~UrlClassifierDBServiceWorkerProxy() {}

  RefPtr<nsUrlClassifierDBServiceWorker> mTarget;
};

// The remaining classes here are all proxies to the main thread

class UrlClassifierLookupCallbackProxy final :
  public nsIUrlClassifierLookupCallback
{
public:
  explicit UrlClassifierLookupCallbackProxy(nsIUrlClassifierLookupCallback* aTarget)
    : mTarget(new nsMainThreadPtrHolder<nsIUrlClassifierLookupCallback>(
        "UrlClassifierLookupCallbackProxy::mTarget", aTarget))
  { }

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIURLCLASSIFIERLOOKUPCALLBACK

  class LookupCompleteRunnable : public mozilla::Runnable
  {
  public:
    LookupCompleteRunnable(const nsMainThreadPtrHandle<nsIUrlClassifierLookupCallback>& aTarget,
                           mozilla::safebrowsing::LookupResultArray *aResults)
      : mTarget(aTarget)
      , mResults(aResults)
    { }

    NS_DECL_NSIRUNNABLE

  private:
    nsMainThreadPtrHandle<nsIUrlClassifierLookupCallback> mTarget;
    mozilla::safebrowsing::LookupResultArray * mResults;
  };

private:
  ~UrlClassifierLookupCallbackProxy() {}

  nsMainThreadPtrHandle<nsIUrlClassifierLookupCallback> mTarget;
};

class UrlClassifierCallbackProxy final : public nsIUrlClassifierCallback
{
public:
  explicit UrlClassifierCallbackProxy(nsIUrlClassifierCallback* aTarget)
    : mTarget(new nsMainThreadPtrHolder<nsIUrlClassifierCallback>(
        "UrlClassifierCallbackProxy::mTarget", aTarget))
  { }

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIURLCLASSIFIERCALLBACK

  class HandleEventRunnable : public mozilla::Runnable
  {
  public:
    HandleEventRunnable(const nsMainThreadPtrHandle<nsIUrlClassifierCallback>& aTarget,
                        const nsACString& aValue)
      : mTarget(aTarget)
      , mValue(aValue)
    { }

    NS_DECL_NSIRUNNABLE

  private:
    nsMainThreadPtrHandle<nsIUrlClassifierCallback> mTarget;
    nsCString mValue;
  };

private:
  ~UrlClassifierCallbackProxy() {}

  nsMainThreadPtrHandle<nsIUrlClassifierCallback> mTarget;
};

class UrlClassifierUpdateObserverProxy final :
  public nsIUrlClassifierUpdateObserver
{
public:
  explicit UrlClassifierUpdateObserverProxy(nsIUrlClassifierUpdateObserver* aTarget)
    : mTarget(new nsMainThreadPtrHolder<nsIUrlClassifierUpdateObserver>(
        "UrlClassifierUpdateObserverProxy::mTarget", aTarget))
  { }

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIURLCLASSIFIERUPDATEOBSERVER

  class UpdateUrlRequestedRunnable : public mozilla::Runnable
  {
  public:
    UpdateUrlRequestedRunnable(const nsMainThreadPtrHandle<nsIUrlClassifierUpdateObserver>& aTarget,
                               const nsACString& aURL,
                               const nsACString& aTable)
      : mTarget(aTarget)
      , mURL(aURL)
      , mTable(aTable)
    { }

    NS_DECL_NSIRUNNABLE

  private:
    nsMainThreadPtrHandle<nsIUrlClassifierUpdateObserver> mTarget;
    nsCString mURL, mTable;
  };

  class StreamFinishedRunnable : public mozilla::Runnable
  {
  public:
    StreamFinishedRunnable(const nsMainThreadPtrHandle<nsIUrlClassifierUpdateObserver>& aTarget,
                           nsresult aStatus, uint32_t aDelay)
      : mTarget(aTarget)
      , mStatus(aStatus)
      , mDelay(aDelay)
    { }

    NS_DECL_NSIRUNNABLE

  private:
    nsMainThreadPtrHandle<nsIUrlClassifierUpdateObserver> mTarget;
    nsresult mStatus;
    uint32_t mDelay;
  };

  class UpdateErrorRunnable : public mozilla::Runnable
  {
  public:
    UpdateErrorRunnable(const nsMainThreadPtrHandle<nsIUrlClassifierUpdateObserver>& aTarget,
                        nsresult aError)
      : mTarget(aTarget)
      , mError(aError)
    { }

    NS_DECL_NSIRUNNABLE

  private:
    nsMainThreadPtrHandle<nsIUrlClassifierUpdateObserver> mTarget;
    nsresult mError;
  };

  class UpdateSuccessRunnable : public mozilla::Runnable
  {
  public:
    UpdateSuccessRunnable(const nsMainThreadPtrHandle<nsIUrlClassifierUpdateObserver>& aTarget,
                          uint32_t aRequestedTimeout)
      : mTarget(aTarget)
      , mRequestedTimeout(aRequestedTimeout)
    { }

    NS_DECL_NSIRUNNABLE

  private:
    nsMainThreadPtrHandle<nsIUrlClassifierUpdateObserver> mTarget;
    uint32_t mRequestedTimeout;
  };

private:
  ~UrlClassifierUpdateObserverProxy() {}

  nsMainThreadPtrHandle<nsIUrlClassifierUpdateObserver> mTarget;
};

#endif // nsUrlClassifierProxies_h
