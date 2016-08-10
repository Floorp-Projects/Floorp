/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <android/log.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/sysinfo.h>

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
#include "nsPrintfCString.h"
#include "nsThreadUtils.h"

#define LOG(args...)  \
  __android_log_print(ANDROID_LOG_INFO, "GonkMemoryPressure" , ## args)

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
    , mLowMemTriggerKB(0)
    , mPageSize(0)
    , mShuttingDown(false)
    , mTriggerFd(-1)
    , mShutdownPipeRead(-1)
    , mShutdownPipeWrite(-1)
  {
  }

  NS_DECL_THREADSAFE_ISUPPORTS

  nsresult Init()
  {
    nsCOMPtr<nsIObserverService> os = services::GetObserverService();
    NS_ENSURE_STATE(os);

    // The observer service holds us alive.
    os->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, /* holdsWeak */ false);

    // Initialize the internal state
    mPageSize = sysconf(_SC_PAGESIZE);
    ReadPrefs();
    nsresult rv = OpenFiles();
    NS_ENSURE_SUCCESS(rv, rv);
    SetLowMemTrigger(mSoftLowMemTriggerKB);

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

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(!NS_IsMainThread());

    int triggerResetTimeout = -1;
    bool memoryPressure;
    nsresult rv = CheckForMemoryPressure(&memoryPressure);
    NS_ENSURE_SUCCESS(rv, rv);

    while (true) {
      // Wait for a notification on mTriggerFd or for data to be written to
      // mShutdownPipeWrite.  (poll(mTriggerFd, POLLPRI) blocks until we're
      // under memory pressure or until we time out, the time out is used
      // to adjust the trigger level after a memory pressure event.)
      struct pollfd pollfds[2];
      pollfds[0].fd = mTriggerFd;
      pollfds[0].events = POLLPRI;
      pollfds[1].fd = mShutdownPipeRead;
      pollfds[1].events = POLLIN;

      int pollRv = MOZ_TEMP_FAILURE_RETRY(
        poll(pollfds, ArrayLength(pollfds), triggerResetTimeout)
      );

      if (pollRv == 0) {
        // Timed out, adjust the trigger and update the timeout.
        triggerResetTimeout = AdjustTrigger(triggerResetTimeout);
        continue;
      }

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

      // POLLPRI on mTriggerFd indicates that we're in a low-memory situation.
      // We could read lowMemFd to double-check, but we've observed that the
      // read sometimes completes after the memory-pressure event is over, so
      // let's just believe the result of poll().
      rv = DispatchMemoryPressure(MemPressure_New);
      NS_ENSURE_SUCCESS(rv, rv);

      // Move to the hard level if we're on the soft one.
      if (mLowMemTriggerKB > mHardLowMemTriggerKB) {
        SetLowMemTrigger(mHardLowMemTriggerKB);
      }

      // Manually check mTriggerFd until we observe that memory pressure is
      // over.  We won't fire any more low-memory events until we observe that
      // we're no longer under pressure. Instead, we fire low-memory-ongoing
      // events, which cause processes to keep flushing caches but will not
      // trigger expensive GCs and other attempts to save memory that are
      // likely futile at this point.
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
        rv = CheckForMemoryPressure(&memoryPressure);
        NS_ENSURE_SUCCESS(rv, rv);

        if (memoryPressure) {
          rv = DispatchMemoryPressure(MemPressure_Ongoing);
          NS_ENSURE_SUCCESS(rv, rv);
          continue;
        }
      } while (false);

      if (XRE_IsParentProcess()) {
        // The main process will try to adjust the trigger.
        triggerResetTimeout = mPollMS * 2;
      }

      LOG("Memory pressure is over.");
    }

    return NS_OK;
  }

protected:
  ~MemoryPressureWatcher() {}

private:
  void ReadPrefs() {
    // While we're under memory pressure, we periodically read()
    // notify_trigger_active to try and see when we're no longer under memory
    // pressure.  mPollMS indicates how many milliseconds we wait between those
    // read()s.
    Preferences::AddUintVarCache(&mPollMS,
      "gonk.systemMemoryPressureRecoveryPollMS", /* default */ 5000);

    // We have two values for the notify trigger, a soft one which is triggered
    // before we start killing background applications and an hard one which is
    // after we've killed background applications but before we start killing
    // foreground ones.
    Preferences::AddUintVarCache(&mSoftLowMemTriggerKB,
      "gonk.notifySoftLowMemUnderKB", /* default */ 43008);
    Preferences::AddUintVarCache(&mHardLowMemTriggerKB,
      "gonk.notifyHardLowMemUnderKB", /* default */ 14336);
  }

  nsresult OpenFiles() {
    mTriggerFd = open("/sys/kernel/mm/lowmemkiller/notify_trigger_active",
                      O_RDONLY | O_CLOEXEC);
    NS_ENSURE_STATE(mTriggerFd != -1);

    int pipes[2];
    NS_ENSURE_STATE(!pipe(pipes));
    mShutdownPipeRead = pipes[0];
    mShutdownPipeWrite = pipes[1];
    return NS_OK;
  }

  /**
   * Set the low memory trigger to the specified value, this can be done by
   * the main process alone.
   */
  void SetLowMemTrigger(uint32_t aValue) {
    if (XRE_IsParentProcess()) {
      nsPrintfCString str("%ld", (aValue * 1024) / mPageSize);
      if (WriteSysFile("/sys/module/lowmemorykiller/parameters/notify_trigger",
                       str.get())) {
        mLowMemTriggerKB = aValue;
      }
    }
  }

  /**
   * Read from the trigger file descriptor and determine whether we're
   * currently under memory pressure.
   *
   * We don't expect this method to block.
   */
  nsresult CheckForMemoryPressure(bool* aOut)
  {
    *aOut = false;

    lseek(mTriggerFd, 0, SEEK_SET);

    char buf[2];
    int nread = MOZ_TEMP_FAILURE_RETRY(read(mTriggerFd, buf, sizeof(buf)));
    NS_ENSURE_STATE(nread == 2);

    // The notify_trigger_active sysfs node should contain either "0\n" or
    // "1\n".  The latter indicates memory pressure.
    *aOut = (buf[0] == '1');
    return NS_OK;
  }

  int AdjustTrigger(int timeout)
  {
    if (!XRE_IsParentProcess()) {
      return -1; // Only the main process can adjust the trigger.
    }

    struct sysinfo info;
    int rv = sysinfo(&info);
    if (rv < 0) {
      return -1; // Without system information we're blind, bail out.
    }

    size_t freeMemory = (info.freeram * info.mem_unit) / 1024;

    if (freeMemory > mSoftLowMemTriggerKB) {
      SetLowMemTrigger(mSoftLowMemTriggerKB);
      return -1; // Trigger adjusted, wait indefinitely.
    }

    // Wait again but double the duration, max once per day.
    return std::min(86400000, timeout * 2);
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
  uint32_t mPollMS; // Ongoing pressure poll delay
  uint32_t mSoftLowMemTriggerKB; // Soft memory pressure level
  uint32_t mHardLowMemTriggerKB; // Hard memory pressure level
  uint32_t mLowMemTriggerKB; // Current value of the trigger
  size_t mPageSize;
  bool mShuttingDown;

  ScopedClose mTriggerFd;
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
  RefPtr<MemoryPressureWatcher> memoryPressureWatcher =
    new MemoryPressureWatcher();
  NS_ENSURE_SUCCESS_VOID(memoryPressureWatcher->Init());

  nsCOMPtr<nsIThread> thread;
  NS_NewNamedThread("MemoryPressure", getter_AddRefs(thread),
                    memoryPressureWatcher);
}

} // namespace mozilla
