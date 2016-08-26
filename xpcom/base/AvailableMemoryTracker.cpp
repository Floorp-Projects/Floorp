/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/AvailableMemoryTracker.h"

#if defined(XP_WIN)
#include "prinrval.h"
#include "prenv.h"
#include "nsIMemoryReporter.h"
#include "nsMemoryPressure.h"
#endif

#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIRunnable.h"
#include "nsISupports.h"
#include "nsThreadUtils.h"

#include "mozilla/Preferences.h"
#include "mozilla/Services.h"

#if defined(XP_WIN)
#   include "nsWindowsDllInterceptor.h"
#   include <windows.h>
#endif

#if defined(MOZ_MEMORY)
#   include "mozmemory.h"
#endif  // MOZ_MEMORY

using namespace mozilla;

namespace {

#if defined(XP_WIN)

// We don't want our diagnostic functions to call malloc, because that could
// call VirtualAlloc, and we'd end up back in here!  So here are a few simple
// debugging macros (modeled on jemalloc's), which hopefully won't allocate.

// #define LOGGING_ENABLED

#ifdef LOGGING_ENABLED

#define LOG(msg)       \
  do {                 \
    safe_write(msg);   \
    safe_write("\n");  \
  } while(0)

#define LOG2(m1, m2)   \
  do {                 \
    safe_write(m1);    \
    safe_write(m2);    \
    safe_write("\n");  \
  } while(0)

#define LOG3(m1, m2, m3) \
  do {                   \
    safe_write(m1);      \
    safe_write(m2);      \
    safe_write(m3);      \
    safe_write("\n");    \
  } while(0)

#define LOG4(m1, m2, m3, m4) \
  do {                       \
    safe_write(m1);          \
    safe_write(m2);          \
    safe_write(m3);          \
    safe_write(m4);          \
    safe_write("\n");        \
  } while(0)

#else

#define LOG(msg)
#define LOG2(m1, m2)
#define LOG3(m1, m2, m3)
#define LOG4(m1, m2, m3, m4)

#endif

void
safe_write(const char* aStr)
{
  // Well, puts isn't exactly "safe", but at least it doesn't call malloc...
  fputs(aStr, stdout);
}

void
safe_write(uint64_t aNum)
{
  // 2^64 is 20 decimal digits.
  const unsigned int max_len = 21;
  char buf[max_len];
  buf[max_len - 1] = '\0';

  uint32_t i;
  for (i = max_len - 2; i < max_len && aNum > 0; i--) {
    buf[i] = "0123456789"[aNum % 10];
    aNum /= 10;
  }

  safe_write(&buf[i + 1]);
}

#ifdef DEBUG
#define DEBUG_WARN_IF_FALSE(cond, msg)          \
  do {                                          \
    if (!(cond)) {                              \
      safe_write(__FILE__);                     \
      safe_write(":");                          \
      safe_write(__LINE__);                     \
      safe_write(" ");                          \
      safe_write(msg);                          \
      safe_write("\n");                         \
    }                                           \
  } while(0)
#else
#define DEBUG_WARN_IF_FALSE(cond, msg)
#endif

uint32_t sLowVirtualMemoryThreshold = 0;
uint32_t sLowCommitSpaceThreshold = 0;
uint32_t sLowPhysicalMemoryThreshold = 0;
uint32_t sLowMemoryNotificationIntervalMS = 0;

Atomic<uint32_t> sNumLowVirtualMemEvents;
Atomic<uint32_t> sNumLowCommitSpaceEvents;
Atomic<uint32_t> sNumLowPhysicalMemEvents;

WindowsDllInterceptor sKernel32Intercept;
WindowsDllInterceptor sGdi32Intercept;

// Has Init() been called?
bool sInitialized = false;

// Has Activate() been called?  The hooks don't do anything until this happens.
bool sHooksActive = false;

// Alas, we'd like to use mozilla::TimeStamp, but we can't, because it acquires
// a lock!
volatile bool sHasScheduledOneLowMemoryNotification = false;
volatile PRIntervalTime sLastLowMemoryNotificationTime;

// These are function pointers to the functions we wrap in Init().

void* (WINAPI* sVirtualAllocOrig)(LPVOID aAddress, SIZE_T aSize,
                                  DWORD aAllocationType, DWORD aProtect);

void* (WINAPI* sMapViewOfFileOrig)(HANDLE aFileMappingObject,
                                   DWORD aDesiredAccess, DWORD aFileOffsetHigh,
                                   DWORD aFileOffsetLow, SIZE_T aNumBytesToMap);

HBITMAP(WINAPI* sCreateDIBSectionOrig)(HDC aDC, const BITMAPINFO* aBitmapInfo,
                                       UINT aUsage, VOID** aBits,
                                       HANDLE aSection, DWORD aOffset);

/**
 * Fire a memory pressure event if it's been long enough since the last one we
 * fired.
 */
bool
MaybeScheduleMemoryPressureEvent()
{
  // If this interval rolls over, we may fire an extra memory pressure
  // event, but that's not a big deal.
  PRIntervalTime interval = PR_IntervalNow() - sLastLowMemoryNotificationTime;
  if (sHasScheduledOneLowMemoryNotification &&
      PR_IntervalToMilliseconds(interval) < sLowMemoryNotificationIntervalMS) {

    LOG("Not scheduling low physical memory notification, "
        "because not enough time has elapsed since last one.");
    return false;
  }

  // There's a bit of a race condition here, since an interval may be a
  // 64-bit number, and 64-bit writes aren't atomic on x86-32.  But let's
  // not worry about it -- the races only happen when we're already
  // experiencing memory pressure and firing notifications, so the worst
  // thing that can happen is that we fire two notifications when we
  // should have fired only one.
  sHasScheduledOneLowMemoryNotification = true;
  sLastLowMemoryNotificationTime = PR_IntervalNow();

  LOG("Scheduling memory pressure notification.");
  NS_DispatchEventualMemoryPressure(MemPressure_New);
  return true;
}

void
CheckMemAvailable()
{
  if (!sHooksActive) {
    return;
  }

  MEMORYSTATUSEX stat;
  stat.dwLength = sizeof(stat);
  bool success = GlobalMemoryStatusEx(&stat);

  DEBUG_WARN_IF_FALSE(success, "GlobalMemoryStatusEx failed.");

  if (success) {
    // sLowVirtualMemoryThreshold is in MB, but ullAvailVirtual is in bytes.
    if (stat.ullAvailVirtual < sLowVirtualMemoryThreshold * 1024 * 1024) {
      // If we're running low on virtual memory, unconditionally schedule the
      // notification.  We'll probably crash if we run out of virtual memory,
      // so don't worry about firing this notification too often.
      LOG("Detected low virtual memory.");
      ++sNumLowVirtualMemEvents;
      NS_DispatchEventualMemoryPressure(MemPressure_New);
    } else if (stat.ullAvailPageFile < sLowCommitSpaceThreshold * 1024 * 1024) {
      LOG("Detected low available page file space.");
      if (MaybeScheduleMemoryPressureEvent()) {
        ++sNumLowCommitSpaceEvents;
      }
    } else if (stat.ullAvailPhys < sLowPhysicalMemoryThreshold * 1024 * 1024) {
      LOG("Detected low physical memory.");
      if (MaybeScheduleMemoryPressureEvent()) {
        ++sNumLowPhysicalMemEvents;
      }
    }
  }
}

LPVOID WINAPI
VirtualAllocHook(LPVOID aAddress, SIZE_T aSize,
                 DWORD aAllocationType,
                 DWORD aProtect)
{
  // It's tempting to see whether we have enough free virtual address space for
  // this allocation and, if we don't, synchronously fire a low-memory
  // notification to free some before we allocate.
  //
  // Unfortunately that doesn't work, principally because code doesn't expect a
  // call to malloc could trigger a GC (or call into the other routines which
  // are triggered by a low-memory notification).
  //
  // I think the best we can do here is try to allocate the memory and check
  // afterwards how much free virtual address space we have.  If we're running
  // low, we schedule a low-memory notification to run as soon as possible.

  LPVOID result = sVirtualAllocOrig(aAddress, aSize, aAllocationType, aProtect);

  // Don't call CheckMemAvailable for MEM_RESERVE if we're not tracking low
  // virtual memory.  Similarly, don't call CheckMemAvailable for MEM_COMMIT if
  // we're not tracking low physical memory.
  if ((sLowVirtualMemoryThreshold != 0 && aAllocationType & MEM_RESERVE) ||
      (sLowPhysicalMemoryThreshold != 0 && aAllocationType & MEM_COMMIT)) {
    LOG3("VirtualAllocHook(size=", aSize, ")");
    CheckMemAvailable();
  }

  return result;
}

LPVOID WINAPI
MapViewOfFileHook(HANDLE aFileMappingObject,
                  DWORD aDesiredAccess,
                  DWORD aFileOffsetHigh,
                  DWORD aFileOffsetLow,
                  SIZE_T aNumBytesToMap)
{
  LPVOID result = sMapViewOfFileOrig(aFileMappingObject, aDesiredAccess,
                                     aFileOffsetHigh, aFileOffsetLow,
                                     aNumBytesToMap);
  LOG("MapViewOfFileHook");
  CheckMemAvailable();
  return result;
}

HBITMAP WINAPI
CreateDIBSectionHook(HDC aDC,
                     const BITMAPINFO* aBitmapInfo,
                     UINT aUsage,
                     VOID** aBits,
                     HANDLE aSection,
                     DWORD aOffset)
{
  // There are a lot of calls to CreateDIBSection, so we make some effort not
  // to CheckMemAvailable() for calls to CreateDIBSection which allocate only
  // a small amount of memory.

  // If aSection is non-null, CreateDIBSection won't allocate any new memory.
  bool doCheck = false;
  if (sHooksActive && !aSection && aBitmapInfo) {
    uint16_t bitCount = aBitmapInfo->bmiHeader.biBitCount;
    if (bitCount == 0) {
      // MSDN says bitCount == 0 means that it figures out how many bits each
      // pixel gets by examining the corresponding JPEG or PNG data.  We'll just
      // assume the worst.
      bitCount = 32;
    }

    // |size| contains the expected allocation size in *bits*.  Height may be
    // negative (indicating the direction the DIB is drawn in), so we take the
    // absolute value.
    int64_t size = bitCount * aBitmapInfo->bmiHeader.biWidth *
                              aBitmapInfo->bmiHeader.biHeight;
    if (size < 0) {
      size *= -1;
    }

    // If we're allocating more than 1MB, check how much memory is left after
    // the allocation.
    if (size > 1024 * 1024 * 8) {
      LOG3("CreateDIBSectionHook: Large allocation (size=", size, ")");
      doCheck = true;
    }
  }

  HBITMAP result = sCreateDIBSectionOrig(aDC, aBitmapInfo, aUsage, aBits,
                                         aSection, aOffset);

  if (doCheck) {
    CheckMemAvailable();
  }

  return result;
}

static int64_t
LowMemoryEventsVirtualDistinguishedAmount()
{
  return sNumLowVirtualMemEvents;
}

static int64_t
LowMemoryEventsPhysicalDistinguishedAmount()
{
  return sNumLowPhysicalMemEvents;
}

class LowEventsReporter final : public nsIMemoryReporter
{
  ~LowEventsReporter() {}

public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                            nsISupports* aData, bool aAnonymize) override
  {
    // We only do virtual-memory tracking on 32-bit builds.
    if (sizeof(void*) == 4) {
      MOZ_COLLECT_REPORT(
        "low-memory-events/virtual", KIND_OTHER, UNITS_COUNT_CUMULATIVE,
        LowMemoryEventsVirtualDistinguishedAmount(),
"Number of low-virtual-memory events fired since startup. We fire such an "
"event if we notice there is less than memory.low_virtual_mem_threshold_mb of "
"virtual address space available (if zero, this behavior is disabled). The "
"process will probably crash if it runs out of virtual address space, so "
"this event is dire.");
    }

    MOZ_COLLECT_REPORT(
      "low-commit-space-events", KIND_OTHER, UNITS_COUNT_CUMULATIVE,
      sNumLowCommitSpaceEvents,
"Number of low-commit-space events fired since startup. We fire such an "
"event if we notice there is less than memory.low_commit_space_threshold_mb of "
"commit space available (if zero, this behavior is disabled). Windows will "
"likely kill the process if it runs out of commit space, so this event is "
"dire.");

    MOZ_COLLECT_REPORT(
      "low-memory-events/physical", KIND_OTHER, UNITS_COUNT_CUMULATIVE,
      LowMemoryEventsPhysicalDistinguishedAmount(),
"Number of low-physical-memory events fired since startup. We fire such an "
"event if we notice there is less than memory.low_physical_memory_threshold_mb "
"of physical memory available (if zero, this behavior is disabled).  The "
"machine will start to page if it runs out of physical memory.  This may "
"cause it to run slowly, but it shouldn't cause it to crash.");

    return NS_OK;
  }
};
NS_IMPL_ISUPPORTS(LowEventsReporter, nsIMemoryReporter)

#endif // defined(XP_WIN)

/**
 * This runnable is executed in response to a memory-pressure event; we spin
 * the event-loop when receiving the memory-pressure event in the hope that
 * other observers will synchronously free some memory that we'll be able to
 * purge here.
 */
class nsJemallocFreeDirtyPagesRunnable final : public nsIRunnable
{
  ~nsJemallocFreeDirtyPagesRunnable() {}

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE
};

NS_IMPL_ISUPPORTS(nsJemallocFreeDirtyPagesRunnable, nsIRunnable)

NS_IMETHODIMP
nsJemallocFreeDirtyPagesRunnable::Run()
{
  MOZ_ASSERT(NS_IsMainThread());

#if defined(MOZ_MEMORY)
  jemalloc_free_dirty_pages();
#endif

  return NS_OK;
}

/**
 * The memory pressure watcher is used for listening to memory-pressure events
 * and reacting upon them. We use one instance per process currently only for
 * cleaning up dirty unused pages held by jemalloc.
 */
class nsMemoryPressureWatcher final : public nsIObserver
{
  ~nsMemoryPressureWatcher() {}

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  void Init();

private:
  static bool sFreeDirtyPages;
};

NS_IMPL_ISUPPORTS(nsMemoryPressureWatcher, nsIObserver)

bool nsMemoryPressureWatcher::sFreeDirtyPages = false;

/**
 * Initialize and subscribe to the memory-pressure events. We subscribe to the
 * observer service in this method and not in the constructor because we need
 * to hold a strong reference to 'this' before calling the observer service.
 */
void
nsMemoryPressureWatcher::Init()
{
  nsCOMPtr<nsIObserverService> os = services::GetObserverService();

  if (os) {
    os->AddObserver(this, "memory-pressure", /* ownsWeak */ false);
  }

  Preferences::AddBoolVarCache(&sFreeDirtyPages, "memory.free_dirty_pages",
                               false);
}

/**
 * Reacts to all types of memory-pressure events, launches a runnable to
 * free dirty pages held by jemalloc.
 */
NS_IMETHODIMP
nsMemoryPressureWatcher::Observe(nsISupports* aSubject, const char* aTopic,
                                 const char16_t* aData)
{
  MOZ_ASSERT(!strcmp(aTopic, "memory-pressure"), "Unknown topic");

  if (sFreeDirtyPages) {
    nsCOMPtr<nsIRunnable> runnable = new nsJemallocFreeDirtyPagesRunnable();

    NS_DispatchToMainThread(runnable);
  }

  return NS_OK;
}

} // namespace

namespace mozilla {
namespace AvailableMemoryTracker {

void
Activate()
{
#if defined(_M_IX86) && defined(XP_WIN)
  MOZ_ASSERT(sInitialized);
  MOZ_ASSERT(!sHooksActive);

  // On 64-bit systems, hardcode sLowVirtualMemoryThreshold to 0 -- we assume
  // we're not going to run out of virtual memory!
  if (sizeof(void*) > 4) {
    sLowVirtualMemoryThreshold = 0;
  } else {
    Preferences::AddUintVarCache(&sLowVirtualMemoryThreshold,
                                 "memory.low_virtual_mem_threshold_mb", 128);
  }

  Preferences::AddUintVarCache(&sLowPhysicalMemoryThreshold,
                               "memory.low_physical_memory_threshold_mb", 0);
  Preferences::AddUintVarCache(&sLowCommitSpaceThreshold,
                               "memory.low_commit_space_threshold_mb", 128);
  Preferences::AddUintVarCache(&sLowMemoryNotificationIntervalMS,
                               "memory.low_memory_notification_interval_ms", 10000);

  RegisterStrongMemoryReporter(new LowEventsReporter());
  RegisterLowMemoryEventsVirtualDistinguishedAmount(
    LowMemoryEventsVirtualDistinguishedAmount);
  RegisterLowMemoryEventsPhysicalDistinguishedAmount(
    LowMemoryEventsPhysicalDistinguishedAmount);
  sHooksActive = true;
#endif

  // This object is held alive by the observer service.
  RefPtr<nsMemoryPressureWatcher> watcher = new nsMemoryPressureWatcher();
  watcher->Init();
}

void
Init()
{
  // Do nothing on x86-64, because nsWindowsDllInterceptor is not thread-safe
  // on 64-bit.  (On 32-bit, it's probably thread-safe.)  Even if we run Init()
  // before any other of our threads are running, another process may have
  // started a remote thread which could call VirtualAlloc!
  //
  // Moreover, the benefit of this code is less clear when we're a 64-bit
  // process, because we aren't going to run out of virtual memory, and the
  // system is likely to have a fair bit of physical memory.

#if defined(_M_IX86) && defined(XP_WIN)
  // Don't register the hooks if we're a build instrumented for PGO: If we're
  // an instrumented build, the compiler adds function calls all over the place
  // which may call VirtualAlloc; this makes it hard to prevent
  // VirtualAllocHook from reentering itself.
  if (!PR_GetEnv("MOZ_PGO_INSTRUMENTED")) {
    sKernel32Intercept.Init("Kernel32.dll");
    sKernel32Intercept.AddHook("VirtualAlloc",
                               reinterpret_cast<intptr_t>(VirtualAllocHook),
                               reinterpret_cast<void**>(&sVirtualAllocOrig));
    sKernel32Intercept.AddHook("MapViewOfFile",
                               reinterpret_cast<intptr_t>(MapViewOfFileHook),
                               reinterpret_cast<void**>(&sMapViewOfFileOrig));

    sGdi32Intercept.Init("Gdi32.dll");
    sGdi32Intercept.AddHook("CreateDIBSection",
                            reinterpret_cast<intptr_t>(CreateDIBSectionHook),
                            reinterpret_cast<void**>(&sCreateDIBSectionOrig));
  }

  sInitialized = true;
#endif
}

} // namespace AvailableMemoryTracker
} // namespace mozilla
