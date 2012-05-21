/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsUrlClassifierProxies.h"
#include "nsUrlClassifierDBService.h"

static nsresult
DispatchToWorkerThread(nsIRunnable* r)
{
  nsIThread* t = nsUrlClassifierDBService::BackgroundThread();
  if (!t)
    return NS_ERROR_FAILURE;

  return t->Dispatch(r, NS_DISPATCH_NORMAL);
}

NS_IMPL_THREADSAFE_ISUPPORTS1(UrlClassifierDBServiceWorkerProxy,
                              nsIUrlClassifierDBServiceWorker)

NS_IMETHODIMP
UrlClassifierDBServiceWorkerProxy::Lookup(const nsACString& aSpec,
                                          nsIUrlClassifierCallback* aCB)
{
  nsCOMPtr<nsIRunnable> r = new LookupRunnable(mTarget, aSpec, aCB);
  return DispatchToWorkerThread(r);
}

NS_IMETHODIMP
UrlClassifierDBServiceWorkerProxy::LookupRunnable::Run()
{
  mTarget->Lookup(mSpec, mCB);
  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierDBServiceWorkerProxy::GetTables(nsIUrlClassifierCallback* aCB)
{
  nsCOMPtr<nsIRunnable> r = new GetTablesRunnable(mTarget, aCB);
  return DispatchToWorkerThread(r);
}

NS_IMETHODIMP
UrlClassifierDBServiceWorkerProxy::GetTablesRunnable::Run()
{
  mTarget->GetTables(mCB);
  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierDBServiceWorkerProxy::SetHashCompleter
  (const nsACString&, nsIUrlClassifierHashCompleter*)
{
  NS_NOTREACHED("This method should not be called!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
UrlClassifierDBServiceWorkerProxy::BeginUpdate
  (nsIUrlClassifierUpdateObserver* aUpdater,
   const nsACString& aTables,
   const nsACString& aClientKey)
{
  nsCOMPtr<nsIRunnable> r = new BeginUpdateRunnable(mTarget, aUpdater,
                                                    aTables, aClientKey);
  return DispatchToWorkerThread(r);
}

NS_IMETHODIMP
UrlClassifierDBServiceWorkerProxy::BeginUpdateRunnable::Run()
{
  mTarget->BeginUpdate(mUpdater, mTables, mClientKey);
  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierDBServiceWorkerProxy::BeginStream(const nsACString& aTable,
                                               const nsACString& aServerMAC)
{
  nsCOMPtr<nsIRunnable> r =
    new BeginStreamRunnable(mTarget, aTable, aServerMAC);
  return DispatchToWorkerThread(r);
}

NS_IMETHODIMP
UrlClassifierDBServiceWorkerProxy::BeginStreamRunnable::Run()
{
  mTarget->BeginStream(mTable, mServerMAC);
  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierDBServiceWorkerProxy::UpdateStream(const nsACString& aUpdateChunk)
{
  nsCOMPtr<nsIRunnable> r =
    new UpdateStreamRunnable(mTarget, aUpdateChunk);
  return DispatchToWorkerThread(r);
}

NS_IMETHODIMP
UrlClassifierDBServiceWorkerProxy::UpdateStreamRunnable::Run()
{
  mTarget->UpdateStream(mUpdateChunk);
  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierDBServiceWorkerProxy::FinishStream()
{
  nsCOMPtr<nsIRunnable> r =
    NS_NewRunnableMethod(mTarget,
                         &nsIUrlClassifierDBServiceWorker::FinishStream);
  return DispatchToWorkerThread(r);
}

NS_IMETHODIMP
UrlClassifierDBServiceWorkerProxy::FinishUpdate()
{
  nsCOMPtr<nsIRunnable> r =
    NS_NewRunnableMethod(mTarget,
                         &nsIUrlClassifierDBServiceWorker::FinishUpdate);
  return DispatchToWorkerThread(r);
}

NS_IMETHODIMP
UrlClassifierDBServiceWorkerProxy::CancelUpdate()
{
  nsCOMPtr<nsIRunnable> r =
    NS_NewRunnableMethod(mTarget,
                         &nsIUrlClassifierDBServiceWorker::CancelUpdate);
  return DispatchToWorkerThread(r);
}

NS_IMETHODIMP
UrlClassifierDBServiceWorkerProxy::ResetDatabase()
{
  nsCOMPtr<nsIRunnable> r =
    NS_NewRunnableMethod(mTarget,
                         &nsIUrlClassifierDBServiceWorker::ResetDatabase);
  return DispatchToWorkerThread(r);
}

NS_IMETHODIMP
UrlClassifierDBServiceWorkerProxy::CloseDb()
{
  nsCOMPtr<nsIRunnable> r =
    NS_NewRunnableMethod(mTarget,
                         &nsIUrlClassifierDBServiceWorker::CloseDb);
  return DispatchToWorkerThread(r);
}

NS_IMETHODIMP
UrlClassifierDBServiceWorkerProxy::CacheCompletions(nsTArray<nsUrlClassifierLookupResult>* aEntries)
{
  nsCOMPtr<nsIRunnable> r = new CacheCompletionsRunnable(mTarget, aEntries);
  return DispatchToWorkerThread(r);
}

NS_IMETHODIMP
UrlClassifierDBServiceWorkerProxy::CacheCompletionsRunnable::Run()
{
  mTarget->CacheCompletions(mEntries);
  return NS_OK;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(UrlClassifierLookupCallbackProxy,
                              nsIUrlClassifierLookupCallback)

NS_IMETHODIMP
UrlClassifierLookupCallbackProxy::LookupComplete
  (nsTArray<nsUrlClassifierLookupResult>* aResults)
{
  nsCOMPtr<nsIRunnable> r = new LookupCompleteRunnable(mTarget, aResults);
  return NS_DispatchToMainThread(r);
}

NS_IMETHODIMP
UrlClassifierLookupCallbackProxy::LookupCompleteRunnable::Run()
{
  mTarget->LookupComplete(mResults);
  return NS_OK;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(UrlClassifierCallbackProxy,
                              nsIUrlClassifierCallback)

NS_IMETHODIMP
UrlClassifierCallbackProxy::HandleEvent(const nsACString& aValue)
{
  nsCOMPtr<nsIRunnable> r = new HandleEventRunnable(mTarget, aValue);
  return NS_DispatchToMainThread(r);
}

NS_IMETHODIMP
UrlClassifierCallbackProxy::HandleEventRunnable::Run()
{
  mTarget->HandleEvent(mValue);
  return NS_OK;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(UrlClassifierUpdateObserverProxy,
                              nsIUrlClassifierUpdateObserver)

NS_IMETHODIMP
UrlClassifierUpdateObserverProxy::UpdateUrlRequested
  (const nsACString& aURL,
   const nsACString& aTable,
   const nsACString& aServerMAC)
{
  nsCOMPtr<nsIRunnable> r =
    new UpdateUrlRequestedRunnable(mTarget, aURL, aTable, aServerMAC);
  return NS_DispatchToMainThread(r);
}

NS_IMETHODIMP
UrlClassifierUpdateObserverProxy::UpdateUrlRequestedRunnable::Run()
{
  mTarget->UpdateUrlRequested(mURL, mTable, mServerMAC);
  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierUpdateObserverProxy::RekeyRequested()
{
  nsCOMPtr<nsIRunnable> r =
    NS_NewRunnableMethod(mTarget, &nsIUrlClassifierUpdateObserver::RekeyRequested);
  return NS_DispatchToMainThread(r);
}

NS_IMETHODIMP
UrlClassifierUpdateObserverProxy::StreamFinished(nsresult aStatus,
                                                 PRUint32 aDelay)
{
  nsCOMPtr<nsIRunnable> r =
    new StreamFinishedRunnable(mTarget, aStatus, aDelay);
  return NS_DispatchToMainThread(r);
}

NS_IMETHODIMP
UrlClassifierUpdateObserverProxy::StreamFinishedRunnable::Run()
{
  mTarget->StreamFinished(mStatus, mDelay);
  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierUpdateObserverProxy::UpdateError(nsresult aError)
{
  nsCOMPtr<nsIRunnable> r =
    new UpdateErrorRunnable(mTarget, aError);
  return NS_DispatchToMainThread(r);
}

NS_IMETHODIMP
UrlClassifierUpdateObserverProxy::UpdateErrorRunnable::Run()
{
  mTarget->UpdateError(mError);
  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierUpdateObserverProxy::UpdateSuccess(PRUint32 aRequestedTimeout)
{
  nsCOMPtr<nsIRunnable> r =
    new UpdateSuccessRunnable(mTarget, aRequestedTimeout);
  return NS_DispatchToMainThread(r);
}

NS_IMETHODIMP
UrlClassifierUpdateObserverProxy::UpdateSuccessRunnable::Run()
{
  mTarget->UpdateSuccess(mRequestedTimeout);
  return NS_OK;
}
