/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CacheIOThread.h"
#include "CacheFileIOManager.h"
#include "CacheLog.h"
#include "CacheObserver.h"

#include "nsIRunnable.h"
#include "nsISupportsImpl.h"
#include "nsPrintfCString.h"
#include "nsThread.h"
#include "nsThreadManager.h"
#include "nsThreadUtils.h"
#include "mozilla/EventQueue.h"
#include "mozilla/IOInterposer.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/ThreadEventQueue.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TelemetryHistogramEnums.h"

#ifdef XP_WIN
#  include <windows.h>
#endif

namespace mozilla::net {

namespace {  // anon

class CacheIOTelemetry {
 public:
  using size_type = CacheIOThread::EventQueue::size_type;
  static size_type mMinLengthToReport[CacheIOThread::LAST_LEVEL];
  static void Report(uint32_t aLevel, size_type aLength);
};

static CacheIOTelemetry::size_type const kGranularity = 30;

CacheIOTelemetry::size_type
    CacheIOTelemetry::mMinLengthToReport[CacheIOThread::LAST_LEVEL] = {
        kGranularity, kGranularity, kGranularity, kGranularity,
        kGranularity, kGranularity, kGranularity, kGranularity};

// static
void CacheIOTelemetry::Report(uint32_t aLevel,
                              CacheIOTelemetry::size_type aLength) {
  if (mMinLengthToReport[aLevel] > aLength) {
    return;
  }

  static Telemetry::HistogramID telemetryID[] = {
      Telemetry::HTTP_CACHE_IO_QUEUE_2_OPEN_PRIORITY,
      Telemetry::HTTP_CACHE_IO_QUEUE_2_READ_PRIORITY,
      Telemetry::HTTP_CACHE_IO_QUEUE_2_MANAGEMENT,
      Telemetry::HTTP_CACHE_IO_QUEUE_2_OPEN,
      Telemetry::HTTP_CACHE_IO_QUEUE_2_READ,
      Telemetry::HTTP_CACHE_IO_QUEUE_2_WRITE_PRIORITY,
      Telemetry::HTTP_CACHE_IO_QUEUE_2_WRITE,
      Telemetry::HTTP_CACHE_IO_QUEUE_2_INDEX,
      Telemetry::HTTP_CACHE_IO_QUEUE_2_EVICT};

  // Each bucket is a multiply of kGranularity (30, 60, 90..., 300+)
  aLength = (aLength / kGranularity);
  // Next time report only when over the current length + kGranularity
  mMinLengthToReport[aLevel] = (aLength + 1) * kGranularity;

  // 10 is number of buckets we have in each probe
  aLength = std::min<size_type>(aLength, 10);

  Telemetry::Accumulate(telemetryID[aLevel], aLength - 1);  // counted from 0
}

}  // namespace

namespace detail {

/**
 * Helper class encapsulating platform-specific code to cancel
 * any pending IO operation taking too long.  Solely used during
 * shutdown to prevent any IO shutdown hangs.
 * Mainly designed for using Win32 CancelSynchronousIo function.
 */
class NativeThreadHandle {
#ifdef XP_WIN
  // The native handle to the thread
  HANDLE mThread;
#endif

 public:
  // Created and destroyed on the main thread only
  NativeThreadHandle();
  ~NativeThreadHandle();

  // Called on the IO thread to grab the platform specific
  // reference to it.
  void InitThread();
  // If there is a blocking operation being handled on the IO
  // thread, this is called on the main thread during shutdown.
  void CancelBlockingIO(Monitor& aMonitor);
};

#ifdef XP_WIN

NativeThreadHandle::NativeThreadHandle() : mThread(NULL) {}

NativeThreadHandle::~NativeThreadHandle() {
  if (mThread) {
    CloseHandle(mThread);
  }
}

void NativeThreadHandle::InitThread() {
  // GetCurrentThread() only returns a pseudo handle, hence DuplicateHandle
  ::DuplicateHandle(GetCurrentProcess(), GetCurrentThread(),
                    GetCurrentProcess(), &mThread, 0, FALSE,
                    DUPLICATE_SAME_ACCESS);
}

void NativeThreadHandle::CancelBlockingIO(Monitor& aMonitor) {
  HANDLE thread;
  {
    MonitorAutoLock lock(aMonitor);
    thread = mThread;

    if (!thread) {
      return;
    }
  }

  LOG(("CacheIOThread: Attempting to cancel a long blocking IO operation"));
  BOOL result = ::CancelSynchronousIo(thread);
  if (result) {
    LOG(("  cancelation signal succeeded"));
  } else {
    DWORD error = GetLastError();
    LOG(("  cancelation signal failed with GetLastError=%lu", error));
  }
}

#else  // WIN

// Stub code only (we don't implement IO cancelation for this platform)

NativeThreadHandle::NativeThreadHandle() = default;
NativeThreadHandle::~NativeThreadHandle() = default;
void NativeThreadHandle::InitThread() {}
void NativeThreadHandle::CancelBlockingIO(Monitor&) {}

#endif

}  // namespace detail

CacheIOThread* CacheIOThread::sSelf = nullptr;

NS_IMPL_ISUPPORTS(CacheIOThread, nsIThreadObserver)

CacheIOThread::CacheIOThread() {
  for (auto& item : mQueueLength) {
    item = 0;
  }

  sSelf = this;
}

CacheIOThread::~CacheIOThread() {
  {
    MonitorAutoLock lock(mMonitor);
    MOZ_RELEASE_ASSERT(mShutdown);
  }

  if (mXPCOMThread) {
    nsIThread* thread = mXPCOMThread;
    thread->Release();
  }

  sSelf = nullptr;
#ifdef DEBUG
  for (auto& event : mEventQueue) {
    MOZ_ASSERT(!event.Length());
  }
#endif
}

nsresult CacheIOThread::Init() {
  {
    MonitorAutoLock lock(mMonitor);
    // Yeah, there is not a thread yet, but we want to make sure
    // the sequencing is correct.
    mNativeThreadHandle = MakeUnique<detail::NativeThreadHandle>();
  }

  // Increase the reference count while spawning a new thread.
  // If PR_CreateThread succeeds, we will forget this reference and the thread
  // will be responsible to release it when it completes.
  RefPtr<CacheIOThread> self = this;
  mThread =
      PR_CreateThread(PR_USER_THREAD, ThreadFunc, this, PR_PRIORITY_NORMAL,
                      PR_GLOBAL_THREAD, PR_JOINABLE_THREAD, 128 * 1024);
  if (!mThread) {
    // Treat this thread as already shutdown.
    MonitorAutoLock lock(mMonitor);
    mShutdown = true;
    return NS_ERROR_FAILURE;
  }

  // IMPORTANT: The thread now owns this reference, so it's important that we
  // leak it here, otherwise we'll end up with a bad refcount.
  // See the dont_AddRef in ThreadFunc().
  Unused << self.forget().take();

  return NS_OK;
}

nsresult CacheIOThread::Dispatch(nsIRunnable* aRunnable, uint32_t aLevel) {
  return Dispatch(do_AddRef(aRunnable), aLevel);
}

nsresult CacheIOThread::Dispatch(already_AddRefed<nsIRunnable> aRunnable,
                                 uint32_t aLevel) {
  NS_ENSURE_ARG(aLevel < LAST_LEVEL);

  nsCOMPtr<nsIRunnable> runnable(aRunnable);

  // Runnable is always expected to be non-null, hard null-check bellow.
  MOZ_ASSERT(runnable);

  MonitorAutoLock lock(mMonitor);

  if (mShutdown && (PR_GetCurrentThread() != mThread)) {
    return NS_ERROR_UNEXPECTED;
  }

  return DispatchInternal(runnable.forget(), aLevel);
}

nsresult CacheIOThread::DispatchAfterPendingOpens(nsIRunnable* aRunnable) {
  // Runnable is always expected to be non-null, hard null-check bellow.
  MOZ_ASSERT(aRunnable);

  MonitorAutoLock lock(mMonitor);

  if (mShutdown && (PR_GetCurrentThread() != mThread)) {
    return NS_ERROR_UNEXPECTED;
  }

  // Move everything from later executed OPEN level to the OPEN_PRIORITY level
  // where we post the (eviction) runnable.
  mQueueLength[OPEN_PRIORITY] += mEventQueue[OPEN].Length();
  mQueueLength[OPEN] -= mEventQueue[OPEN].Length();
  mEventQueue[OPEN_PRIORITY].AppendElements(mEventQueue[OPEN]);
  mEventQueue[OPEN].Clear();

  return DispatchInternal(do_AddRef(aRunnable), OPEN_PRIORITY);
}

nsresult CacheIOThread::DispatchInternal(
    already_AddRefed<nsIRunnable> aRunnable, uint32_t aLevel) {
  nsCOMPtr<nsIRunnable> runnable(aRunnable);

  LogRunnable::LogDispatch(runnable.get());

  if (NS_WARN_IF(!runnable)) return NS_ERROR_NULL_POINTER;

  mMonitor.AssertCurrentThreadOwns();

  ++mQueueLength[aLevel];
  mEventQueue[aLevel].AppendElement(runnable.forget());
  if (mLowestLevelWaiting > aLevel) mLowestLevelWaiting = aLevel;

  mMonitor.NotifyAll();

  return NS_OK;
}

bool CacheIOThread::IsCurrentThread() {
  return mThread == PR_GetCurrentThread();
}

uint32_t CacheIOThread::QueueSize(bool highPriority) {
  MonitorAutoLock lock(mMonitor);
  if (highPriority) {
    return mQueueLength[OPEN_PRIORITY] + mQueueLength[READ_PRIORITY];
  }

  return mQueueLength[OPEN_PRIORITY] + mQueueLength[READ_PRIORITY] +
         mQueueLength[MANAGEMENT] + mQueueLength[OPEN] + mQueueLength[READ];
}

bool CacheIOThread::YieldInternal() {
  if (!IsCurrentThread()) {
    NS_WARNING(
        "Trying to yield to priority events on non-cache2 I/O thread? "
        "You probably do something wrong.");
    return false;
  }

  if (mCurrentlyExecutingLevel == XPCOM_LEVEL) {
    // Doesn't make any sense, since this handler is the one
    // that would be executed as the next one.
    return false;
  }

  if (!EventsPending(mCurrentlyExecutingLevel)) return false;

  mRerunCurrentEvent = true;
  return true;
}

void CacheIOThread::Shutdown() {
  if (!mThread) {
    return;
  }

  {
    MonitorAutoLock lock(mMonitor);
    mShutdown = true;
    mMonitor.NotifyAll();
  }

  PR_JoinThread(mThread);
  mThread = nullptr;
}

void CacheIOThread::CancelBlockingIO() {
  // This is an attempt to cancel any blocking I/O operation taking
  // too long time.
  if (!mNativeThreadHandle) {
    return;
  }

  if (!mIOCancelableEvents) {
    LOG(("CacheIOThread::CancelBlockingIO, no blocking operation to cancel"));
    return;
  }

  // OK, when we are here, we are processing an IO on the thread that
  // can be cancelled.
  mNativeThreadHandle->CancelBlockingIO(mMonitor);
}

already_AddRefed<nsIEventTarget> CacheIOThread::Target() {
  nsCOMPtr<nsIEventTarget> target;

  target = mXPCOMThread;
  if (!target && mThread) {
    MonitorAutoLock lock(mMonitor);
    while (!mXPCOMThread) {
      lock.Wait();
    }

    target = mXPCOMThread;
  }

  return target.forget();
}

// static
void CacheIOThread::ThreadFunc(void* aClosure) {
  // XXXmstange We'd like to register this thread with the profiler, but doing
  // so causes leaks, see bug 1323100.
  NS_SetCurrentThreadName("Cache2 I/O");

  mozilla::IOInterposer::RegisterCurrentThread();
  // We hold on to this reference for the duration of the thread.
  RefPtr<CacheIOThread> thread =
      dont_AddRef(static_cast<CacheIOThread*>(aClosure));
  thread->ThreadFunc();
  mozilla::IOInterposer::UnregisterCurrentThread();
}

void CacheIOThread::ThreadFunc() {
  nsCOMPtr<nsIThreadInternal> threadInternal;

  {
    MonitorAutoLock lock(mMonitor);

    MOZ_ASSERT(mNativeThreadHandle);
    mNativeThreadHandle->InitThread();

    auto queue =
        MakeRefPtr<ThreadEventQueue>(MakeUnique<mozilla::EventQueue>());
    nsCOMPtr<nsIThread> xpcomThread =
        nsThreadManager::get().CreateCurrentThread(queue,
                                                   nsThread::NOT_MAIN_THREAD);

    threadInternal = do_QueryInterface(xpcomThread);
    if (threadInternal) threadInternal->SetObserver(this);

    mXPCOMThread = xpcomThread.forget().take();
    nsCOMPtr<nsIThread> thread = NS_GetCurrentThread();

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
          rv = thread->ProcessNextEvent(false, &processedEvent);

          ++mEventCounter;
          MOZ_ASSERT(mNativeThreadHandle);
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

      if (EventsPending()) {
        continue;
      }

      if (mShutdown) {
        break;
      }

      AUTO_PROFILER_LABEL("CacheIOThread::ThreadFunc::Wait", IDLE);
      lock.Wait();

    } while (true);

    MOZ_ASSERT(!EventsPending());

#ifdef DEBUG
    // This is for correct assertion on XPCOM events dispatch.
    mInsideLoop = false;
#endif
  }  // lock

  if (threadInternal) threadInternal->SetObserver(nullptr);
}

void CacheIOThread::LoopOneLevel(uint32_t aLevel) {
  mMonitor.AssertCurrentThreadOwns();
  EventQueue events = std::move(mEventQueue[aLevel]);
  EventQueue::size_type length = events.Length();

  mCurrentlyExecutingLevel = aLevel;

  bool returnEvents = false;
  bool reportTelemetry = true;

  EventQueue::size_type index;
  {
    MonitorAutoUnlock unlock(mMonitor);

    for (index = 0; index < length; ++index) {
      if (EventsPending(aLevel)) {
        // Somebody scheduled a new event on a lower level, break and harry
        // to execute it!  Don't forget to return what we haven't exec.
        returnEvents = true;
        break;
      }

      if (reportTelemetry) {
        reportTelemetry = false;
        CacheIOTelemetry::Report(aLevel, length);
      }

      // Drop any previous flagging, only an event on the current level may set
      // this flag.
      mRerunCurrentEvent = false;

      LogRunnable::Run log(events[index].get());

      events[index]->Run();

      MOZ_ASSERT(mNativeThreadHandle);

      if (mRerunCurrentEvent) {
        // The event handler yields to higher priority events and wants to
        // rerun.
        log.WillRunAgain();
        returnEvents = true;
        break;
      }

      ++mEventCounter;
      --mQueueLength[aLevel];

      // Release outside the lock.
      events[index] = nullptr;
    }
  }

  if (returnEvents) {
    // This code must prevent any AddRef/Release calls on the stored COMPtrs as
    // it might be exhaustive and block the monitor's lock for an excessive
    // amout of time.

    // 'index' points at the event that was interrupted and asked for re-run,
    // all events before have run, been nullified, and can be removed.
    events.RemoveElementsAt(0, index);
    // Move events that might have been scheduled on this queue to the tail to
    // preserve the expected per-queue FIFO order.
    // XXX(Bug 1631371) Check if this should use a fallible operation as it
    // pretended earlier.
    events.AppendElements(std::move(mEventQueue[aLevel]));
    // And finally move everything back to the main queue.
    mEventQueue[aLevel] = std::move(events);
  }
}

bool CacheIOThread::EventsPending(uint32_t aLastLevel) {
  return mLowestLevelWaiting < aLastLevel || mHasXPCOMEvents;
}

NS_IMETHODIMP CacheIOThread::OnDispatchedEvent() {
  MonitorAutoLock lock(mMonitor);
  mHasXPCOMEvents = true;
  MOZ_ASSERT(mInsideLoop);
  lock.Notify();
  return NS_OK;
}

NS_IMETHODIMP CacheIOThread::OnProcessNextEvent(nsIThreadInternal* thread,
                                                bool mayWait) {
  return NS_OK;
}

NS_IMETHODIMP CacheIOThread::AfterProcessNextEvent(nsIThreadInternal* thread,
                                                   bool eventWasProcessed) {
  return NS_OK;
}

// Memory reporting

size_t CacheIOThread::SizeOfExcludingThis(
    mozilla::MallocSizeOf mallocSizeOf) const {
  MonitorAutoLock lock(const_cast<CacheIOThread*>(this)->mMonitor);

  size_t n = 0;
  for (const auto& event : mEventQueue) {
    n += event.ShallowSizeOfExcludingThis(mallocSizeOf);
    // Events referenced by the queues are arbitrary objects we cannot be sure
    // are reported elsewhere as well as probably not implementing nsISizeOf
    // interface.  Deliberatly omitting them from reporting here.
  }

  return n;
}

size_t CacheIOThread::SizeOfIncludingThis(
    mozilla::MallocSizeOf mallocSizeOf) const {
  return mallocSizeOf(this) + SizeOfExcludingThis(mallocSizeOf);
}

CacheIOThread::Cancelable::Cancelable(bool aCancelable)
    : mCancelable(aCancelable) {
  // This will only ever be used on the I/O thread,
  // which is expected to be alive longer than this class.
  MOZ_ASSERT(CacheIOThread::sSelf);
  MOZ_ASSERT(CacheIOThread::sSelf->IsCurrentThread());

  if (mCancelable) {
    ++CacheIOThread::sSelf->mIOCancelableEvents;
  }
}

CacheIOThread::Cancelable::~Cancelable() {
  MOZ_ASSERT(CacheIOThread::sSelf);

  if (mCancelable) {
    --CacheIOThread::sSelf->mIOCancelableEvents;
  }
}

}  // namespace mozilla::net
