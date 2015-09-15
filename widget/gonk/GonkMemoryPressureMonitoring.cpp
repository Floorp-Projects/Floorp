/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GonkMemoryPressureMonitoring.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/FileUtils.h"
#include "mozilla/Monitor.h"
#include "mozilla/Preferences.h"
#include "mozilla/ProcessPriorityManager.h"
#include "mozilla/Services.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsMemoryPressure.h"
#include "nsThreadUtils.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <android/log.h>

#define LOG(args...)  \
  __android_log_print(ANDROID_LOG_INFO, "GonkMemoryPressure" , ## args)

#ifdef MOZ_NUWA_PROCESS
#include "ipc/Nuwa.h"
#endif

using namespace mozilla;

namespace {

/**
 * MemoryPressureWatcher watches sysfs from its own thread to notice when the
 * system is under memory pressure.  When we observe memory pressure, we use
 * MemoryPressureRunnable to notify observers that they should release memory.
 *
 * When the system is under memory pressure, we don't want to constantly fire
 * memory-pressure events.  So instead, we try to detect when sysfs indicates
 * that we're no longer under memory pressure, and only then start firing events
 * again.
 *
 * (This is a bit problematic because we can't poll() to detect when we're no
 * longer under memory pressure; instead we have to periodically read the sysfs
 * node.  If we remain under memory pressure for a long time, this means we'll
 * continue waking up to read from the node for a long time, potentially wasting
 * battery life.  Hopefully we don't hit this case in practice!  We write to
 * logcat each time we go around this loop so it's at least noticable.)
 *
 * Shutting down safely is a bit of a chore.  XPCOM won't shut down until all
 * threads exit, so we need to exit the Run() method below on shutdown.  But our
 * thread might be blocked in one of two situations: We might be poll()'ing the
 * sysfs node waiting for memory pressure to occur, or we might be asleep
 * waiting to read() the sysfs node to see if we're no longer under memory
 * pressure.
 *
 * To let us wake up from the poll(), we poll() not just the sysfs node but also
 * a pipe, which we write to on shutdown.  To let us wake up from sleeping
 * between read()s, we sleep by Wait()'ing on a monitor, which we notify on
 * shutdown.
 */
class MemoryPressureWatcher final
  : public nsIRunnable
  , public nsIObserver
{
public:
  MemoryPressureWatcher()
    : mMonitor("MemoryPressureWatcher")
    , mShuttingDown(false)
  {
  }

  NS_DECL_THREADSAFE_ISUPPORTS

  nsresult Init()
  {
    nsCOMPtr<nsIObserverService> os = services::GetObserverService();
    NS_ENSURE_STATE(os);

    // The observer service holds us alive.
    os->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, /* holdsWeak */ false);

    // While we're under memory pressure, we periodically read()
    // notify_trigger_active to try and see when we're no longer under memory
    // pressure.  mPollMS indicates how many milliseconds we wait between those
    // read()s.
    mPollMS = Preferences::GetUint("gonk.systemMemoryPressureRecoveryPollMS",
                                   /* default */ 5000);

    int pipes[2];
    NS_ENSURE_STATE(!pipe(pipes));
    mShutdownPipeRead = pipes[0];
    mShutdownPipeWrite = pipes[1];
    return NS_OK;
  }

  NS_IMETHOD Observe(nsISupports* aSubject, const char* aTopic,
                     const char16_t* aData)
  {
    MOZ_ASSERT(strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0);
    LOG("Observed XPCOM shutdown.");

    MonitorAutoLock lock(mMonitor);
    mShuttingDown = true;
    mMonitor.Notify();

    int rv;
    do {
      // Write something to the pipe; doesn't matter what.
      uint32_t dummy = 0;
      rv = write(mShutdownPipeWrite, &dummy, sizeof(dummy));
    } while(rv == -1 && errno == EINTR);

    return NS_OK;
  }

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(!NS_IsMainThread());

#ifdef MOZ_NUWA_PROCESS
    if (IsNuwaProcess()) {
      NuwaMarkCurrentThread(nullptr, nullptr);
    }
#endif

    int lowMemFd = open("/sys/kernel/mm/lowmemkiller/notify_trigger_active",
                        O_RDONLY | O_CLOEXEC);
    NS_ENSURE_STATE(lowMemFd != -1);
    ScopedClose autoClose(lowMemFd);

    nsresult rv = CheckForMemoryPressure(lowMemFd, nullptr);
    NS_ENSURE_SUCCESS(rv, rv);

    while (true) {
      // Wait for a notification on lowMemFd or for data to be written to
      // mShutdownPipeWrite.  (poll(lowMemFd, POLLPRI) blocks until we're under
      // memory pressure.)
      struct pollfd pollfds[2];
      pollfds[0].fd = lowMemFd;
      pollfds[0].events = POLLPRI;
      pollfds[1].fd = mShutdownPipeRead;
      pollfds[1].events = POLLIN;

      int pollRv;
      do {
        pollRv = poll(pollfds, ArrayLength(pollfds), /* timeout */ -1);
      } while (pollRv == -1 && errno == EINTR);

      if (pollfds[1].revents) {
        // Something was written to our shutdown pipe; we're outta here.
        LOG("shutting down (1)");
        return NS_OK;
      }

      // If pollfds[1] isn't happening, pollfds[0] ought to be!
      if (!(pollfds[0].revents & POLLPRI)) {
        LOG("Unexpected revents value after poll(): %d. "
            "Shutting down GonkMemoryPressureMonitoring.", pollfds[0].revents);
        return NS_ERROR_FAILURE;
      }

      // POLLPRI on lowMemFd indicates that we're in a low-memory situation.  We
      // could read lowMemFd to double-check, but we've observed that the read
      // sometimes completes after the memory-pressure event is over, so let's
      // just believe the result of poll().

      // We use low-memory-no-forward because each process has its own watcher
      // and thus there is no need for the main process to forward this event.
      rv = DispatchMemoryPressure(MemPressure_New);
      NS_ENSURE_SUCCESS(rv, rv);

      // Manually check lowMemFd until we observe that memory pressure is over.
      // We won't fire any more low-memory events until we observe that
      // we're no longer under pressure. Instead, we fire low-memory-ongoing
      // events, which cause processes to keep flushing caches but will not
      // trigger expensive GCs and other attempts to save memory that are
      // likely futile at this point.
      bool memoryPressure;
      do {
        {
          MonitorAutoLock lock(mMonitor);

          // We need to check mShuttingDown before we wait here, in order to
          // catch a shutdown signal sent after we poll()'ed mShutdownPipeRead
          // above but before we started waiting on the monitor.  But we don't
          // need to check after we wait, because we'll either do another
          // iteration of this inner loop, in which case we'll check
          // mShuttingDown, or we'll exit this loop and do another iteration
          // of the outer loop, in which case we'll check the shutdown pipe.
          if (mShuttingDown) {
            LOG("shutting down (2)");
            return NS_OK;
          }
          mMonitor.Wait(PR_MillisecondsToInterval(mPollMS));
        }

        LOG("Checking to see if memory pressure is over.");
        rv = CheckForMemoryPressure(lowMemFd, &memoryPressure);
        NS_ENSURE_SUCCESS(rv, rv);

        if (memoryPressure) {
          rv = DispatchMemoryPressure(MemPressure_Ongoing);
          NS_ENSURE_SUCCESS(rv, rv);
          continue;
        }
      } while (false);

      LOG("Memory pressure is over.");
    }

    return NS_OK;
  }

protected:
  ~MemoryPressureWatcher() {}

private:
  /**
   * Read from aLowMemFd, which we assume corresponds to the
   * notify_trigger_active sysfs node, and determine whether we're currently
   * under memory pressure.
   *
   * We don't expect this method to block.
   */
  nsresult CheckForMemoryPressure(int aLowMemFd, bool* aOut)
  {
    if (aOut) {
      *aOut = false;
    }

    lseek(aLowMemFd, 0, SEEK_SET);

    char buf[2];
    int nread;
    do {
      nread = read(aLowMemFd, buf, sizeof(buf));
    } while(nread == -1 && errno == EINTR);
    NS_ENSURE_STATE(nread == 2);

    // The notify_trigger_active sysfs node should contain either "0\n" or
    // "1\n".  The latter indicates memory pressure.
    if (aOut) {
      *aOut = buf[0] == '1' && buf[1] == '\n';
    }
    return NS_OK;
  }

  /**
   * Dispatch the specified memory pressure event unless a high-priority
   * process is present. If a high-priority process is present then it's likely
   * responding to an urgent event (an incoming call or message for example) so
   * avoid wasting CPU time responding to low-memory events.
   */
  nsresult DispatchMemoryPressure(MemoryPressureState state)
  {
    if (ProcessPriorityManager::AnyProcessHasHighPriority()) {
      return NS_OK;
    }

    return NS_DispatchMemoryPressure(state);
  }

  Monitor mMonitor;
  uint32_t mPollMS;
  bool mShuttingDown;

  ScopedClose mShutdownPipeRead;
  ScopedClose mShutdownPipeWrite;
};

NS_IMPL_ISUPPORTS(MemoryPressureWatcher, nsIRunnable, nsIObserver);

} // namespace

namespace mozilla {

void
InitGonkMemoryPressureMonitoring()
{
  // memoryPressureWatcher is held alive by the observer service.
  nsRefPtr<MemoryPressureWatcher> memoryPressureWatcher =
    new MemoryPressureWatcher();
  NS_ENSURE_SUCCESS_VOID(memoryPressureWatcher->Init());

  nsCOMPtr<nsIThread> thread;
  NS_NewNamedThread("MemoryPressure", getter_AddRefs(thread),
                    memoryPressureWatcher);
}

} // namespace mozilla
