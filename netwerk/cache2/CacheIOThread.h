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
#include "mozilla/DebugOnly.h"
#include "mozilla/Atomics.h"
#include "mozilla/UniquePtr.h"

class nsIRunnable;

namespace mozilla {
namespace net {

namespace detail {
// A class keeping platform specific information needed to watch and
// cancel any long blocking synchronous IO.  Must be predeclared here
// since including windows.h breaks stuff with number of macro definition
// conflicts.
class BlockingIOWatcher;
}

class CacheIOThread : public nsIThreadObserver
{
  virtual ~CacheIOThread();

public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSITHREADOBSERVER

  CacheIOThread();

  typedef nsTArray<nsCOMPtr<nsIRunnable>> EventQueue;

  enum ELevel : uint32_t {
    OPEN_PRIORITY,
    READ_PRIORITY,
    OPEN,
    READ,
    MANAGEMENT,
    WRITE,
    CLOSE = WRITE,
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
  nsresult Dispatch(already_AddRefed<nsIRunnable>, uint32_t aLevel);
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

  void Shutdown();
  // This method checks if there is a long blocking IO on the
  // IO thread and tries to cancel it.  It waits maximum of
  // two seconds.
  void CancelBlockingIO();
  already_AddRefed<nsIEventTarget> Target();

  // A stack class used to annotate running interruptable I/O event
  class Cancelable
  {
    bool mCancelable;
  public:
    explicit Cancelable(bool aCancelable);
    ~Cancelable();
  };

  // Memory reporting
  size_t SizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const;
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

private:
  static void ThreadFunc(void* aClosure);
  void ThreadFunc();
  void LoopOneLevel(uint32_t aLevel);
  bool EventsPending(uint32_t aLastLevel = LAST_LEVEL);
  nsresult DispatchInternal(already_AddRefed<nsIRunnable> aRunnable, uint32_t aLevel);
  bool YieldInternal();

  static CacheIOThread* sSelf;

  mozilla::Monitor mMonitor;
  PRThread* mThread;
  UniquePtr<detail::BlockingIOWatcher> mBlockingIOWatcher;
  Atomic<nsIThread *> mXPCOMThread;
  Atomic<uint32_t, Relaxed> mLowestLevelWaiting;
  uint32_t mCurrentlyExecutingLevel;

  EventQueue mEventQueue[LAST_LEVEL];
  // Raised when nsIEventTarget.Dispatch() is called on this thread
  Atomic<bool, Relaxed> mHasXPCOMEvents;
  // See YieldAndRerun() above
  bool mRerunCurrentEvent;
  // Signal to process all pending events and then shutdown
  // Synchronized by mMonitor
  bool mShutdown;
  // If > 0 there is currently an I/O operation on the thread that
  // can be canceled when after shutdown, see the Shutdown() method
  // for usage. Made a counter to allow nesting of the Cancelable class.
  Atomic<uint32_t, Relaxed> mIOCancelableEvents;
#ifdef DEBUG
  bool mInsideLoop;
#endif
};

} // namespace net
} // namespace mozilla

#endif
