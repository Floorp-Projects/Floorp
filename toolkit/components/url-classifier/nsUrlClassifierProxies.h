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
class UrlClassifierDBServiceWorkerProxy final
    : public nsIUrlClassifierDBService {
 public:
  explicit UrlClassifierDBServiceWorkerProxy(
      nsUrlClassifierDBServiceWorker* aTarget)
      : mTarget(aTarget) {}

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIURLCLASSIFIERDBSERVICE

  class LookupRunnable : public mozilla::Runnable {
   public:
    LookupRunnable(nsUrlClassifierDBServiceWorker* aTarget,
                   nsIPrincipal* aPrincipal, const nsACString& aTables,
                   nsIUrlClassifierCallback* aCB)
        : mozilla::Runnable(
              "UrlClassifierDBServiceWorkerProxy::LookupRunnable"),
          mTarget(aTarget),
          mPrincipal(aPrincipal),
          mLookupTables(aTables),
          mCB(aCB) {}

    NS_DECL_NSIRUNNABLE

   private:
    const RefPtr<nsUrlClassifierDBServiceWorker> mTarget;
    const nsCOMPtr<nsIPrincipal> mPrincipal;
    const nsCString mLookupTables;
    const nsCOMPtr<nsIUrlClassifierCallback> mCB;
  };

  class GetTablesRunnable : public mozilla::Runnable {
   public:
    GetTablesRunnable(nsUrlClassifierDBServiceWorker* aTarget,
                      nsIUrlClassifierCallback* aCB)
        : mozilla::Runnable(
              "UrlClassifierDBServiceWorkerProxy::GetTablesRunnable"),
          mTarget(aTarget),
          mCB(aCB) {}

    NS_DECL_NSIRUNNABLE

   private:
    const RefPtr<nsUrlClassifierDBServiceWorker> mTarget;
    const nsCOMPtr<nsIUrlClassifierCallback> mCB;
  };

  class BeginUpdateRunnable : public mozilla::Runnable {
   public:
    BeginUpdateRunnable(nsUrlClassifierDBServiceWorker* aTarget,
                        nsIUrlClassifierUpdateObserver* aUpdater,
                        const nsACString& aTables)
        : mozilla::Runnable(
              "UrlClassifierDBServiceWorkerProxy::BeginUpdateRunnable"),
          mTarget(aTarget),
          mUpdater(aUpdater),
          mTables(aTables) {}

    NS_DECL_NSIRUNNABLE

   private:
    const RefPtr<nsUrlClassifierDBServiceWorker> mTarget;
    const nsCOMPtr<nsIUrlClassifierUpdateObserver> mUpdater;
    const nsCString mTables;
  };

  class BeginStreamRunnable : public mozilla::Runnable {
   public:
    BeginStreamRunnable(nsUrlClassifierDBServiceWorker* aTarget,
                        const nsACString& aTable)
        : mozilla::Runnable(
              "UrlClassifierDBServiceWorkerProxy::BeginStreamRunnable"),
          mTarget(aTarget),
          mTable(aTable) {}

    NS_DECL_NSIRUNNABLE

   private:
    const RefPtr<nsUrlClassifierDBServiceWorker> mTarget;
    const nsCString mTable;
  };

  class UpdateStreamRunnable : public mozilla::Runnable {
   public:
    UpdateStreamRunnable(nsUrlClassifierDBServiceWorker* aTarget,
                         const nsACString& aUpdateChunk)
        : mozilla::Runnable(
              "UrlClassifierDBServiceWorkerProxy::UpdateStreamRunnable"),
          mTarget(aTarget),
          mUpdateChunk(aUpdateChunk) {}

    NS_DECL_NSIRUNNABLE

   private:
    const RefPtr<nsUrlClassifierDBServiceWorker> mTarget;
    const nsCString mUpdateChunk;
  };

  class CacheCompletionsRunnable : public mozilla::Runnable {
   public:
    CacheCompletionsRunnable(
        nsUrlClassifierDBServiceWorker* aTarget,
        const mozilla::safebrowsing::ConstCacheResultArray& aEntries)
        : mozilla::Runnable(
              "UrlClassifierDBServiceWorkerProxy::CacheCompletionsRunnable"),
          mTarget(aTarget),
          mEntries(aEntries.Clone()) {}

    NS_DECL_NSIRUNNABLE

   private:
    const RefPtr<nsUrlClassifierDBServiceWorker> mTarget;
    const mozilla::safebrowsing::ConstCacheResultArray mEntries;
  };

  class ClearLastResultsRunnable : public mozilla::Runnable {
   public:
    explicit ClearLastResultsRunnable(nsUrlClassifierDBServiceWorker* aTarget)
        : mozilla::Runnable(
              "UrlClassifierDBServiceWorkerProxy::ClearLastResultsRunnable"),
          mTarget(aTarget) {}

    NS_DECL_NSIRUNNABLE
   private:
    const RefPtr<nsUrlClassifierDBServiceWorker> mTarget;
  };

  class GetCacheInfoRunnable : public mozilla::Runnable {
   public:
    explicit GetCacheInfoRunnable(nsUrlClassifierDBServiceWorker* aTarget,
                                  const nsACString& aTable,
                                  nsIUrlClassifierGetCacheCallback* aCallback)
        : mozilla::Runnable(
              "UrlClassifierDBServiceWorkerProxy::GetCacheInfoRunnable"),
          mTarget(aTarget),
          mTable(aTable),
          mCache(nullptr),
          mCallback(new nsMainThreadPtrHolder<nsIUrlClassifierGetCacheCallback>(
              "nsIUrlClassifierGetCacheCallback", aCallback)) {}

    NS_DECL_NSIRUNNABLE
   private:
    const RefPtr<nsUrlClassifierDBServiceWorker> mTarget;
    const nsCString mTable;
    nsCOMPtr<nsIUrlClassifierCacheInfo> mCache;
    const nsMainThreadPtrHandle<nsIUrlClassifierGetCacheCallback> mCallback;
  };

  class GetCacheInfoCallbackRunnable : public mozilla::Runnable {
   public:
    explicit GetCacheInfoCallbackRunnable(
        nsIUrlClassifierCacheInfo* aCache,
        const nsMainThreadPtrHandle<nsIUrlClassifierGetCacheCallback>&
            aCallback)
        : mozilla::Runnable(
              "UrlClassifierDBServiceWorkerProxy::"
              "GetCacheInfoCallbackRunnable"),
          mCache(aCache),
          mCallback(aCallback) {}

    NS_DECL_NSIRUNNABLE
   private:
    nsCOMPtr<nsIUrlClassifierCacheInfo> mCache;
    const nsMainThreadPtrHandle<nsIUrlClassifierGetCacheCallback> mCallback;
  };

 public:
  nsresult OpenDb() const;
  nsresult CloseDb() const;
  nsresult PreShutdown() const;

  nsresult CacheCompletions(
      const mozilla::safebrowsing::ConstCacheResultArray& aEntries) const;

  nsresult GetCacheInfo(const nsACString& aTable,
                        nsIUrlClassifierGetCacheCallback* aCallback) const;

 private:
  ~UrlClassifierDBServiceWorkerProxy() = default;

  const RefPtr<nsUrlClassifierDBServiceWorker> mTarget;
};

// The remaining classes here are all proxies to the main thread

class UrlClassifierLookupCallbackProxy final
    : public nsIUrlClassifierLookupCallback {
 public:
  explicit UrlClassifierLookupCallbackProxy(
      nsIUrlClassifierLookupCallback* aTarget)
      : mTarget(new nsMainThreadPtrHolder<nsIUrlClassifierLookupCallback>(
            "UrlClassifierLookupCallbackProxy::mTarget", aTarget)) {}

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIURLCLASSIFIERLOOKUPCALLBACK

  class LookupCompleteRunnable : public mozilla::Runnable {
   public:
    LookupCompleteRunnable(
        const nsMainThreadPtrHandle<nsIUrlClassifierLookupCallback>& aTarget,
        mozilla::UniquePtr<mozilla::safebrowsing::LookupResultArray> aResults)
        : mozilla::Runnable(
              "UrlClassifierLookupCallbackProxy::LookupCompleteRunnable"),
          mTarget(aTarget),
          mResults(std::move(aResults)) {}

    NS_DECL_NSIRUNNABLE

   private:
    const nsMainThreadPtrHandle<nsIUrlClassifierLookupCallback> mTarget;
    mozilla::UniquePtr<mozilla::safebrowsing::LookupResultArray> mResults;
  };

 private:
  ~UrlClassifierLookupCallbackProxy() = default;

  const nsMainThreadPtrHandle<nsIUrlClassifierLookupCallback> mTarget;
};

class UrlClassifierCallbackProxy final : public nsIUrlClassifierCallback {
 public:
  explicit UrlClassifierCallbackProxy(nsIUrlClassifierCallback* aTarget)
      : mTarget(new nsMainThreadPtrHolder<nsIUrlClassifierCallback>(
            "UrlClassifierCallbackProxy::mTarget", aTarget)) {}

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIURLCLASSIFIERCALLBACK

  class HandleEventRunnable : public mozilla::Runnable {
   public:
    HandleEventRunnable(
        const nsMainThreadPtrHandle<nsIUrlClassifierCallback>& aTarget,
        const nsACString& aValue)
        : mozilla::Runnable("UrlClassifierCallbackProxy::HandleEventRunnable"),
          mTarget(aTarget),
          mValue(aValue) {}

    NS_DECL_NSIRUNNABLE

   private:
    const nsMainThreadPtrHandle<nsIUrlClassifierCallback> mTarget;
    const nsCString mValue;
  };

 private:
  ~UrlClassifierCallbackProxy() = default;

  const nsMainThreadPtrHandle<nsIUrlClassifierCallback> mTarget;
};

class UrlClassifierUpdateObserverProxy final
    : public nsIUrlClassifierUpdateObserver {
 public:
  explicit UrlClassifierUpdateObserverProxy(
      nsIUrlClassifierUpdateObserver* aTarget)
      : mTarget(new nsMainThreadPtrHolder<nsIUrlClassifierUpdateObserver>(
            "UrlClassifierUpdateObserverProxy::mTarget", aTarget)) {}

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIURLCLASSIFIERUPDATEOBSERVER

  class UpdateUrlRequestedRunnable : public mozilla::Runnable {
   public:
    UpdateUrlRequestedRunnable(
        const nsMainThreadPtrHandle<nsIUrlClassifierUpdateObserver>& aTarget,
        const nsACString& aURL, const nsACString& aTable)
        : mozilla::Runnable(
              "UrlClassifierUpdateObserverProxy::UpdateUrlRequestedRunnable"),
          mTarget(aTarget),
          mURL(aURL),
          mTable(aTable) {}

    NS_DECL_NSIRUNNABLE

   private:
    const nsMainThreadPtrHandle<nsIUrlClassifierUpdateObserver> mTarget;
    const nsCString mURL;
    const nsCString mTable;
  };

  class StreamFinishedRunnable : public mozilla::Runnable {
   public:
    StreamFinishedRunnable(
        const nsMainThreadPtrHandle<nsIUrlClassifierUpdateObserver>& aTarget,
        nsresult aStatus, uint32_t aDelay)
        : mozilla::Runnable(
              "UrlClassifierUpdateObserverProxy::StreamFinishedRunnable"),
          mTarget(aTarget),
          mStatus(aStatus),
          mDelay(aDelay) {}

    NS_DECL_NSIRUNNABLE

   private:
    const nsMainThreadPtrHandle<nsIUrlClassifierUpdateObserver> mTarget;
    const nsresult mStatus;
    const uint32_t mDelay;
  };

  class UpdateErrorRunnable : public mozilla::Runnable {
   public:
    UpdateErrorRunnable(
        const nsMainThreadPtrHandle<nsIUrlClassifierUpdateObserver>& aTarget,
        nsresult aError)
        : mozilla::Runnable(
              "UrlClassifierUpdateObserverProxy::UpdateErrorRunnable"),
          mTarget(aTarget),
          mError(aError) {}

    NS_DECL_NSIRUNNABLE

   private:
    const nsMainThreadPtrHandle<nsIUrlClassifierUpdateObserver> mTarget;
    const nsresult mError;
  };

  class UpdateSuccessRunnable : public mozilla::Runnable {
   public:
    UpdateSuccessRunnable(
        const nsMainThreadPtrHandle<nsIUrlClassifierUpdateObserver>& aTarget,
        uint32_t aRequestedTimeout)
        : mozilla::Runnable(
              "UrlClassifierUpdateObserverProxy::UpdateSuccessRunnable"),
          mTarget(aTarget),
          mRequestedTimeout(aRequestedTimeout) {}

    NS_DECL_NSIRUNNABLE

   private:
    const nsMainThreadPtrHandle<nsIUrlClassifierUpdateObserver> mTarget;
    const uint32_t mRequestedTimeout;
  };

 private:
  ~UrlClassifierUpdateObserverProxy() = default;

  const nsMainThreadPtrHandle<nsIUrlClassifierUpdateObserver> mTarget;
};

#endif  // nsUrlClassifierProxies_h
