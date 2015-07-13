/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CacheIOThread.h"
#include "CacheFileIOManager.h"

#include "nsIRunnable.h"
#include "nsISupportsImpl.h"
#include "nsPrintfCString.h"
#include "nsThreadUtils.h"
#include "mozilla/IOInterposer.h"

namespace mozilla {
namespace net {

CacheIOThread* CacheIOThread::sSelf = nullptr;

NS_IMPL_ISUPPORTS(CacheIOThread, nsIThreadObserver)

CacheIOThread::CacheIOThread()
: mMonitor("CacheIOThread")
, mThread(nullptr)
, mLowestLevelWaiting(LAST_LEVEL)
, mCurrentlyExecutingLevel(0)
, mHasXPCOMEvents(false)
, mRerunCurrentEvent(false)
, mShutdown(false)
, mInsideLoop(true)
{
  sSelf = this;
}

CacheIOThread::~CacheIOThread()
{
  sSelf = nullptr;
#ifdef DEBUG
  for (uint32_t level = 0; level < LAST_LEVEL; ++level) {
    MOZ_ASSERT(!mEventQueue[level].Length());
  }
#endif
}

nsresult CacheIOThread::Init()
{
  mThread = PR_CreateThread(PR_USER_THREAD, ThreadFunc, this,
                            PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD,
                            PR_JOINABLE_THREAD, 128 * 1024);
  if (!mThread)
    return NS_ERROR_FAILURE;

  return NS_OK;
}

nsresult CacheIOThread::Dispatch(nsIRunnable* aRunnable, uint32_t aLevel)
{
  NS_ENSURE_ARG(aLevel < LAST_LEVEL);

  // Runnable is always expected to be non-null, hard null-check bellow.
  MOZ_ASSERT(aRunnable);

  MonitorAutoLock lock(mMonitor);

  if (mShutdown && (PR_GetCurrentThread() != mThread))
    return NS_ERROR_UNEXPECTED;

  return DispatchInternal(aRunnable, aLevel);
}

nsresult CacheIOThread::DispatchAfterPendingOpens(nsIRunnable* aRunnable)
{
  // Runnable is always expected to be non-null, hard null-check bellow.
  MOZ_ASSERT(aRunnable);

  MonitorAutoLock lock(mMonitor);

  if (mShutdown && (PR_GetCurrentThread() != mThread))
    return NS_ERROR_UNEXPECTED;

  // Move everything from later executed OPEN level to the OPEN_PRIORITY level
  // where we post the (eviction) runnable.
  mEventQueue[OPEN_PRIORITY].AppendElements(mEventQueue[OPEN]);
  mEventQueue[OPEN].Clear();

  return DispatchInternal(aRunnable, OPEN_PRIORITY);
}

nsresult CacheIOThread::DispatchInternal(nsIRunnable* aRunnable, uint32_t aLevel)
{
  if (NS_WARN_IF(!aRunnable))
    return NS_ERROR_NULL_POINTER;

  mMonitor.AssertCurrentThreadOwns();

  mEventQueue[aLevel].AppendElement(aRunnable);
  if (mLowestLevelWaiting > aLevel)
    mLowestLevelWaiting = aLevel;

  mMonitor.NotifyAll();

  return NS_OK;
}

bool CacheIOThread::IsCurrentThread()
{
  return mThread == PR_GetCurrentThread();
}

bool CacheIOThread::YieldInternal()
{
  if (!IsCurrentThread()) {
    NS_WARNING("Trying to yield to priority events on non-cache2 I/O thread? "
               "You probably do something wrong.");
    return false;
  }

  if (mCurrentlyExecutingLevel == XPCOM_LEVEL) {
    // Doesn't make any sense, since this handler is the one
    // that would be executed as the next one.
    return false;
  }

  if (!EventsPending(mCurrentlyExecutingLevel))
    return false;

  mRerunCurrentEvent = true;
  return true;
}

nsresult CacheIOThread::Shutdown()
{
  {
    MonitorAutoLock lock(mMonitor);
    mShutdown = true;
    mMonitor.NotifyAll();
  }

  PR_JoinThread(mThread);
  mThread = nullptr;

  return NS_OK;
}

already_AddRefed<nsIEventTarget> CacheIOThread::Target()
{
  nsCOMPtr<nsIEventTarget> target;

  target = mXPCOMThread;
  if (!target && mThread)
  {
    MonitorAutoLock lock(mMonitor);
    if (!mXPCOMThread)
      lock.Wait();

    target = mXPCOMThread;
  }

  return target.forget();
}

// static
void CacheIOThread::ThreadFunc(void* aClosure)
{
  PR_SetCurrentThreadName("Cache2 I/O");
  mozilla::IOInterposer::RegisterCurrentThread();
  CacheIOThread* thread = static_cast<CacheIOThread*>(aClosure);
  thread->ThreadFunc();
  mozilla::IOInterposer::UnregisterCurrentThread();
}

void CacheIOThread::ThreadFunc()
{
  nsCOMPtr<nsIThreadInternal> threadInternal;

  {
    MonitorAutoLock lock(mMonitor);

    // This creates nsThread for this PRThread
    nsCOMPtr<nsIThread> xpcomThread = NS_GetCurrentThread();

    threadInternal = do_QueryInterface(xpcomThread);
    if (threadInternal)
      threadInternal->SetObserver(this);

    mXPCOMThread.swap(xpcomThread);

    lock.NotifyAll();

    do {
loopStart:
      // Reset the lowest level now, so that we can detect a new event on
      // a lower level (i.e. higher priority) has been scheduled while
      // executing any previously scheduled event.
      mLowestLevelWaiting = LAST_LEVEL;

      // Process xpcom events first
      while (mHasXPCOMEvents) {
        mHasXPCOMEvents = false;
        mCurrentlyExecutingLevel = XPCOM_LEVEL;

        MonitorAutoUnlock unlock(mMonitor);

        bool processedEvent;
        nsresult rv;
        do {
          rv = mXPCOMThread->ProcessNextEvent(false, &processedEvent);
        } while (NS_SUCCEEDED(rv) && processedEvent);
      }

      uint32_t level;
      for (level = 0; level < LAST_LEVEL; ++level) {
        if (!mEventQueue[level].Length()) {
          // no events on this level, go to the next level
          continue;
        }

        LoopOneLevel(level);

        // Go to the first (lowest) level again
        goto loopStart;
      }

      if (EventsPending())
        continue;

      if (mShutdown)
        break;

      lock.Wait(PR_INTERVAL_NO_TIMEOUT);

      if (EventsPending())
        continue;

    } while (true);

    MOZ_ASSERT(!EventsPending());

    // This is for correct assertion on XPCOM events dispatch.
    mInsideLoop = false;
  } // lock

  if (threadInternal)
    threadInternal->SetObserver(nullptr);
}

static const char* const sLevelTraceName[] = {
  "net::cache::io::level(0)",
  "net::cache::io::level(1)",
  "net::cache::io::level(2)",
  "net::cache::io::level(3)",
  "net::cache::io::level(4)",
  "net::cache::io::level(5)",
  "net::cache::io::level(6)",
  "net::cache::io::level(7)",
  "net::cache::io::level(8)",
  "net::cache::io::level(9)",
  "net::cache::io::level(10)",
  "net::cache::io::level(11)",
  "net::cache::io::level(12)"
};

void CacheIOThread::LoopOneLevel(uint32_t aLevel)
{
  nsTArray<nsCOMPtr<nsIRunnable> > events;
  events.SwapElements(mEventQueue[aLevel]);
  uint32_t length = events.Length();

  mCurrentlyExecutingLevel = aLevel;

  bool returnEvents = false;
  uint32_t index;
  {
    MonitorAutoUnlock unlock(mMonitor);

    for (index = 0; index < length; ++index) {
      if (EventsPending(aLevel)) {
        // Somebody scheduled a new event on a lower level, break and harry
        // to execute it!  Don't forget to return what we haven't exec.
        returnEvents = true;
        break;
      }

      // Drop any previous flagging, only an event on the current level may set
      // this flag.
      mRerunCurrentEvent = false;

      events[index]->Run();

      if (mRerunCurrentEvent) {
        // The event handler yields to higher priority events and wants to rerun.
        returnEvents = true;
        break;
      }

      // Release outside the lock.
      events[index] = nullptr;
    }
  }

  if (returnEvents)
    mEventQueue[aLevel].InsertElementsAt(0, events.Elements() + index, length - index);
}

bool CacheIOThread::EventsPending(uint32_t aLastLevel)
{
  return mLowestLevelWaiting < aLastLevel || mHasXPCOMEvents;
}

NS_IMETHODIMP CacheIOThread::OnDispatchedEvent(nsIThreadInternal *thread)
{
  MonitorAutoLock lock(mMonitor);
  mHasXPCOMEvents = true;
  MOZ_ASSERT(mInsideLoop);
  lock.Notify();
  return NS_OK;
}

NS_IMETHODIMP CacheIOThread::OnProcessNextEvent(nsIThreadInternal *thread, bool mayWait, uint32_t recursionDepth)
{
  return NS_OK;
}

NS_IMETHODIMP CacheIOThread::AfterProcessNextEvent(nsIThreadInternal *thread, uint32_t recursionDepth,
                                                   bool eventWasProcessed)
{
  return NS_OK;
}

// Memory reporting

size_t CacheIOThread::SizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const
{
  MonitorAutoLock lock(const_cast<CacheIOThread*>(this)->mMonitor);

  size_t n = 0;
  n += mallocSizeOf(mThread);
  for (uint32_t level = 0; level < LAST_LEVEL; ++level) {
    n += mEventQueue[level].SizeOfExcludingThis(mallocSizeOf);
    // Events referenced by the queues are arbitrary objects we cannot be sure
    // are reported elsewhere as well as probably not implementing nsISizeOf
    // interface.  Deliberatly omitting them from reporting here.
  }

  return n;
}

size_t CacheIOThread::SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const
{
  return mallocSizeOf(this) + SizeOfExcludingThis(mallocSizeOf);
}

} // namespace net
} // namespace mozilla
