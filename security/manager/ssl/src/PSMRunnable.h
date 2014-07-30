/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PSMRunnable_h
#define PSMRunnable_h

#include "mozilla/Monitor.h"
#include "nsThreadUtils.h"
#include "nsIObserver.h"
#include "nsProxyRelease.h"

namespace mozilla { namespace psm {

// Wait for the event to run on the target thread without spinning the event
// loop on the calling thread. (Dispatching events to a thread using
// NS_DISPATCH_SYNC would cause the event loop on the calling thread to spin.)
class SyncRunnableBase : public nsRunnable
{
public:
  NS_DECL_NSIRUNNABLE
  nsresult DispatchToMainThreadAndWait();
protected:
  SyncRunnableBase();
  virtual void RunOnTargetThread() = 0;
private:
  mozilla::Monitor monitor;
};

class NotifyObserverRunnable : public nsRunnable
{
public:
  NotifyObserverRunnable(nsIObserver * observer,
                         const char * topicStringLiteral)
    : mObserver(new nsMainThreadPtrHolder<nsIObserver>(observer)),
      mTopic(topicStringLiteral) {
  }
  NS_DECL_NSIRUNNABLE
private:
  nsMainThreadPtrHandle<nsIObserver> mObserver;
  const char * const mTopic;
};

} } // namespace mozilla::psm

#endif
