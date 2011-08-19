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
