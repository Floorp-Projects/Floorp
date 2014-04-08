/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CacheIOThread__h__
#define CacheIOThread__h__

#include "nsIThreadInternal.h"
#include "nsISupportsImpl.h"
#include "prthread.h"
#include "nsTArray.h"
#include "nsAutoPtr.h"
#include "mozilla/Monitor.h"

class nsIRunnable;

namespace mozilla {
namespace net {

class CacheIOThread : public nsIThreadObserver
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSITHREADOBSERVER

  CacheIOThread();
  virtual ~CacheIOThread();

  enum ELevel {
    OPEN_PRIORITY,
    READ_PRIORITY,
    OPEN,
    READ,
    MANAGEMENT,
    WRITE,
    CLOSE,
    INDEX,
    EVICT,
    LAST_LEVEL,

    // This is actually executed as the first level, but we want this enum
    // value merely as an indicator while other values are used as indexes
    // to the queue array.  Hence put at end and not as the first.
    XPCOM_LEVEL
  };

  nsresult Init();
  nsresult Dispatch(nsIRunnable* aRunnable, uint32_t aLevel);
  // Makes sure that any previously posted event to OPEN or OPEN_PRIORITY
  // levels (such as file opennings and dooms) are executed before aRunnable
  // that is intended to evict stuff from the cache.
  nsresult DispatchAfterPendingOpens(nsIRunnable* aRunnable);
  bool IsCurrentThread();

  /**
   * Callable only on this thread, checks if there is an event waiting in
   * the event queue with a higher execution priority.  If so, the result
   * is true and the current event handler should break it's work and return
   * from Run() method immediately.  The event handler will be rerun again
   * when all more priority events are processed.  Events pending after this
   * handler (i.e. the one that called YieldAndRerun()) will not execute sooner
   * then this handler is executed w/o a call to YieldAndRerun().
   */
  static bool YieldAndRerun()
  {
    return sSelf ? sSelf->YieldInternal() : false;
  }

  nsresult Shutdown();
  already_AddRefed<nsIEventTarget> Target();

  // Memory reporting
  size_t SizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const;
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

private:
  static void ThreadFunc(void* aClosure);
  void ThreadFunc();
  void LoopOneLevel(uint32_t aLevel);
  bool EventsPending(uint32_t aLastLevel = LAST_LEVEL);
  nsresult DispatchInternal(nsIRunnable* aRunnable, uint32_t aLevel);
  bool YieldInternal();

  static CacheIOThread* sSelf;

  mozilla::Monitor mMonitor;
  PRThread* mThread;
  nsCOMPtr<nsIThread> mXPCOMThread;
  uint32_t mLowestLevelWaiting;
  uint32_t mCurrentlyExecutingLevel;
  nsTArray<nsRefPtr<nsIRunnable> > mEventQueue[LAST_LEVEL];

  bool mHasXPCOMEvents;
  bool mRerunCurrentEvent;
  bool mShutdown;
};

} // net
} // mozilla

#endif
