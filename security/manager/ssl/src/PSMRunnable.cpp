/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 
#include "PSMRunnable.h"
 
namespace mozilla { namespace psm {

SyncRunnableBase::SyncRunnableBase()
  : monitor("SyncRunnableBase::monitor")
{
}

nsresult
SyncRunnableBase::DispatchToMainThreadAndWait()
{
  nsresult rv;
  if (NS_IsMainThread()) {
    RunOnTargetThread();
    rv = NS_OK;
  } else {
    mozilla::MonitorAutoLock lock(monitor);
    rv = NS_DispatchToMainThread(this);
    if (NS_SUCCEEDED(rv)) {
      lock.Wait();
    }
  }

  return rv;
}

NS_IMETHODIMP
SyncRunnableBase::Run()
{
  RunOnTargetThread();
  mozilla::MonitorAutoLock(monitor).Notify();
  return NS_OK;
}

nsresult
NotifyObserverRunnable::Run()
{
  mObserver->Observe(nsnull, mTopic, nsnull);
  return NS_OK;
}

} } // namespace mozilla::psm
