/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Firefox.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation <http://www.mozilla.org>.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "mozilla/HangMonitor.h"
#include "mozilla/Monitor.h"
#include "mozilla/Preferences.h"
#include "mozilla/Telemetry.h"
#include "nsXULAppAPI.h"
#include "nsThreadUtils.h"
#include "nsStackWalk.h"

#ifdef MOZ_CRASHREPORTER
#include "nsExceptionHandler.h"
#endif

#ifdef XP_WIN
#include <windows.h>
#endif

#if defined(MOZ_ENABLE_PROFILER_SPS) && defined(MOZ_PROFILING) && defined(XP_WIN)
  #define REPORT_CHROME_HANGS
#endif

namespace mozilla { namespace HangMonitor {

/**
 * A flag which may be set from within a debugger to disable the hang
 * monitor.
 */
volatile bool gDebugDisableHangMonitor = false;

const char kHangMonitorPrefName[] = "hangmonitor.timeout";

const char kTelemetryPrefName[] = "toolkit.telemetry.enabled";

// Monitor protects gShutdown and gTimeout, but not gTimestamp which rely on
// being atomically set by the processor; synchronization doesn't really matter
// in this use case.
Monitor* gMonitor;

// The timeout preference, in seconds.
PRInt32 gTimeout;

PRThread* gThread;

// Set when shutdown begins to signal the thread to exit immediately.
bool gShutdown;

// The timestamp of the last event notification, or PR_INTERVAL_NO_WAIT if
// we're currently not processing events.
volatile PRIntervalTime gTimestamp;

#ifdef REPORT_CHROME_HANGS
// Main thread ID used in reporting chrome hangs under Windows
static HANDLE winMainThreadHandle = NULL;

// Default timeout for reporting chrome hangs to Telemetry (10 seconds)
static const PRInt32 DEFAULT_CHROME_HANG_INTERVAL = 10;
#endif

// PrefChangedFunc
int
PrefChanged(const char*, void*)
{
  PRInt32 newval = Preferences::GetInt(kHangMonitorPrefName);
#ifdef REPORT_CHROME_HANGS
  // Monitor chrome hangs on the profiling branch if Telemetry enabled
  if (newval == 0) {
    bool telemetryEnabled = Preferences::GetBool(kTelemetryPrefName);
    if (telemetryEnabled) {
      newval = DEFAULT_CHROME_HANG_INTERVAL;
    }
  }
#endif
  MonitorAutoLock lock(*gMonitor);
  if (newval != gTimeout) {
    gTimeout = newval;
    lock.Notify();
  }

  return 0;
}

void
Crash()
{
  if (gDebugDisableHangMonitor) {
    return;
  }

#ifdef XP_WIN
  if (::IsDebuggerPresent()) {
    return;
  }
#endif

#ifdef MOZ_CRASHREPORTER
  CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("Hang"),
                                     NS_LITERAL_CSTRING("1"));
#endif

  NS_RUNTIMEABORT("HangMonitor triggered");
}

#ifdef REPORT_CHROME_HANGS
static void
ChromeStackWalker(void *aPC, void *aClosure)
{
  MOZ_ASSERT(aClosure);
  Telemetry::HangStack *callStack =
    reinterpret_cast< Telemetry::HangStack* >(aClosure);
  callStack->AppendElement(reinterpret_cast<uintptr_t>(aPC));
}

static void
GetChromeHangReport(Telemetry::HangStack &callStack, SharedLibraryInfo &moduleMap)
{
  MOZ_ASSERT(winMainThreadHandle);

  DWORD ret = ::SuspendThread(winMainThreadHandle);
  if (ret == -1) {
    callStack.Clear();
    moduleMap.Clear();
    return;
  }
  NS_StackWalk(ChromeStackWalker, 0, &callStack,
               reinterpret_cast<uintptr_t>(winMainThreadHandle));
  ret = ::ResumeThread(winMainThreadHandle);
  if (ret == -1) {
    callStack.Clear();
    moduleMap.Clear();
    return;
  }

  moduleMap = SharedLibraryInfo::GetInfoForSelf();
  moduleMap.SortByAddress();

  // Remove all modules not referenced by a PC on the stack
  Telemetry::HangStack sortedStack = callStack;
  sortedStack.Sort();

  size_t moduleIndex = 0;
  size_t stackIndex = 0;
  bool unreferencedModule = true;
  while (stackIndex < sortedStack.Length() && moduleIndex < moduleMap.GetSize()) {
    uintptr_t pc = sortedStack[stackIndex];
    SharedLibrary& module = moduleMap.GetEntry(moduleIndex);
    uintptr_t moduleStart = module.GetStart();
    uintptr_t moduleEnd = module.GetEnd() - 1;
    if (moduleStart <= pc && pc <= moduleEnd) {
      // If the current PC is within the current module, mark module as used
      unreferencedModule = false;
      ++stackIndex;
    } else if (pc > moduleEnd) {
      if (unreferencedModule) {
        // Remove module if no PCs within its address range
        moduleMap.RemoveEntries(moduleIndex, moduleIndex + 1);
      } else {
        // Module was referenced on stack, but current PC belongs to later module
        unreferencedModule = true;
        ++moduleIndex;
      }
    } else {
      // PC does not belong to any module
      ++stackIndex;
    }
  }

  // Clean up remaining unreferenced modules, i.e. module addresses > max(pc)
  if (moduleIndex + 1 < moduleMap.GetSize()) {
    moduleMap.RemoveEntries(moduleIndex + 1, moduleMap.GetSize());
  }
}
#endif

void
ThreadMain(void*)
{
  MonitorAutoLock lock(*gMonitor);

  // In order to avoid issues with the hang monitor incorrectly triggering
  // during a general system stop such as sleeping, the monitor thread must
  // run twice to trigger hang protection.
  PRIntervalTime lastTimestamp = 0;
  int waitCount = 0;

#ifdef REPORT_CHROME_HANGS
  Telemetry::HangStack hangStack;
  SharedLibraryInfo hangModuleMap;
#endif

  while (true) {
    if (gShutdown) {
      return; // Exit the thread
    }

    // avoid rereading the volatile value in this loop
    PRIntervalTime timestamp = gTimestamp;

    PRIntervalTime now = PR_IntervalNow();

    if (timestamp != PR_INTERVAL_NO_WAIT &&
        now < timestamp) {
      // 32-bit overflow, reset for another waiting period
      timestamp = 1; // lowest legal PRInterval value
    }

    if (timestamp != PR_INTERVAL_NO_WAIT &&
        timestamp == lastTimestamp &&
        gTimeout > 0) {
      ++waitCount;
      if (waitCount == 2) {
#ifdef REPORT_CHROME_HANGS
        GetChromeHangReport(hangStack, hangModuleMap);
#else
        PRInt32 delay =
          PRInt32(PR_IntervalToSeconds(now - timestamp));
        if (delay > gTimeout) {
          MonitorAutoUnlock unlock(*gMonitor);
          Crash();
        }
#endif
      }
    }
    else {
#ifdef REPORT_CHROME_HANGS
      if (waitCount >= 2) {
        PRUint32 hangDuration = PR_IntervalToSeconds(now - lastTimestamp);
        Telemetry::RecordChromeHang(hangDuration, hangStack, hangModuleMap);
        hangStack.Clear();
        hangModuleMap.Clear();
      }
#endif
      lastTimestamp = timestamp;
      waitCount = 0;
    }

    PRIntervalTime timeout;
    if (gTimeout <= 0) {
      timeout = PR_INTERVAL_NO_TIMEOUT;
    }
    else {
      timeout = PR_MillisecondsToInterval(gTimeout * 500);
    }
    lock.Wait(timeout);
  }
}

void
Startup()
{
  // The hang detector only runs in chrome processes. If you change this,
  // you must also deal with the threadsafety of AnnotateCrashReport in
  // non-chrome processes!
  if (GeckoProcessType_Default != XRE_GetProcessType())
    return;

  NS_ASSERTION(!gMonitor, "Hang monitor already initialized");
  gMonitor = new Monitor("HangMonitor");

  Preferences::RegisterCallback(PrefChanged, kHangMonitorPrefName, NULL);
  PrefChanged(NULL, NULL);

#ifdef REPORT_CHROME_HANGS
  Preferences::RegisterCallback(PrefChanged, kTelemetryPrefName, NULL);
  winMainThreadHandle =
    OpenThread(THREAD_ALL_ACCESS, FALSE, GetCurrentThreadId());
  if (!winMainThreadHandle)
    return;
#endif

  // Don't actually start measuring hangs until we hit the main event loop.
  // This potentially misses a small class of really early startup hangs,
  // but avoids dealing with some xpcshell tests and other situations which
  // start XPCOM but don't ever start the event loop.
  Suspend();

  gThread = PR_CreateThread(PR_USER_THREAD,
                            ThreadMain,
                            NULL, PR_PRIORITY_LOW, PR_GLOBAL_THREAD,
                            PR_JOINABLE_THREAD, 0);
}

void
Shutdown()
{
  if (GeckoProcessType_Default != XRE_GetProcessType())
    return;

  NS_ASSERTION(gMonitor, "Hang monitor not started");

  { // Scope the lock we're going to delete later
    MonitorAutoLock lock(*gMonitor);
    gShutdown = true;
    lock.Notify();
  }

  // thread creation could theoretically fail
  if (gThread) {
    PR_JoinThread(gThread);
    gThread = NULL;
  }

  delete gMonitor;
  gMonitor = NULL;
}

void
NotifyActivity()
{
  NS_ASSERTION(NS_IsMainThread(), "HangMonitor::Notify called from off the main thread.");

  // This is not a locked activity because PRTimeStamp is a 32-bit quantity
  // which can be read/written atomically, and we don't want to pay locking
  // penalties here.
  gTimestamp = PR_IntervalNow();
}

void
Suspend()
{
  NS_ASSERTION(NS_IsMainThread(), "HangMonitor::Suspend called from off the main thread.");

  // Because gTimestamp changes this resets the wait count.
  gTimestamp = PR_INTERVAL_NO_WAIT;
}

} } // namespace mozilla::HangMonitor
