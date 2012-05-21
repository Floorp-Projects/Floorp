/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
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
#include "mozilla/Services.h"

using namespace mozilla;

#if defined(PR_LOGGING)
PRLogModuleInfo *gWifiMonitorLog;
#endif

NS_IMPL_THREADSAFE_ISUPPORTS3(nsWifiMonitor,
                              nsIRunnable,
                              nsIObserver,
                              nsIWifiMonitor)

nsWifiMonitor::nsWifiMonitor()
: mKeepGoing(true)
, mReentrantMonitor("nsWifiMonitor.mReentrantMonitor")
{
#if defined(PR_LOGGING)
  gWifiMonitorLog = PR_NewLogModule("WifiMonitor");
#endif

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
                     const PRUnichar *data)
{
  if (!strcmp(topic, "xpcom-shutdown")) {
    LOG(("Shutting down\n"));
    mKeepGoing = false;

    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    mon.Notify();
  }
  return NS_OK;
}


NS_IMETHODIMP nsWifiMonitor::StartWatching(nsIWifiListener *aListener)
{
  if (!aListener)
    return NS_ERROR_NULL_POINTER;

  nsresult rv = NS_OK;
  if (!mThread) {
    rv = NS_NewThread(getter_AddRefs(mThread), this);
    if (NS_FAILED(rv))
      return rv;
  }

  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  mKeepGoing = true;

  mListeners.AppendElement(nsWifiListener(aListener));

  // tell ourselves that we have a new watcher.
  mon.Notify();
  return NS_OK;
}

NS_IMETHODIMP nsWifiMonitor::StopWatching(nsIWifiListener *aListener)
{
  if (!aListener)
    return NS_ERROR_NULL_POINTER;

  LOG(("removing listener\n"));

  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  for (PRUint32 i = 0; i < mListeners.Length(); i++) {

    if (mListeners[i].mListener == aListener) {
      mListeners.RemoveElementAt(i);
      break;
    }
  }

  if (mListeners.Length() == 0) {
    mKeepGoing = false;
    mon.Notify();
    mThread = nsnull;
  }

  return NS_OK;
}

class nsPassErrorToWifiListeners : public nsIRunnable
{
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  nsPassErrorToWifiListeners(nsAutoPtr<nsCOMArray<nsIWifiListener> > aListeners,
                             nsresult aResult)
  : mListeners(aListeners),
    mResult(aResult)
  {}

 private:
  nsAutoPtr<nsCOMArray<nsIWifiListener> > mListeners;
  nsresult mResult;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(nsPassErrorToWifiListeners,
                              nsIRunnable)

NS_IMETHODIMP nsPassErrorToWifiListeners::Run()
{
  LOG(("About to send error to the wifi listeners\n"));
  for (PRInt32 i = 0; i < mListeners->Count(); i++) {
    (*mListeners)[i]->OnError(mResult);
  }
  return NS_OK;
}

NS_IMETHODIMP nsWifiMonitor::Run()
{
  LOG(("@@@@@ wifi monitor run called\n"));

  nsresult rv = DoScan();

  if (mKeepGoing && NS_FAILED(rv)) {
    nsAutoPtr<nsCOMArray<nsIWifiListener> > currentListeners(
                           new nsCOMArray<nsIWifiListener>(mListeners.Length()));
    if (!currentListeners)
      return NS_ERROR_OUT_OF_MEMORY;

    for (PRUint32 i = 0; i < mListeners.Length(); i++)
      currentListeners->AppendObject(mListeners[i].mListener);

    nsCOMPtr<nsIThread> thread = do_GetMainThread();
    if (!thread)
      return NS_ERROR_UNEXPECTED;

    nsCOMPtr<nsIRunnable> runnable(new nsPassErrorToWifiListeners(currentListeners, rv));
    if (!runnable)
      return NS_ERROR_OUT_OF_MEMORY;

    thread->Dispatch(runnable, NS_DISPATCH_SYNC);
  }

  return NS_OK;
}

class nsCallWifiListeners : public nsIRunnable
{
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  nsCallWifiListeners(nsAutoPtr<nsCOMArray<nsIWifiListener> > aListeners,
                      nsAutoPtr<nsTArray<nsIWifiAccessPoint*> > aAccessPoints)
  : mListeners(aListeners),
    mAccessPoints(aAccessPoints)
  {}

 private:
  nsAutoPtr<nsCOMArray<nsIWifiListener> > mListeners;
  nsAutoPtr<nsTArray<nsIWifiAccessPoint*> > mAccessPoints;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(nsCallWifiListeners,
                              nsIRunnable)

NS_IMETHODIMP nsCallWifiListeners::Run()
{
  LOG(("About to send data to the wifi listeners\n"));
  for (PRInt32 i = 0; i < mListeners->Count(); i++) {
    (*mListeners)[i]->OnChange(mAccessPoints->Elements(), mAccessPoints->Length());
  }
  return NS_OK;
}

nsresult
nsWifiMonitor::CallWifiListeners(const nsCOMArray<nsWifiAccessPoint> &aAccessPoints,
                                 bool aAccessPointsChanged)
{
    nsAutoPtr<nsCOMArray<nsIWifiListener> > currentListeners(
                           new nsCOMArray<nsIWifiListener>(mListeners.Length()));
    if (!currentListeners)
      return NS_ERROR_OUT_OF_MEMORY;

    {
      ReentrantMonitorAutoEnter mon(mReentrantMonitor);

      for (PRUint32 i = 0; i < mListeners.Length(); i++) {
        if (!mListeners[i].mHasSentData || aAccessPointsChanged) {
          mListeners[i].mHasSentData = true;
          currentListeners->AppendObject(mListeners[i].mListener);
        }
      }
    }

    if (currentListeners->Count() > 0)
    {
      PRUint32 resultCount = aAccessPoints.Count();
      nsAutoPtr<nsTArray<nsIWifiAccessPoint*> > accessPoints(
                               new nsTArray<nsIWifiAccessPoint *>(resultCount));
      if (!accessPoints)
        return NS_ERROR_OUT_OF_MEMORY;

      for (PRUint32 i = 0; i < resultCount; i++)
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
