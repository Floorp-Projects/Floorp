/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 ci et: */
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
 * The Original Code is Mozilla.org code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * 
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Justin Lebar <justin.lebar@gmail.com>
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

#include "mozilla/AvailableMemoryTracker.h"
#include "nsThread.h"
#include "nsIObserverService.h"
#include "mozilla/Services.h"
#include "mozilla/Preferences.h"
#include "nsWindowsDllInterceptor.h"
#include "prinrval.h"
#include "pratom.h"
#include "prenv.h"
#include "nsIMemoryReporter.h"
#include "nsPrintfCString.h"
#include <windows.h>

using namespace mozilla;

namespace {

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

void safe_write(const char *a)
{
  // Well, puts isn't exactly "safe", but at least it doesn't call malloc...
  fputs(a, stdout);
}

void safe_write(PRUint64 x)
{
  // 2^64 is 20 decimal digits.
  const int max_len = 21;
  char buf[max_len];
  buf[max_len - 1] = '\0';

  PRUint32 i;
  for (i = max_len - 2; i >= 0 && x > 0; i--)
  {
    buf[i] = "0123456789"[x % 10];
    x /= 10;
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

PRUint32 sLowVirtualMemoryThreshold = 0;
PRUint32 sLowCommitSpaceThreshold = 0;
PRUint32 sLowPhysicalMemoryThreshold = 0;
PRUint32 sLowMemoryNotificationIntervalMS = 0;

PRUint32 sNumLowVirtualMemEvents = 0;
PRUint32 sNumLowCommitSpaceEvents = 0;
PRUint32 sNumLowPhysicalMemEvents = 0;

WindowsDllInterceptor sKernel32Intercept;
WindowsDllInterceptor sGdi32Intercept;

// Have we installed the kernel intercepts above?
bool sHooksInstalled = false;

// Alas, we'd like to use mozilla::TimeStamp, but we can't, because it acquires
// a lock!
volatile bool sHasScheduledOneLowMemoryNotification = false;
volatile PRIntervalTime sLastLowMemoryNotificationTime;

// These are function pointers to the functions we wrap in Init().

void* (WINAPI *sVirtualAllocOrig)
  (LPVOID aAddress, SIZE_T aSize, DWORD aAllocationType, DWORD aProtect);

void* (WINAPI *sMapViewOfFileOrig)
  (HANDLE aFileMappingObject, DWORD aDesiredAccess,
   DWORD aFileOffsetHigh, DWORD aFileOffsetLow,
   SIZE_T aNumBytesToMap);

HBITMAP (WINAPI *sCreateDIBSectionOrig)
  (HDC aDC, const BITMAPINFO *aBitmapInfo,
   UINT aUsage, VOID **aBits,
   HANDLE aSection, DWORD aOffset);

/**
 * Fire a memory pressure event if it's been long enough since the last one we
 * fired.
 */
bool MaybeScheduleMemoryPressureEvent()
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
  ScheduleMemoryPressureEvent();
  return true;
}

void CheckMemAvailable()
{
  MEMORYSTATUSEX stat;
  stat.dwLength = sizeof(stat);
  bool success = GlobalMemoryStatusEx(&stat);

  DEBUG_WARN_IF_FALSE(success, "GlobalMemoryStatusEx failed.");

  if (success)
  {
    // sLowVirtualMemoryThreshold is in MB, but ullAvailVirtual is in bytes.
    if (stat.ullAvailVirtual < sLowVirtualMemoryThreshold * 1024 * 1024) {
      // If we're running low on virtual memory, unconditionally schedule the
      // notification.  We'll probably crash if we run out of virtual memory,
      // so don't worry about firing this notification too often.
      LOG("Detected low virtual memory.");
      PR_ATOMIC_INCREMENT(&sNumLowVirtualMemEvents);
      ScheduleMemoryPressureEvent();
    }
    else if (stat.ullAvailPageFile < sLowCommitSpaceThreshold * 1024 * 1024) {
      LOG("Detected low available page file space.");
      if (MaybeScheduleMemoryPressureEvent()) {
        PR_ATOMIC_INCREMENT(&sNumLowCommitSpaceEvents);
      }
    }
    else if (stat.ullAvailPhys < sLowPhysicalMemoryThreshold * 1024 * 1024) {
      LOG("Detected low physical memory.");
      if (MaybeScheduleMemoryPressureEvent()) {
        PR_ATOMIC_INCREMENT(&sNumLowPhysicalMemEvents);
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
      (sLowPhysicalMemoryThreshold != 0 &&  aAllocationType & MEM_COMMIT)) {
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
                     const BITMAPINFO *aBitmapInfo,
                     UINT aUsage,
                     VOID **aBits,
                     HANDLE aSection,
                     DWORD aOffset)
{
  // There are a lot of calls to CreateDIBSection, so we make some effort not
  // to CheckMemAvailable() for calls to CreateDIBSection which allocate only
  // a small amount of memory.

  // If aSection is non-null, CreateDIBSection won't allocate any new memory.
  PRBool doCheck = PR_FALSE;
  if (!aSection && aBitmapInfo) {
    PRUint16 bitCount = aBitmapInfo->bmiHeader.biBitCount;
    if (bitCount == 0) {
      // MSDN says bitCount == 0 means that it figures out how many bits each
      // pixel gets by examining the corresponding JPEG or PNG data.  We'll just
      // assume the worst.
      bitCount = 32;
    }

    // |size| contains the expected allocation size in *bits*.  Height may be
    // negative (indicating the direction the DIB is drawn in), so we take the
    // absolute value.
    PRInt64 size = bitCount * aBitmapInfo->bmiHeader.biWidth *
                              aBitmapInfo->bmiHeader.biHeight;
    if (size < 0)
      size *= -1;

    // If we're allocating more than 1MB, check how much memory is left after
    // the allocation.
    if (size > 1024 * 1024 * 8) {
      LOG3("CreateDIBSectionHook: Large allocation (size=", size, ")");
      doCheck = PR_TRUE;
    }
  }

  HBITMAP result = sCreateDIBSectionOrig(aDC, aBitmapInfo, aUsage, aBits,
                                         aSection, aOffset);

  if (doCheck) {
    CheckMemAvailable();
  }

  return result;
}

class NumLowMemoryEventsReporter : public nsIMemoryReporter
{
  NS_IMETHOD GetProcess(nsACString &aProcess)
  {
    aProcess.Truncate();
    return NS_OK;
  }

  NS_IMETHOD GetKind(int *aKind)
  {
    *aKind = KIND_OTHER;
    return NS_OK;
  }

  NS_IMETHOD GetUnits(int *aUnits)
  {
    *aUnits = UNITS_COUNT_CUMULATIVE;
    return NS_OK;
  }
};

class NumLowVirtualMemoryEventsMemoryReporter : public NumLowMemoryEventsReporter
{
public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD GetPath(nsACString &aPath)
  {
    aPath.AssignLiteral("low-memory-events-virtual");
    return NS_OK;
  }

  NS_IMETHOD GetAmount(PRInt64 *aAmount)
  {
    // This memory reporter shouldn't be installed on 64-bit machines, since we
    // force-disable virtual-memory tracking there.
    MOZ_ASSERT(sizeof(void*) == 4);

    *aAmount = sNumLowVirtualMemEvents;
    return NS_OK;
  }

  NS_IMETHOD GetDescription(nsACString &aDescription)
  {
    aDescription.AssignLiteral(
      "Number of low-virtual-memory events fired since startup. ");

    if (sLowVirtualMemoryThreshold == 0 || !sHooksInstalled) {
      aDescription.Append(nsPrintfCString(1024,
        "Tracking low-virtual-memory events is disabled, but you can enable it "
        "by giving the memory.low_virtual_mem_threshold_mb pref a non-zero "
        "value%s.",
        sHooksInstalled ? "" : " and restarting"));
    }
    else {
      aDescription.Append(nsPrintfCString(1024,
        "We fire such an event if we notice there is less than %d MB of virtual "
        "address space available (controlled by the "
        "'memory.low_virtual_mem_threshold_mb' pref).  We'll likely crash if "
        "we run out of virtual address space, so this event is somewhat dire.",
        sLowVirtualMemoryThreshold));
    }
    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS1(NumLowVirtualMemoryEventsMemoryReporter, nsIMemoryReporter)

class NumLowCommitSpaceEventsMemoryReporter : public NumLowMemoryEventsReporter
{
public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD GetPath(nsACString &aPath)
  {
    aPath.AssignLiteral("low-commit-space-events");
    return NS_OK;
  }

  NS_IMETHOD GetAmount(PRInt64 *aAmount)
  {
    *aAmount = sNumLowCommitSpaceEvents;
    return NS_OK;
  }

  NS_IMETHOD GetDescription(nsACString &aDescription)
  {
    aDescription.AssignLiteral(
      "Number of low-commit-space events fired since startup. ");

    if (sLowCommitSpaceThreshold == 0 || !sHooksInstalled) {
      aDescription.Append(nsPrintfCString(1024,
        "Tracking low-commit-space events is disabled, but you can enable it "
        "by giving the memory.low_commit_space_threshold_mb pref a non-zero "
        "value%s.",
        sHooksInstalled ? "" : " and restarting"));
    }
    else {
      aDescription.Append(nsPrintfCString(1024,
        "We fire such an event if we notice there is less than %d MB of "
        "available commit space (controlled by the "
        "'memory.low_commit_space_threshold_mb' pref).  Windows will likely "
        "kill us if we run out of commit space, so this event is somewhat dire.",
        sLowCommitSpaceThreshold));
    }
    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS1(NumLowCommitSpaceEventsMemoryReporter, nsIMemoryReporter)

class NumLowPhysicalMemoryEventsMemoryReporter : public NumLowMemoryEventsReporter
{
public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD GetPath(nsACString &aPath)
  {
    aPath.AssignLiteral("low-memory-events-physical");
    return NS_OK;
  }

  NS_IMETHOD GetAmount(PRInt64 *aAmount)
  {
    *aAmount = sNumLowPhysicalMemEvents;
    return NS_OK;
  }

  NS_IMETHOD GetDescription(nsACString &aDescription)
  {
    aDescription.AssignLiteral(
      "Number of low-physical-memory events fired since startup. ");

    if (sLowPhysicalMemoryThreshold == 0 || !sHooksInstalled) {
      aDescription.Append(nsPrintfCString(1024,
        "Tracking low-physical-memory events is disabled, but you can enable it "
        "by giving the memory.low_physical_memory_threshold_mb pref a non-zero "
        "value%s.",
        sHooksInstalled ? "" : " and restarting"));
    }
    else {
      aDescription.Append(nsPrintfCString(1024,
        "We fire such an event if we notice there is less than %d MB of "
        "available physical memory (controlled by the "
        "'memory.low_physical_memory_threshold_mb' pref).  The machine will start "
        "to page if it runs out of physical memory; this may cause it to run "
        "slowly, but it shouldn't cause us to crash.",
        sLowPhysicalMemoryThreshold));
    }
    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS1(NumLowPhysicalMemoryEventsMemoryReporter, nsIMemoryReporter)

} // anonymous namespace

namespace mozilla {
namespace AvailableMemoryTracker {

void Init()
{
  // On 64-bit systems, hardcode sLowVirtualMemoryThreshold to 0 -- we assume
  // we're not going to run out of virtual memory!
  if (sizeof(void*) > 4) {
    sLowVirtualMemoryThreshold = 0;
  }
  else {
    Preferences::AddUintVarCache(&sLowVirtualMemoryThreshold,
        "memory.low_virtual_mem_threshold_mb", 128);
  }

  Preferences::AddUintVarCache(&sLowPhysicalMemoryThreshold,
      "memory.low_physical_memory_threshold_mb", 0);
  Preferences::AddUintVarCache(&sLowCommitSpaceThreshold,
      "memory.low_commit_space_threshold_mb", 128);
  Preferences::AddUintVarCache(&sLowMemoryNotificationIntervalMS,
      "memory.low_memory_notification_interval_ms", 10000);

  // Don't register the hooks if we're a build instrumented for PGO or if both
  // thresholds are 0.  (If we're an instrumented build, the compiler adds
  // function calls all over the place which may call VirtualAlloc; this makes
  // it hard to prevent VirtualAllocHook from reentering itself.)

  if (!PR_GetEnv("MOZ_PGO_INSTRUMENTED") &&
      (sLowVirtualMemoryThreshold != 0 || sLowPhysicalMemoryThreshold != 0)) {
    sHooksInstalled = true;
    sKernel32Intercept.Init("Kernel32.dll");
    sKernel32Intercept.AddHook("VirtualAlloc",
      reinterpret_cast<intptr_t>(VirtualAllocHook),
      (void**) &sVirtualAllocOrig);
    sKernel32Intercept.AddHook("MapViewOfFile",
      reinterpret_cast<intptr_t>(MapViewOfFileHook),
      (void**) &sMapViewOfFileOrig);

    sGdi32Intercept.Init("Gdi32.dll");
    sGdi32Intercept.AddHook("CreateDIBSection",
      reinterpret_cast<intptr_t>(CreateDIBSectionHook),
      (void**) &sCreateDIBSectionOrig);
  }
  else {
    sHooksInstalled = false;
  }

  NS_RegisterMemoryReporter(new NumLowCommitSpaceEventsMemoryReporter());
  NS_RegisterMemoryReporter(new NumLowPhysicalMemoryEventsMemoryReporter());
  if (sizeof(void*) == 4) {
    NS_RegisterMemoryReporter(new NumLowVirtualMemoryEventsMemoryReporter());
  }
}

} // namespace AvailableMemoryTracker
} // namespace mozilla
