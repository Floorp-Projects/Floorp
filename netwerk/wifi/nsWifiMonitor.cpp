/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsProxyRelease.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "nsXPCOM.h"
#include "nsXPCOMCID.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsWifiMonitor.h"
#include "nsWifiAccessPoint.h"

#include "nsServiceManagerUtils.h"
#include "nsComponentManagerUtils.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Services.h"

using namespace mozilla;

LazyLogModule gWifiMonitorLog("WifiMonitor");

NS_IMPL_ISUPPORTS(nsWifiMonitor,
                  nsIRunnable,
                  nsIObserver,
                  nsIWifiMonitor)

nsWifiMonitor::nsWifiMonitor()
: mKeepGoing(true)
, mThreadComplete(false)
, mReentrantMonitor("nsWifiMonitor.mReentrantMonitor")
{
  nsCOMPtr<nsIObserverService> obsSvc = mozilla::services::GetObserverService();
  if (obsSvc)
    obsSvc->AddObserver(this, "xpcom-shutdown", false);

  LOG(("@@@@@ wifimonitor created\n"));
}

nsWifiMonitor::~nsWifiMonitor()
{
}

NS_IMETHODIMP
nsWifiMonitor::Observe(nsISupports *subject, const char *topic,
                     const char16_t *data)
{
  if (!strcmp(topic, "xpcom-shutdown")) {
    LOG(("Shutting down\n"));

    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    mKeepGoing = false;
    mon.Notify();
    mThread = nullptr;
  }
  return NS_OK;
}


NS_IMETHODIMP nsWifiMonitor::StartWatching(nsIWifiListener *aListener)
{
  LOG(("nsWifiMonitor::StartWatching %p thread %p listener %p\n",
       this, mThread.get(), aListener));
  MOZ_ASSERT(NS_IsMainThread());

  if (!aListener)
    return NS_ERROR_NULL_POINTER;
  if (!mKeepGoing) {
      return NS_ERROR_NOT_AVAILABLE;
  }

  nsresult rv = NS_OK;

  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  if (mThreadComplete) {
      // generally there is just one thread for the lifetime of the service,
      // but if DoScan returns with an error before shutdown (i.e. !mKeepGoing)
      // then we will respawn the thread.
      LOG(("nsWifiMonitor::StartWatching %p restarting thread\n", this));
      mThreadComplete = false;
      mThread = nullptr;
  }

  if (!mThread) {
    rv = NS_NewNamedThread("Wifi Monitor", getter_AddRefs(mThread), this);
    if (NS_FAILED(rv))
      return rv;
  }


  mListeners.AppendElement(nsWifiListener(new nsMainThreadPtrHolder<nsIWifiListener>(aListener)));

  // tell ourselves that we have a new watcher.
  mon.Notify();
  return NS_OK;
}

NS_IMETHODIMP nsWifiMonitor::StopWatching(nsIWifiListener *aListener)
{
  LOG(("nsWifiMonitor::StopWatching %p thread %p listener %p\n",
       this, mThread.get(), aListener));
  MOZ_ASSERT(NS_IsMainThread());

  if (!aListener)
    return NS_ERROR_NULL_POINTER;

  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  for (uint32_t i = 0; i < mListeners.Length(); i++) {

    if (mListeners[i].mListener == aListener) {
      mListeners.RemoveElementAt(i);
      break;
    }
  }

  return NS_OK;
}

typedef nsTArray<nsMainThreadPtrHandle<nsIWifiListener> > WifiListenerArray;

class nsPassErrorToWifiListeners final : public nsIRunnable
{
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  nsPassErrorToWifiListeners(nsAutoPtr<WifiListenerArray> aListeners,
                             nsresult aResult)
  : mListeners(aListeners),
    mResult(aResult)
  {}

 private:
  ~nsPassErrorToWifiListeners() {}
  nsAutoPtr<WifiListenerArray> mListeners;
  nsresult mResult;
};

NS_IMPL_ISUPPORTS(nsPassErrorToWifiListeners,
                  nsIRunnable)

NS_IMETHODIMP nsPassErrorToWifiListeners::Run()
{
  LOG(("About to send error to the wifi listeners\n"));
  for (size_t i = 0; i < mListeners->Length(); i++) {
    (*mListeners)[i]->OnError(mResult);
  }
  return NS_OK;
}

NS_IMETHODIMP nsWifiMonitor::Run()
{
  LOG(("@@@@@ wifi monitor run called\n"));

  nsresult rv = DoScan();
  LOG(("@@@@@ wifi monitor run::doscan complete %" PRIx32 "\n", static_cast<uint32_t>(rv)));

  nsAutoPtr<WifiListenerArray> currentListeners;
  bool doError = false;

  {
      ReentrantMonitorAutoEnter mon(mReentrantMonitor);
      if (mKeepGoing && NS_FAILED(rv)) {
          doError = true;
          currentListeners = new WifiListenerArray(mListeners.Length());
          for (uint32_t i = 0; i < mListeners.Length(); i++)
              currentListeners->AppendElement(mListeners[i].mListener);
      }
      mThreadComplete = true;
  }

  if (doError) {
    nsCOMPtr<nsIEventTarget> target = GetMainThreadEventTarget();
    if (!target)
      return NS_ERROR_UNEXPECTED;

    nsCOMPtr<nsIRunnable> runnable(new nsPassErrorToWifiListeners(currentListeners, rv));
    if (!runnable)
      return NS_ERROR_OUT_OF_MEMORY;

    target->Dispatch(runnable, NS_DISPATCH_SYNC);
  }

  LOG(("@@@@@ wifi monitor run complete\n"));
  return NS_OK;
}

class nsCallWifiListeners final : public nsIRunnable
{
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  nsCallWifiListeners(nsAutoPtr<WifiListenerArray> aListeners,
                      nsAutoPtr<nsTArray<nsIWifiAccessPoint*> > aAccessPoints)
  : mListeners(aListeners),
    mAccessPoints(aAccessPoints)
  {}

 private:
  ~nsCallWifiListeners() {}
  nsAutoPtr<WifiListenerArray> mListeners;
  nsAutoPtr<nsTArray<nsIWifiAccessPoint*> > mAccessPoints;
};

NS_IMPL_ISUPPORTS(nsCallWifiListeners,
                  nsIRunnable)

NS_IMETHODIMP nsCallWifiListeners::Run()
{
  LOG(("About to send data to the wifi listeners\n"));
  for (size_t i = 0; i < mListeners->Length(); i++) {
    (*mListeners)[i]->OnChange(mAccessPoints->Elements(), mAccessPoints->Length());
  }
  return NS_OK;
}

nsresult
nsWifiMonitor::CallWifiListeners(const nsCOMArray<nsWifiAccessPoint> &aAccessPoints,
                                 bool aAccessPointsChanged)
{
    nsAutoPtr<WifiListenerArray> currentListeners;
    {
      ReentrantMonitorAutoEnter mon(mReentrantMonitor);

      currentListeners = new WifiListenerArray(mListeners.Length());

      for (uint32_t i = 0; i < mListeners.Length(); i++) {
        if (!mListeners[i].mHasSentData || aAccessPointsChanged) {
          mListeners[i].mHasSentData = true;
          currentListeners->AppendElement(mListeners[i].mListener);
        }
      }
    }

    if (currentListeners->Length() > 0)
    {
      uint32_t resultCount = aAccessPoints.Count();
      nsAutoPtr<nsTArray<nsIWifiAccessPoint*> > accessPoints(
                               new nsTArray<nsIWifiAccessPoint *>(resultCount));
      if (!accessPoints)
        return NS_ERROR_OUT_OF_MEMORY;

      for (uint32_t i = 0; i < resultCount; i++)
        accessPoints->AppendElement(aAccessPoints[i]);

      nsCOMPtr<nsIThread> thread = do_GetMainThread();
      if (!thread)
        return NS_ERROR_UNEXPECTED;

      nsCOMPtr<nsIRunnable> runnable(
                      new nsCallWifiListeners(currentListeners, accessPoints));
      if (!runnable)
        return NS_ERROR_OUT_OF_MEMORY;

      thread->Dispatch(runnable, NS_DISPATCH_SYNC);
    }

    return NS_OK;
}
