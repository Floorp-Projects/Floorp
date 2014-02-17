/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GonkMemoryPressureMonitoring.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsMemoryPressure.h"
#include "nsXULAppAPI.h"
#include "base/message_loop.h"
#include <errno.h>
#include <fcntl.h>
#include <android/log.h>

#define LOG(args...)  \
  __android_log_print(ANDROID_LOG_INFO, "GonkMemoryPressure" , ## args)

using namespace mozilla;

namespace {

//
// MemoryPressureWatcher watches on the I/O thread for changes to the
// lowmemkiller's sysfs interface. If the system runs low on memory,
// MemoryPressureWatcher sends a MemoryPressureEvent, removes itself
// from the I/O loop, and schedules a PollTask to re-start polling
// after a timeout has been reached.
//
// The PollTask is allocated by MemoryPressureWatcher and handed over
// to the I/O loop, which then owns the object and deletes it after it
// ran. We cannot allocate the object dynamically, because the system
// is already low on memory. Instead we overload the new and delete
// operators for PollTask to hand-out statically allocated memory.
// There can only be at most one instance of PollTask at a time, so
// we can re-use the same memory on each allocation.
//
// There is a separate observer for shutdown events. When the system
// shuts down, it sends a task to the I/O thread for removing the
// watcher. If a PollTask is pending, it gets canceled. We cannot
// delete it at this point, because it's owned by the I/O loop.
//
class MemoryPressureWatcher : public MessageLoopForIO::Watcher
{
public:
  class PollTask : public CancelableTask
  {
  public:
    PollTask(MemoryPressureWatcher* aWatcher)
    : mWatcher(aWatcher)
    {
      MOZ_ASSERT(mWatcher);
    }

    static void* operator new(size_t aSize);
    static void operator delete(void* aMem, size_t aSize);

    void Run() MOZ_OVERRIDE
    {
      MOZ_ASSERT(MessageLoopForIO::current());

      if (mWatcher) {
        MOZ_ASSERT(MessageLoopForIO::current() == mWatcher->GetIOLoop());
        mWatcher->StartWatching();
      }
    }

    void Cancel() MOZ_OVERRIDE
    {
      mWatcher = nullptr;
    }

  private:
    MemoryPressureWatcher* mWatcher;
  };

  template <size_t Size> class PollTaskAllocator
  {
  public:
    void* Alloc()
    {
      MOZ_ASSERT(!sAllocated);
      sAllocated = true;
      return mMem;
    }
    void Release(void* aMem)
    {
      MOZ_ASSERT(mMem == aMem);
      MOZ_ASSERT(sAllocated);
      sAllocated = false;
    }

  private:
    static bool sAllocated;
    unsigned char mMem[Size];
  };

  MemoryPressureWatcher(MessageLoop* aIOLoop, uint32_t aPollMS)
  : mFd(-1)
  , mIOLoop(aIOLoop)
  , mPollTask(nullptr)
  , mPollMS(aPollMS)
  , mMemoryPressure(false)
  {
    MOZ_ASSERT(mIOLoop);
  }

  virtual ~MemoryPressureWatcher()
  {
    MOZ_ASSERT(MessageLoopForIO::current() == mIOLoop);
    MOZ_ASSERT(mFd == -1);
  }

  MessageLoop* GetIOLoop () const
  {
    return mIOLoop;
  }

  nsresult Open()
  {
    MOZ_ASSERT(MessageLoopForIO::current() == mIOLoop);

    int fd;

    do {
      fd = open("/sys/kernel/mm/lowmemkiller/notify_trigger_active",
                O_RDONLY | O_CLOEXEC);
    } while (fd == -1 && errno == EINTR);

    if (NS_WARN_IF(fd == -1)) {
      return NS_ERROR_NOT_AVAILABLE;
    }

    mFd = fd;

    return NS_OK;
  }

  void Close()
  {
    MOZ_ASSERT(MessageLoopForIO::current() == mIOLoop);

    if (NS_WARN_IF(mFd == -1)) {
      return;
    }

    int res;

    do {
      res = close(mFd);
    } while (res == -1 && errno == EINTR);

    NS_WARN_IF(res == -1);
    mFd = -1;
  }

  void StartWatching()
  {
    MessageLoopForIO* ioLoop = MessageLoopForIO::current();
    MOZ_ASSERT(ioLoop == mIOLoop);
    ioLoop->WatchFileDescriptor(mFd, true, MessageLoopForIO::WATCH_READ,
                                &mReadWatcher, this);
    mPollTask = nullptr;
  }

  void StopWatching()
  {
    if (mPollTask) {
      mPollTask->Cancel();
    }
    mReadWatcher.StopWatchingFileDescriptor();
  }

  virtual void OnFileCanWriteWithoutBlocking(int aFd) MOZ_OVERRIDE
  {
    MOZ_ASSERT(MessageLoopForIO::current() == mIOLoop);
    NS_WARNING("Must not write to memory monitor");
  }

  virtual void OnFileCanReadWithoutBlocking(int aFd) MOZ_OVERRIDE
  {
    MOZ_ASSERT(MessageLoopForIO::current() == mIOLoop);

    bool memoryPressure;
    nsresult rv = CheckForMemoryPressure(memoryPressure);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    if (memoryPressure) {
      LOG("Memory pressure detected.");
      StopWatching();
      if (mMemoryPressure) {
        rv = NS_DispatchMemoryPressure(MemPressure_Ongoing);
      } else {
        rv = NS_DispatchMemoryPressure(MemPressure_New);
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return;
      }

      // While we're under memory pressure, we periodically read()
      // notify_trigger_active to try and see when we're no longer under
      // memory pressure.  mPollMS indicates how many milliseconds we wait
      // between those read()s.
      mPollTask = new PollTask(this);
      mIOLoop->PostDelayedTask(FROM_HERE, mPollTask, mPollMS);
    } else if (mMemoryPressure) {
      LOG("Memory pressure is over.");
    }
    mMemoryPressure = memoryPressure;
  }

private:
  /**
   * Read from aLowMemFd, which we assume corresponds to the
   * notify_trigger_active sysfs node, and determine whether we're currently
   * under memory pressure.
   *
   * We don't expect this method to block.
   */
  nsresult CheckForMemoryPressure(bool& aOut)
  {
    aOut = false;

    off_t off = lseek(mFd, 0, SEEK_SET);
    if (NS_WARN_IF(off)) {
      return NS_ERROR_UNEXPECTED;
    }

    char buf[2];
    int nread;
    do {
      nread = read(mFd, buf, sizeof(buf));
    } while(nread == -1 && errno == EINTR);
    if (NS_WARN_IF(nread != 2)) {
      return NS_ERROR_UNEXPECTED;
    }

    // The notify_trigger_active sysfs node should contain either "0\n" or
    // "1\n".  The latter indicates memory pressure.
    aOut = buf[0] == '1' && buf[1] == '\n';

    return NS_OK;
  }

  int mFd;
  MessageLoop* mIOLoop;
  MessageLoopForIO::FileDescriptorWatcher mReadWatcher;
  PollTask* mPollTask;
  uint32_t mPollMS;
  bool mMemoryPressure;
};

static
  MemoryPressureWatcher::PollTaskAllocator<sizeof(MemoryPressureWatcher::PollTask)>
    sPollTaskAllocator;

template<>
bool
  MemoryPressureWatcher::PollTaskAllocator<sizeof(MemoryPressureWatcher::PollTask)>::sAllocated(false);

void*
MemoryPressureWatcher::PollTask::operator new(size_t aSize)
{
  return sPollTaskAllocator.Alloc();
}

void
MemoryPressureWatcher::PollTask::operator delete(void* aMem, size_t aSize)
{
  sPollTaskAllocator.Release(aMem);
}

// Initializes MemoryPressureWatcher on I/O thread
//
class InitMemoryPressureWatcherTask : public Task
{
public:
  InitMemoryPressureWatcherTask(MemoryPressureWatcher* aWatcher)
  : mWatcher(aWatcher)
  {
    MOZ_ASSERT(mWatcher);
  }

  void Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(MessageLoopForIO::current() == mWatcher->GetIOLoop());

    nsresult rv = mWatcher->Open();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }
    mWatcher->StartWatching();
  }

private:
  MemoryPressureWatcher* mWatcher;
};

// Releases MemoryPressureWatcher on I/O thread
//
class ShutdownMemoryPressureWatcherTask : public Task
{
public:
  ShutdownMemoryPressureWatcherTask(MemoryPressureWatcher* aWatcher)
  : mWatcher(aWatcher)
  {
    MOZ_ASSERT(mWatcher);
  }

  void Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(MessageLoopForIO::current() == mWatcher->GetIOLoop());

    mWatcher->StopWatching();
    mWatcher->Close();
  }

private:
  nsAutoPtr<MemoryPressureWatcher> mWatcher;
};

// Closes MemoryPressureWatcher on shutdown
//
class ShutdownObserver : public nsIObserver
{
public:
  ShutdownObserver(MemoryPressureWatcher* aWatcher)
  : mWatcher(aWatcher)
  {
    MOZ_ASSERT(mWatcher);
  }

  NS_DECL_THREADSAFE_ISUPPORTS

  NS_IMETHOD Observe(nsISupports* aSubject, const char* aTopic,
                     const char16_t* aData)
  {
    MOZ_ASSERT(!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID));
    LOG("Observed XPCOM shutdown.");

    Task* task = new ShutdownMemoryPressureWatcherTask(mWatcher);
    mWatcher->GetIOLoop()->PostTask(FROM_HERE, task);

    return NS_OK;
  }

private:
  MemoryPressureWatcher* mWatcher;
};

NS_IMPL_ISUPPORTS1(ShutdownObserver, nsIObserver);

} // anonymous namespace

namespace mozilla {

void
InitGonkMemoryPressureMonitoring()
{
  MessageLoop* ioLoop = XRE_GetIOMessageLoop();
  uint32_t pollMS =
    Preferences::GetUint("gonk.systemMemoryPressureRecoveryPollMS", 5000);
  MemoryPressureWatcher* watcher = new MemoryPressureWatcher(ioLoop, pollMS);

  // Start watcher on I/O thread
  Task* task = new InitMemoryPressureWatcherTask(watcher);
  watcher->GetIOLoop()->PostTask(FROM_HERE, task);

  // Install shutdown observer
  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  if (NS_WARN_IF(!os)) {
    return;
  }
  nsRefPtr<ShutdownObserver> observer = new ShutdownObserver(watcher);
  os->AddObserver(observer, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
}

} // namespace mozilla
