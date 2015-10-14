/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAtomTable.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsPrintfCString.h"
#include "nsServiceManagerUtils.h"
#include "nsMemoryReporterManager.h"
#include "nsITimer.h"
#include "nsThreadUtils.h"
#include "nsIDOMWindow.h"
#include "nsPIDOMWindow.h"
#include "nsIObserverService.h"
#include "nsIGlobalObject.h"
#include "nsIXPConnect.h"
#if defined(XP_UNIX) || defined(MOZ_DMD)
#include "nsMemoryInfoDumper.h"
#endif
#include "mozilla/Attributes.h"
#include "mozilla/PodOperations.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/Telemetry.h"
#include "mozilla/dom/PMemoryReportRequestParent.h" // for dom::MemoryReport
#include "mozilla/dom/ContentParent.h"
#include "mozilla/ipc/FileDescriptorUtils.h"

#ifdef XP_WIN
#include <process.h>
#ifndef getpid
#define getpid _getpid
#endif
#else
#include <unistd.h>
#endif

using namespace mozilla;

#if defined(MOZ_MEMORY)
#  define HAVE_JEMALLOC_STATS 1
#  include "mozmemory.h"
#endif  // MOZ_MEMORY

#if defined(XP_LINUX)

#include <malloc.h>
#include <string.h>
#include <stdlib.h>

static nsresult
GetProcSelfStatmField(int aField, int64_t* aN)
{
  // There are more than two fields, but we're only interested in the first
  // two.
  static const int MAX_FIELD = 2;
  size_t fields[MAX_FIELD];
  MOZ_ASSERT(aField < MAX_FIELD, "bad field number");
  FILE* f = fopen("/proc/self/statm", "r");
  if (f) {
    int nread = fscanf(f, "%zu %zu", &fields[0], &fields[1]);
    fclose(f);
    if (nread == MAX_FIELD) {
      *aN = fields[aField] * getpagesize();
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}

static nsresult
GetProcSelfSmapsPrivate(int64_t* aN)
{
  // You might be tempted to calculate USS by subtracting the "shared" value
  // from the "resident" value in /proc/<pid>/statm. But at least on Linux,
  // statm's "shared" value actually counts pages backed by files, which has
  // little to do with whether the pages are actually shared. /proc/self/smaps
  // on the other hand appears to give us the correct information.

  FILE* f = fopen("/proc/self/smaps", "r");
  if (NS_WARN_IF(!f)) {
    return NS_ERROR_UNEXPECTED;
  }

  // We carry over the end of the buffer to the beginning to make sure we only
  // interpret complete lines.
  static const uint32_t carryOver = 32;
  static const uint32_t readSize = 4096;

  int64_t amount = 0;
  char buffer[carryOver + readSize + 1];

  // Fill the beginning of the buffer with spaces, as a sentinel for the first
  // iteration.
  memset(buffer, ' ', carryOver);

  for (;;) {
    size_t bytes = fread(buffer + carryOver, sizeof(*buffer), readSize, f);
    char* end = buffer + bytes;
    char* ptr = buffer;
    end[carryOver] = '\0';
    // We are looking for lines like "Private_{Clean,Dirty}: 4 kB".
    while ((ptr = strstr(ptr, "Private"))) {
      if (ptr >= end) {
        break;
      }
      ptr += sizeof("Private_Xxxxx:");
      amount += strtol(ptr, nullptr, 10);
    }
    if (bytes < readSize) {
      // We do not expect any match within the end of the buffer.
      MOZ_ASSERT(!strstr(end, "Private"));
      break;
    }
    // Carry the end of the buffer over to the beginning.
    memcpy(buffer, end, carryOver);
  }

  fclose(f);
  // Convert from kB to bytes.
  *aN = amount * 1024;
  return NS_OK;
}

#define HAVE_VSIZE_AND_RESIDENT_REPORTERS 1
static nsresult
VsizeDistinguishedAmount(int64_t* aN)
{
  return GetProcSelfStatmField(0, aN);
}

static nsresult
ResidentDistinguishedAmount(int64_t* aN)
{
  return GetProcSelfStatmField(1, aN);
}

static nsresult
ResidentFastDistinguishedAmount(int64_t* aN)
{
  return ResidentDistinguishedAmount(aN);
}

#define HAVE_RESIDENT_UNIQUE_REPORTER 1
static nsresult
ResidentUniqueDistinguishedAmount(int64_t* aN)
{
  return GetProcSelfSmapsPrivate(aN);
}

class ResidentUniqueReporter final : public nsIMemoryReporter
{
  ~ResidentUniqueReporter() {}

public:
  NS_DECL_ISUPPORTS

  NS_METHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                           nsISupports* aData, bool aAnonymize) override
  {
    int64_t amount = 0;
    nsresult rv = ResidentUniqueDistinguishedAmount(&amount);
    NS_ENSURE_SUCCESS(rv, rv);

    return MOZ_COLLECT_REPORT(
      "resident-unique", KIND_OTHER, UNITS_BYTES, amount,
"Memory mapped by the process that is present in physical memory and not "
"shared with any other processes.  This is also known as the process's unique "
"set size (USS).  This is the amount of RAM we'd expect to be freed if we "
"closed this process.");
  }
};
NS_IMPL_ISUPPORTS(ResidentUniqueReporter, nsIMemoryReporter)

#define HAVE_SYSTEM_HEAP_REPORTER 1
nsresult
SystemHeapSize(int64_t* aSizeOut)
{
    struct mallinfo info = mallinfo();

    // The documentation in the glibc man page makes it sound like |uordblks|
    // would suffice, but that only gets the small allocations that are put in
    // the brk heap. We need |hblkhd| as well to get the larger allocations
    // that are mmapped.
    //
    // The fields in |struct mallinfo| are all |int|, <sigh>, so it is
    // unreliable if memory usage gets high. However, the system heap size on
    // Linux should usually be zero (so long as jemalloc is enabled) so that
    // shouldn't be a problem. Nonetheless, cast the |int|s to |size_t| before
    // adding them to provide a small amount of extra overflow protection.
    *aSizeOut = size_t(info.hblkhd) + size_t(info.uordblks);
    return NS_OK;
}

#elif defined(__DragonFly__) || defined(__FreeBSD__) \
    || defined(__NetBSD__) || defined(__OpenBSD__) \
    || defined(__FreeBSD_kernel__)

#include <sys/param.h>
#include <sys/sysctl.h>
#if defined(__DragonFly__) || defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
#include <sys/user.h>
#endif

#include <unistd.h>

#if defined(__NetBSD__)
#undef KERN_PROC
#define KERN_PROC KERN_PROC2
#define KINFO_PROC struct kinfo_proc2
#else
#define KINFO_PROC struct kinfo_proc
#endif

#if defined(__DragonFly__)
#define KP_SIZE(kp) (kp.kp_vm_map_size)
#define KP_RSS(kp) (kp.kp_vm_rssize * getpagesize())
#elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
#define KP_SIZE(kp) (kp.ki_size)
#define KP_RSS(kp) (kp.ki_rssize * getpagesize())
#elif defined(__NetBSD__)
#define KP_SIZE(kp) (kp.p_vm_msize * getpagesize())
#define KP_RSS(kp) (kp.p_vm_rssize * getpagesize())
#elif defined(__OpenBSD__)
#define KP_SIZE(kp) ((kp.p_vm_dsize + kp.p_vm_ssize                     \
                      + kp.p_vm_tsize) * getpagesize())
#define KP_RSS(kp) (kp.p_vm_rssize * getpagesize())
#endif

static nsresult
GetKinfoProcSelf(KINFO_PROC* aProc)
{
  int mib[] = {
    CTL_KERN,
    KERN_PROC,
    KERN_PROC_PID,
    getpid(),
#if defined(__NetBSD__) || defined(__OpenBSD__)
    sizeof(KINFO_PROC),
    1,
#endif
  };
  u_int miblen = sizeof(mib) / sizeof(mib[0]);
  size_t size = sizeof(KINFO_PROC);
  if (sysctl(mib, miblen, aProc, &size, nullptr, 0)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

#define HAVE_VSIZE_AND_RESIDENT_REPORTERS 1
static nsresult
VsizeDistinguishedAmount(int64_t* aN)
{
  KINFO_PROC proc;
  nsresult rv = GetKinfoProcSelf(&proc);
  if (NS_SUCCEEDED(rv)) {
    *aN = KP_SIZE(proc);
  }
  return rv;
}

static nsresult
ResidentDistinguishedAmount(int64_t* aN)
{
  KINFO_PROC proc;
  nsresult rv = GetKinfoProcSelf(&proc);
  if (NS_SUCCEEDED(rv)) {
    *aN = KP_RSS(proc);
  }
  return rv;
}

static nsresult
ResidentFastDistinguishedAmount(int64_t* aN)
{
  return ResidentDistinguishedAmount(aN);
}

#ifdef __FreeBSD__
#include <libutil.h>
#include <algorithm>

static nsresult
GetKinfoVmentrySelf(int64_t* aPrss, uint64_t* aMaxreg)
{
  int cnt;
  struct kinfo_vmentry* vmmap;
  struct kinfo_vmentry* kve;
  if (!(vmmap = kinfo_getvmmap(getpid(), &cnt))) {
    return NS_ERROR_FAILURE;
  }
  if (aPrss) {
    *aPrss = 0;
  }
  if (aMaxreg) {
    *aMaxreg = 0;
  }

  for (int i = 0; i < cnt; i++) {
    kve = &vmmap[i];
    if (aPrss) {
      *aPrss += kve->kve_private_resident;
    }
    if (aMaxreg) {
      *aMaxreg = std::max(*aMaxreg, kve->kve_end - kve->kve_start);
    }
  }

  free(vmmap);
  return NS_OK;
}

#define HAVE_PRIVATE_REPORTER 1
static nsresult
PrivateDistinguishedAmount(int64_t* aN)
{
  int64_t priv;
  nsresult rv = GetKinfoVmentrySelf(&priv, nullptr);
  NS_ENSURE_SUCCESS(rv, rv);
  *aN = priv * getpagesize();
  return NS_OK;
}

#define HAVE_VSIZE_MAX_CONTIGUOUS_REPORTER 1
static nsresult
VsizeMaxContiguousDistinguishedAmount(int64_t* aN)
{
  uint64_t biggestRegion;
  nsresult rv = GetKinfoVmentrySelf(nullptr, &biggestRegion);
  if (NS_SUCCEEDED(rv)) {
    *aN = biggestRegion;
  }
  return NS_OK;
}
#endif // FreeBSD

#elif defined(SOLARIS)

#include <procfs.h>
#include <fcntl.h>
#include <unistd.h>

static void XMappingIter(int64_t& aVsize, int64_t& aResident)
{
  aVsize = -1;
  aResident = -1;
  int mapfd = open("/proc/self/xmap", O_RDONLY);
  struct stat st;
  prxmap_t* prmapp = nullptr;
  if (mapfd >= 0) {
    if (!fstat(mapfd, &st)) {
      int nmap = st.st_size / sizeof(prxmap_t);
      while (1) {
        // stat(2) on /proc/<pid>/xmap returns an incorrect value,
        // prior to the release of Solaris 11.
        // Here is a workaround for it.
        nmap *= 2;
        prmapp = (prxmap_t*)malloc((nmap + 1) * sizeof(prxmap_t));
        if (!prmapp) {
          // out of memory
          break;
        }
        int n = pread(mapfd, prmapp, (nmap + 1) * sizeof(prxmap_t), 0);
        if (n < 0) {
          break;
        }
        if (nmap >= n / sizeof(prxmap_t)) {
          aVsize = 0;
          aResident = 0;
          for (int i = 0; i < n / sizeof(prxmap_t); i++) {
            aVsize += prmapp[i].pr_size;
            aResident += prmapp[i].pr_rss * prmapp[i].pr_pagesize;
          }
          break;
        }
        free(prmapp);
      }
      free(prmapp);
    }
    close(mapfd);
  }
}

#define HAVE_VSIZE_AND_RESIDENT_REPORTERS 1
static nsresult
VsizeDistinguishedAmount(int64_t* aN)
{
  int64_t vsize, resident;
  XMappingIter(vsize, resident);
  if (vsize == -1) {
    return NS_ERROR_FAILURE;
  }
  *aN = vsize;
  return NS_OK;
}

static nsresult
ResidentDistinguishedAmount(int64_t* aN)
{
  int64_t vsize, resident;
  XMappingIter(vsize, resident);
  if (resident == -1) {
    return NS_ERROR_FAILURE;
  }
  *aN = resident;
  return NS_OK;
}

static nsresult
ResidentFastDistinguishedAmount(int64_t* aN)
{
  return ResidentDistinguishedAmount(aN);
}

#elif defined(XP_MACOSX)

#include <mach/mach_init.h>
#include <mach/task.h>

static bool
GetTaskBasicInfo(struct task_basic_info* aTi)
{
  mach_msg_type_number_t count = TASK_BASIC_INFO_COUNT;
  kern_return_t kr = task_info(mach_task_self(), TASK_BASIC_INFO,
                               (task_info_t)aTi, &count);
  return kr == KERN_SUCCESS;
}

// The VSIZE figure on Mac includes huge amounts of shared memory and is always
// absurdly high, eg. 2GB+ even at start-up.  But both 'top' and 'ps' report
// it, so we might as well too.
#define HAVE_VSIZE_AND_RESIDENT_REPORTERS 1
static nsresult
VsizeDistinguishedAmount(int64_t* aN)
{
  task_basic_info ti;
  if (!GetTaskBasicInfo(&ti)) {
    return NS_ERROR_FAILURE;
  }
  *aN = ti.virtual_size;
  return NS_OK;
}

// If we're using jemalloc on Mac, we need to instruct jemalloc to purge the
// pages it has madvise(MADV_FREE)'d before we read our RSS in order to get
// an accurate result.  The OS will take away MADV_FREE'd pages when there's
// memory pressure, so ideally, they shouldn't count against our RSS.
//
// Purging these pages can take a long time for some users (see bug 789975),
// so we provide the option to get the RSS without purging first.
static nsresult
ResidentDistinguishedAmountHelper(int64_t* aN, bool aDoPurge)
{
#ifdef HAVE_JEMALLOC_STATS
#ifndef MOZ_JEMALLOC4
  if (aDoPurge) {
    Telemetry::AutoTimer<Telemetry::MEMORY_FREE_PURGED_PAGES_MS> timer;
    jemalloc_purge_freed_pages();
  }
#endif
#endif

  task_basic_info ti;
  if (!GetTaskBasicInfo(&ti)) {
    return NS_ERROR_FAILURE;
  }
  *aN = ti.resident_size;
  return NS_OK;
}

static nsresult
ResidentFastDistinguishedAmount(int64_t* aN)
{
  return ResidentDistinguishedAmountHelper(aN, /* doPurge = */ false);
}

static nsresult
ResidentDistinguishedAmount(int64_t* aN)
{
  return ResidentDistinguishedAmountHelper(aN, /* doPurge = */ true);
}

#elif defined(XP_WIN)

#include <windows.h>
#include <psapi.h>
#include <algorithm>

#define HAVE_VSIZE_AND_RESIDENT_REPORTERS 1
static nsresult
VsizeDistinguishedAmount(int64_t* aN)
{
  MEMORYSTATUSEX s;
  s.dwLength = sizeof(s);

  if (!GlobalMemoryStatusEx(&s)) {
    return NS_ERROR_FAILURE;
  }

  *aN = s.ullTotalVirtual - s.ullAvailVirtual;
  return NS_OK;
}

static nsresult
ResidentDistinguishedAmount(int64_t* aN)
{
  PROCESS_MEMORY_COUNTERS pmc;
  pmc.cb = sizeof(PROCESS_MEMORY_COUNTERS);

  if (!GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
    return NS_ERROR_FAILURE;
  }

  *aN = pmc.WorkingSetSize;
  return NS_OK;
}

static nsresult
ResidentFastDistinguishedAmount(int64_t* aN)
{
  return ResidentDistinguishedAmount(aN);
}

#define HAVE_VSIZE_MAX_CONTIGUOUS_REPORTER 1
static nsresult
VsizeMaxContiguousDistinguishedAmount(int64_t* aN)
{
  SIZE_T biggestRegion = 0;
  MEMORY_BASIC_INFORMATION vmemInfo = { 0 };
  for (size_t currentAddress = 0; ; ) {
    if (!VirtualQuery((LPCVOID)currentAddress, &vmemInfo, sizeof(vmemInfo))) {
      // Something went wrong, just return whatever we've got already.
      break;
    }

    if (vmemInfo.State == MEM_FREE) {
      biggestRegion = std::max(biggestRegion, vmemInfo.RegionSize);
    }

    SIZE_T lastAddress = currentAddress;
    currentAddress += vmemInfo.RegionSize;

    // If we overflow, we've examined all of the address space.
    if (currentAddress < lastAddress) {
      break;
    }
  }

  *aN = biggestRegion;
  return NS_OK;
}

#define HAVE_PRIVATE_REPORTER 1
static nsresult
PrivateDistinguishedAmount(int64_t* aN)
{
  PROCESS_MEMORY_COUNTERS_EX pmcex;
  pmcex.cb = sizeof(PROCESS_MEMORY_COUNTERS_EX);

  if (!GetProcessMemoryInfo(GetCurrentProcess(),
                            (PPROCESS_MEMORY_COUNTERS) &pmcex, sizeof(pmcex))) {
    return NS_ERROR_FAILURE;
  }

  *aN = pmcex.PrivateUsage;
  return NS_OK;
}

#define HAVE_SYSTEM_HEAP_REPORTER 1
// Windows can have multiple separate heaps. During testing there were multiple
// heaps present but the non-default ones had sizes no more than a few 10s of
// KiBs. So we combine their sizes into a single measurement.
nsresult
SystemHeapSize(int64_t* aSizeOut)
{
  // Get the number of heaps.
  DWORD nHeaps = GetProcessHeaps(0, nullptr);
  NS_ENSURE_TRUE(nHeaps != 0, NS_ERROR_FAILURE);

  // Get handles to all heaps, checking that the number of heaps hasn't
  // changed in the meantime.
  UniquePtr<HANDLE[]> heaps(new HANDLE[nHeaps]);
  DWORD nHeaps2 = GetProcessHeaps(nHeaps, heaps.get());
  NS_ENSURE_TRUE(nHeaps2 != 0 && nHeaps2 == nHeaps, NS_ERROR_FAILURE);

  // Lock and iterate over each heap to get its size.
  int64_t heapsSize = 0;
  for (DWORD i = 0; i < nHeaps; i++) {
    HANDLE heap = heaps[i];

    NS_ENSURE_TRUE(HeapLock(heap), NS_ERROR_FAILURE);

    int64_t heapSize = 0;
    PROCESS_HEAP_ENTRY entry;
    entry.lpData = nullptr;
    while (HeapWalk(heap, &entry)) {
      // We don't count entry.cbOverhead, because we just want to measure the
      // space available to the program.
      if (entry.wFlags & PROCESS_HEAP_ENTRY_BUSY) {
        heapSize += entry.cbData;
      }
    }

    // Check this result only after unlocking the heap, so that we don't leave
    // the heap locked if there was an error.
    DWORD lastError = GetLastError();

    // I have no idea how things would proceed if unlocking this heap failed...
    NS_ENSURE_TRUE(HeapUnlock(heap), NS_ERROR_FAILURE);

    NS_ENSURE_TRUE(lastError == ERROR_NO_MORE_ITEMS, NS_ERROR_FAILURE);

    heapsSize += heapSize;
  }

  *aSizeOut = heapsSize;
  return NS_OK;
}

class WindowsAddressSpaceReporter final : public nsIMemoryReporter
{
  ~WindowsAddressSpaceReporter() {}

public:
  NS_DECL_ISUPPORTS

  NS_METHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                           nsISupports* aData, bool aAnonymize) override
  {
    MEMORY_BASIC_INFORMATION info = { 0 };
    bool isPrevSegStackGuard = false;
    for (size_t currentAddress = 0; ; ) {
      if (!VirtualQuery((LPCVOID)currentAddress, &info, sizeof(info))) {
        // Something went wrong, just return whatever we've got already.
        break;
      }

      size_t size = info.RegionSize;

      // For each range of pages, we consider one or more of its State, Type
      // and Protect values. These are documented at
      // https://msdn.microsoft.com/en-us/library/windows/desktop/aa366775%28v=vs.85%29.aspx
      // (for State and Type) and
      // https://msdn.microsoft.com/en-us/library/windows/desktop/aa366786%28v=vs.85%29.aspx
      // (for Protect).
      //
      // Not all State values have accompanying Type and Protection values.
      bool doType = false;
      bool doProtect = false;

      nsCString path("address-space");

      switch (info.State) {
        case MEM_FREE:
          path.AppendLiteral("/free");
          break;

        case MEM_RESERVE:
          path.AppendLiteral("/reserved");
          doType = true;
          break;

        case MEM_COMMIT:
          path.AppendLiteral("/commit");
          doType = true;
          doProtect = true;
          break;

        default:
          // Should be impossible, but handle it just in case.
          path.AppendLiteral("/???");
          break;
      }

      if (doType) {
        switch (info.Type) {
          case MEM_IMAGE:
            path.AppendLiteral("/image");
            break;

          case MEM_MAPPED:
            path.AppendLiteral("/mapped");
            break;

          case MEM_PRIVATE:
            path.AppendLiteral("/private");
            break;

          default:
            // Should be impossible, but handle it just in case.
            path.AppendLiteral("/???");
            break;
        }
      }

      if (doProtect) {
        // Basic attributes. Exactly one of these should be set.
        if (info.Protect & PAGE_EXECUTE) {
          path.AppendLiteral("/execute");
        }
        if (info.Protect & PAGE_EXECUTE_READ) {
          path.AppendLiteral("/execute-read");
        }
        if (info.Protect & PAGE_EXECUTE_READWRITE) {
          path.AppendLiteral("/execute-readwrite");
        }
        if (info.Protect & PAGE_EXECUTE_WRITECOPY) {
          path.AppendLiteral("/execute-writecopy");
        }
        if (info.Protect & PAGE_NOACCESS) {
          path.AppendLiteral("/noaccess");
        }
        if (info.Protect & PAGE_READONLY) {
          path.AppendLiteral("/readonly");
        }
        if (info.Protect & PAGE_READWRITE) {
          path.AppendLiteral("/readwrite");
        }
        if (info.Protect & PAGE_WRITECOPY) {
          path.AppendLiteral("/writecopy");
        }

        // Modifiers. At most one of these should be set.
        if (info.Protect & PAGE_GUARD) {
          path.AppendLiteral("+guard");
        }
        if (info.Protect & PAGE_NOCACHE) {
          path.AppendLiteral("+nocache");
        }
        if (info.Protect & PAGE_WRITECOMBINE) {
          path.AppendLiteral("+writecombine");
        }

        // Annotate likely stack segments, too.
        if (isPrevSegStackGuard &&
            info.State == MEM_COMMIT &&
            doType && info.Type == MEM_PRIVATE &&
            doProtect && info.Protect == PAGE_READWRITE) {
          path.AppendLiteral(" (stack)");
        }
      }

      isPrevSegStackGuard =
        info.State == MEM_COMMIT &&
        doType && info.Type == MEM_PRIVATE &&
        doProtect && info.Protect == (PAGE_READWRITE|PAGE_GUARD);

      nsresult rv;
      rv = aHandleReport->Callback(
        EmptyCString(), path, KIND_OTHER, UNITS_BYTES, size,
        NS_LITERAL_CSTRING("From MEMORY_BASIC_INFORMATION."), aData);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      size_t lastAddress = currentAddress;
      currentAddress += size;

      // If we overflow, we've examined all of the address space.
      if (currentAddress < lastAddress) {
        break;
      }
    }

    return NS_OK;
  }
};
NS_IMPL_ISUPPORTS(WindowsAddressSpaceReporter, nsIMemoryReporter)

#endif  // XP_<PLATFORM>

#ifdef HAVE_VSIZE_MAX_CONTIGUOUS_REPORTER
class VsizeMaxContiguousReporter final : public nsIMemoryReporter
{
  ~VsizeMaxContiguousReporter() {}

public:
  NS_DECL_ISUPPORTS

  NS_METHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                           nsISupports* aData, bool aAnonymize) override
  {
    int64_t amount;
    nsresult rv = VsizeMaxContiguousDistinguishedAmount(&amount);
    NS_ENSURE_SUCCESS(rv, rv);
    return MOZ_COLLECT_REPORT(
      "vsize-max-contiguous", KIND_OTHER, UNITS_BYTES, amount,
      "Size of the maximum contiguous block of available virtual "
      "memory.");
  }
};
NS_IMPL_ISUPPORTS(VsizeMaxContiguousReporter, nsIMemoryReporter)
#endif

#ifdef HAVE_PRIVATE_REPORTER
class PrivateReporter final : public nsIMemoryReporter
{
  ~PrivateReporter() {}

public:
  NS_DECL_ISUPPORTS

  NS_METHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                           nsISupports* aData, bool aAnonymize) override
  {
    int64_t amount;
    nsresult rv = PrivateDistinguishedAmount(&amount);
    NS_ENSURE_SUCCESS(rv, rv);
    return MOZ_COLLECT_REPORT(
      "private", KIND_OTHER, UNITS_BYTES, amount,
"Memory that cannot be shared with other processes, including memory that is "
"committed and marked MEM_PRIVATE, data that is not mapped, and executable "
"pages that have been written to.");
  }
};
NS_IMPL_ISUPPORTS(PrivateReporter, nsIMemoryReporter)
#endif

#ifdef HAVE_VSIZE_AND_RESIDENT_REPORTERS
class VsizeReporter final : public nsIMemoryReporter
{
  ~VsizeReporter() {}

public:
  NS_DECL_ISUPPORTS

  NS_METHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                           nsISupports* aData, bool aAnonymize) override
  {
    int64_t amount;
    nsresult rv = VsizeDistinguishedAmount(&amount);
    NS_ENSURE_SUCCESS(rv, rv);

    return MOZ_COLLECT_REPORT(
      "vsize", KIND_OTHER, UNITS_BYTES, amount,
"Memory mapped by the process, including code and data segments, the heap, "
"thread stacks, memory explicitly mapped by the process via mmap and similar "
"operations, and memory shared with other processes. This is the vsize figure "
"as reported by 'top' and 'ps'.  This figure is of limited use on Mac, where "
"processes share huge amounts of memory with one another.  But even on other "
"operating systems, 'resident' is a much better measure of the memory "
"resources used by the process.");
  }
};
NS_IMPL_ISUPPORTS(VsizeReporter, nsIMemoryReporter)

class ResidentReporter final : public nsIMemoryReporter
{
  ~ResidentReporter() {}

public:
  NS_DECL_ISUPPORTS

  NS_METHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                           nsISupports* aData, bool aAnonymize) override
  {
    int64_t amount;
    nsresult rv = ResidentDistinguishedAmount(&amount);
    NS_ENSURE_SUCCESS(rv, rv);

    return MOZ_COLLECT_REPORT(
      "resident", KIND_OTHER, UNITS_BYTES, amount,
"Memory mapped by the process that is present in physical memory, also known "
"as the resident set size (RSS).  This is the best single figure to use when "
"considering the memory resources used by the process, but it depends both on "
"other processes being run and details of the OS kernel and so is best used "
"for comparing the memory usage of a single process at different points in "
"time.");
    }
};
NS_IMPL_ISUPPORTS(ResidentReporter, nsIMemoryReporter)

#endif  // HAVE_VSIZE_AND_RESIDENT_REPORTERS

#ifdef HAVE_SYSTEM_HEAP_REPORTER

class SystemHeapReporter final : public nsIMemoryReporter
{
  ~SystemHeapReporter() {}

public:
  NS_DECL_ISUPPORTS

  NS_METHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                           nsISupports* aData, bool aAnonymize) override
  {
    int64_t amount;
    nsresult rv = SystemHeapSize(&amount);
    NS_ENSURE_SUCCESS(rv, rv);

    return MOZ_COLLECT_REPORT(
      "system-heap-allocated", KIND_OTHER, UNITS_BYTES, amount,
"Memory used by the system allocator that is currently allocated to the "
"application. This is distinct from the jemalloc heap that Firefox uses for "
"most or all of its heap allocations. Ideally this number is zero, but "
"on some platforms we cannot force every heap allocation through jemalloc.");
  }
};
NS_IMPL_ISUPPORTS(SystemHeapReporter, nsIMemoryReporter)

#endif // HAVE_SYSTEM_HEAP_REPORTER

#ifdef XP_UNIX

#include <sys/resource.h>

#define HAVE_RESIDENT_PEAK_REPORTER 1

static nsresult
ResidentPeakDistinguishedAmount(int64_t* aN)
{
  struct rusage usage;
  if (0 == getrusage(RUSAGE_SELF, &usage)) {
    // The units for ru_maxrrs:
    // - Mac: bytes
    // - Solaris: pages? But some sources it actually always returns 0, so
    //   check for that
    // - Linux, {Net/Open/Free}BSD, DragonFly: KiB
#ifdef XP_MACOSX
    *aN = usage.ru_maxrss;
#elif defined(SOLARIS)
    *aN = usage.ru_maxrss * getpagesize();
#else
    *aN = usage.ru_maxrss * 1024;
#endif
    if (*aN > 0) {
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}

class ResidentPeakReporter final : public nsIMemoryReporter
{
  ~ResidentPeakReporter() {}

public:
  NS_DECL_ISUPPORTS

  NS_METHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                           nsISupports* aData, bool aAnonymize) override
  {
    int64_t amount = 0;
    nsresult rv = ResidentPeakDistinguishedAmount(&amount);
    NS_ENSURE_SUCCESS(rv, rv);

    return MOZ_COLLECT_REPORT(
      "resident-peak", KIND_OTHER, UNITS_BYTES, amount,
"The peak 'resident' value for the lifetime of the process.");
  }
};
NS_IMPL_ISUPPORTS(ResidentPeakReporter, nsIMemoryReporter)

#define HAVE_PAGE_FAULT_REPORTERS 1

class PageFaultsSoftReporter final : public nsIMemoryReporter
{
  ~PageFaultsSoftReporter() {}

public:
  NS_DECL_ISUPPORTS

  NS_METHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                           nsISupports* aData, bool aAnonymize) override
  {
    struct rusage usage;
    int err = getrusage(RUSAGE_SELF, &usage);
    if (err != 0) {
      return NS_ERROR_FAILURE;
    }
    int64_t amount = usage.ru_minflt;

    return MOZ_COLLECT_REPORT(
      "page-faults-soft", KIND_OTHER, UNITS_COUNT_CUMULATIVE, amount,
"The number of soft page faults (also known as 'minor page faults') that "
"have occurred since the process started.  A soft page fault occurs when the "
"process tries to access a page which is present in physical memory but is "
"not mapped into the process's address space.  For instance, a process might "
"observe soft page faults when it loads a shared library which is already "
"present in physical memory. A process may experience many thousands of soft "
"page faults even when the machine has plenty of available physical memory, "
"and because the OS services a soft page fault without accessing the disk, "
"they impact performance much less than hard page faults.");
  }
};
NS_IMPL_ISUPPORTS(PageFaultsSoftReporter, nsIMemoryReporter)

static nsresult
PageFaultsHardDistinguishedAmount(int64_t* aAmount)
{
  struct rusage usage;
  int err = getrusage(RUSAGE_SELF, &usage);
  if (err != 0) {
    return NS_ERROR_FAILURE;
  }
  *aAmount = usage.ru_majflt;
  return NS_OK;
}

class PageFaultsHardReporter final : public nsIMemoryReporter
{
  ~PageFaultsHardReporter() {}

public:
  NS_DECL_ISUPPORTS

  NS_METHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                           nsISupports* aData, bool aAnonymize) override
  {
    int64_t amount = 0;
    nsresult rv = PageFaultsHardDistinguishedAmount(&amount);
    NS_ENSURE_SUCCESS(rv, rv);

    return MOZ_COLLECT_REPORT(
      "page-faults-hard", KIND_OTHER, UNITS_COUNT_CUMULATIVE, amount,
"The number of hard page faults (also known as 'major page faults') that have "
"occurred since the process started.  A hard page fault occurs when a process "
"tries to access a page which is not present in physical memory. The "
"operating system must access the disk in order to fulfill a hard page fault. "
"When memory is plentiful, you should see very few hard page faults. But if "
"the process tries to use more memory than your machine has available, you "
"may see many thousands of hard page faults. Because accessing the disk is up "
"to a million times slower than accessing RAM, the program may run very "
"slowly when it is experiencing more than 100 or so hard page faults a "
"second.");
  }
};
NS_IMPL_ISUPPORTS(PageFaultsHardReporter, nsIMemoryReporter)

#endif  // XP_UNIX

/**
 ** memory reporter implementation for jemalloc and OSX malloc,
 ** to obtain info on total memory in use (that we know about,
 ** at least -- on OSX, there are sometimes other zones in use).
 **/

#ifdef HAVE_JEMALLOC_STATS

// This has UNITS_PERCENTAGE, so it is multiplied by 100.
static int64_t
HeapOverheadRatio(jemalloc_stats_t* aStats)
{
  return (int64_t)10000 *
    (aStats->waste + aStats->bookkeeping + aStats->page_cache) /
    ((double)aStats->allocated);
}

class JemallocHeapReporter final : public nsIMemoryReporter
{
  ~JemallocHeapReporter() {}

public:
  NS_DECL_ISUPPORTS

  NS_METHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                           nsISupports* aData, bool aAnonymize) override
  {
    jemalloc_stats_t stats;
    jemalloc_stats(&stats);

    nsresult rv;

    rv = MOZ_COLLECT_REPORT(
      "heap-allocated", KIND_OTHER, UNITS_BYTES, stats.allocated,
"Memory mapped by the heap allocator that is currently allocated to the "
"application.  This may exceed the amount of memory requested by the "
"application because the allocator regularly rounds up request sizes. (The "
"exact amount requested is not recorded.)");
    NS_ENSURE_SUCCESS(rv, rv);

    // We mark this and the other heap-overhead reporters as KIND_NONHEAP
    // because KIND_HEAP memory means "counted in heap-allocated", which
    // this is not.
    rv = MOZ_COLLECT_REPORT(
      "explicit/heap-overhead/bin-unused", KIND_NONHEAP, UNITS_BYTES,
      stats.bin_unused,
"Bytes reserved for bins of fixed-size allocations which do not correspond to "
"an active allocation.");
    NS_ENSURE_SUCCESS(rv, rv);

    rv = MOZ_COLLECT_REPORT(
      "explicit/heap-overhead/waste", KIND_NONHEAP, UNITS_BYTES,
      stats.waste,
"Committed bytes which do not correspond to an active allocation and which the "
"allocator is not intentionally keeping alive (i.e., not 'heap-bookkeeping' or "
"'heap-page-cache' or 'heap-bin-unused').  Although the allocator will waste "
"some space under any circumstances, a large value here may indicate that the "
"heap is highly fragmented, or that allocator is performing poorly for some "
"other reason.");
    NS_ENSURE_SUCCESS(rv, rv);

    rv = MOZ_COLLECT_REPORT(
      "explicit/heap-overhead/bookkeeping", KIND_NONHEAP, UNITS_BYTES,
      stats.bookkeeping,
"Committed bytes which the heap allocator uses for internal data structures.");
    NS_ENSURE_SUCCESS(rv, rv);

    rv = MOZ_COLLECT_REPORT(
      "explicit/heap-overhead/page-cache", KIND_NONHEAP, UNITS_BYTES,
      stats.page_cache,
"Memory which the allocator could return to the operating system, but hasn't. "
"The allocator keeps this memory around as an optimization, so it doesn't "
"have to ask the OS the next time it needs to fulfill a request. This value "
"is typically not larger than a few megabytes.");
    NS_ENSURE_SUCCESS(rv, rv);

    rv = MOZ_COLLECT_REPORT(
      "heap-committed", KIND_OTHER, UNITS_BYTES,
      stats.allocated + stats.waste + stats.bookkeeping + stats.page_cache,
"Memory mapped by the heap allocator that is committed, i.e. in physical "
"memory or paged to disk.  This value corresponds to 'heap-allocated' + "
"'heap-waste' + 'heap-bookkeeping' + 'heap-page-cache', but because "
"these values are read at different times, the result probably won't match "
"exactly.");
    NS_ENSURE_SUCCESS(rv, rv);

    rv = MOZ_COLLECT_REPORT(
      "heap-overhead-ratio", KIND_OTHER, UNITS_PERCENTAGE,
      HeapOverheadRatio(&stats),
"Ratio of committed, unused bytes to allocated bytes; i.e., "
"'heap-overhead' / 'heap-allocated'.  This measures the overhead of "
"the heap allocator relative to amount of memory allocated.");
    NS_ENSURE_SUCCESS(rv, rv);

    rv = MOZ_COLLECT_REPORT(
      "heap-mapped", KIND_OTHER, UNITS_BYTES, stats.mapped,
      "Amount of memory currently mapped.");
    NS_ENSURE_SUCCESS(rv, rv);

    rv = MOZ_COLLECT_REPORT(
      "heap-chunksize", KIND_OTHER, UNITS_BYTES, stats.chunksize,
      "Size of chunks.");
    NS_ENSURE_SUCCESS(rv, rv);

    rv = MOZ_COLLECT_REPORT(
      "heap-chunks", KIND_OTHER, UNITS_COUNT, (stats.mapped / stats.chunksize),
      "Number of chunks currently mapped.");
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }
};
NS_IMPL_ISUPPORTS(JemallocHeapReporter, nsIMemoryReporter)

#endif  // HAVE_JEMALLOC_STATS

// Why is this here?  At first glance, you'd think it could be defined and
// registered with nsMemoryReporterManager entirely within nsAtomTable.cpp.
// However, the obvious time to register it is when the table is initialized,
// and that happens before XPCOM components are initialized, which means the
// RegisterStrongMemoryReporter call fails.  So instead we do it here.
class AtomTablesReporter final : public nsIMemoryReporter
{
  MOZ_DEFINE_MALLOC_SIZE_OF(MallocSizeOf)

  ~AtomTablesReporter() {}

public:
  NS_DECL_ISUPPORTS

  NS_METHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                           nsISupports* aData, bool aAnonymize) override
  {
    size_t Main, Static;
    NS_SizeOfAtomTablesIncludingThis(MallocSizeOf, &Main, &Static);

    nsresult rv;
    rv = MOZ_COLLECT_REPORT(
      "explicit/atom-tables/main", KIND_HEAP, UNITS_BYTES, Main,
      "Memory used by the main atoms table.");
    NS_ENSURE_SUCCESS(rv, rv);

    rv = MOZ_COLLECT_REPORT(
      "explicit/atom-tables/static", KIND_HEAP, UNITS_BYTES, Static,
      "Memory used by the static atoms table.");
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }
};
NS_IMPL_ISUPPORTS(AtomTablesReporter, nsIMemoryReporter)

#ifdef DEBUG

// Ideally, this would be implemented in BlockingResourceBase.cpp.
// However, this ends up breaking the linking step of various unit tests due
// to adding a new dependency to libdmd for a commonly used feature (mutexes)
// in  DMD  builds. So instead we do it here.
class DeadlockDetectorReporter final : public nsIMemoryReporter
{
  MOZ_DEFINE_MALLOC_SIZE_OF(MallocSizeOf)

  ~DeadlockDetectorReporter() {}

public:
  NS_DECL_ISUPPORTS

  NS_METHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                           nsISupports* aData, bool aAnonymize) override
  {
    return MOZ_COLLECT_REPORT(
      "explicit/deadlock-detector", KIND_HEAP, UNITS_BYTES,
      BlockingResourceBase::SizeOfDeadlockDetector(MallocSizeOf),
      "Memory used by the deadlock detector.");
  }
};
NS_IMPL_ISUPPORTS(DeadlockDetectorReporter, nsIMemoryReporter)

#endif

#ifdef MOZ_DMD

namespace mozilla {
namespace dmd {

class DMDReporter final : public nsIMemoryReporter
{
public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                            nsISupports* aData, bool aAnonymize) override
  {
    dmd::Sizes sizes;
    dmd::SizeOf(&sizes);

#define REPORT(_path, _amount, _desc)                                         \
  do {                                                                        \
    nsresult rv;                                                              \
    rv = aHandleReport->Callback(EmptyCString(), NS_LITERAL_CSTRING(_path),   \
                                 KIND_HEAP, UNITS_BYTES, _amount,             \
                                 NS_LITERAL_CSTRING(_desc), aData);           \
    if (NS_WARN_IF(NS_FAILED(rv))) {                                          \
      return rv;                                                              \
    }                                                                         \
  } while (0)

    REPORT("explicit/dmd/stack-traces/used",
           sizes.mStackTracesUsed,
           "Memory used by stack traces which correspond to at least "
           "one heap block DMD is tracking.");

    REPORT("explicit/dmd/stack-traces/unused",
           sizes.mStackTracesUnused,
           "Memory used by stack traces which don't correspond to any heap "
           "blocks DMD is currently tracking.");

    REPORT("explicit/dmd/stack-traces/table",
           sizes.mStackTraceTable,
           "Memory used by DMD's stack trace table.");

    REPORT("explicit/dmd/live-block-table",
           sizes.mLiveBlockTable,
           "Memory used by DMD's live block table.");

    REPORT("explicit/dmd/dead-block-list",
           sizes.mDeadBlockTable,
           "Memory used by DMD's dead block list.");

#undef REPORT

    return NS_OK;
  }

private:
  ~DMDReporter() {}
};
NS_IMPL_ISUPPORTS(DMDReporter, nsIMemoryReporter)

} // namespace dmd
} // namespace mozilla

#endif  // MOZ_DMD

/**
 ** nsMemoryReporterManager implementation
 **/

NS_IMPL_ISUPPORTS(nsMemoryReporterManager, nsIMemoryReporterManager)

NS_IMETHODIMP
nsMemoryReporterManager::Init()
{
#if defined(HAVE_JEMALLOC_STATS) && defined(MOZ_GLUE_IN_PROGRAM)
  if (!jemalloc_stats) {
    return NS_ERROR_FAILURE;
  }
#endif

#ifdef HAVE_JEMALLOC_STATS
  RegisterStrongReporter(new JemallocHeapReporter());
#endif

#ifdef HAVE_VSIZE_AND_RESIDENT_REPORTERS
  RegisterStrongReporter(new VsizeReporter());
  RegisterStrongReporter(new ResidentReporter());
#endif

#ifdef HAVE_VSIZE_MAX_CONTIGUOUS_REPORTER
  RegisterStrongReporter(new VsizeMaxContiguousReporter());
#endif

#ifdef HAVE_RESIDENT_PEAK_REPORTER
  RegisterStrongReporter(new ResidentPeakReporter());
#endif

#ifdef HAVE_RESIDENT_UNIQUE_REPORTER
  RegisterStrongReporter(new ResidentUniqueReporter());
#endif

#ifdef HAVE_PAGE_FAULT_REPORTERS
  RegisterStrongReporter(new PageFaultsSoftReporter());
  RegisterStrongReporter(new PageFaultsHardReporter());
#endif

#ifdef HAVE_PRIVATE_REPORTER
  RegisterStrongReporter(new PrivateReporter());
#endif

#ifdef HAVE_SYSTEM_HEAP_REPORTER
  RegisterStrongReporter(new SystemHeapReporter());
#endif

  RegisterStrongReporter(new AtomTablesReporter());

#ifdef DEBUG
  RegisterStrongReporter(new DeadlockDetectorReporter());
#endif

#ifdef MOZ_DMD
  RegisterStrongReporter(new mozilla::dmd::DMDReporter());
#endif

#ifdef XP_WIN
  RegisterStrongReporter(new WindowsAddressSpaceReporter());
#endif

#ifdef XP_UNIX
  nsMemoryInfoDumper::Initialize();
#endif

  return NS_OK;
}

nsMemoryReporterManager::nsMemoryReporterManager()
  : mMutex("nsMemoryReporterManager::mMutex")
  , mIsRegistrationBlocked(false)
  , mStrongReporters(new StrongReportersTable())
  , mWeakReporters(new WeakReportersTable())
  , mSavedStrongReporters(nullptr)
  , mSavedWeakReporters(nullptr)
  , mNextGeneration(1)
  , mGetReportsState(nullptr)
{
}

nsMemoryReporterManager::~nsMemoryReporterManager()
{
  delete mStrongReporters;
  delete mWeakReporters;
  NS_ASSERTION(!mSavedStrongReporters, "failed to restore strong reporters");
  NS_ASSERTION(!mSavedWeakReporters, "failed to restore weak reporters");
}

#ifdef MOZ_WIDGET_GONK
#define DEBUG_CHILD_PROCESS_MEMORY_REPORTING 1
#endif

#ifdef DEBUG_CHILD_PROCESS_MEMORY_REPORTING
#define MEMORY_REPORTING_LOG(format, ...) \
  printf_stderr("++++ MEMORY REPORTING: " format, ##__VA_ARGS__);
#else
#define MEMORY_REPORTING_LOG(...)
#endif

NS_IMETHODIMP
nsMemoryReporterManager::GetReports(
  nsIHandleReportCallback* aHandleReport,
  nsISupports* aHandleReportData,
  nsIFinishReportingCallback* aFinishReporting,
  nsISupports* aFinishReportingData,
  bool aAnonymize)
{
  return GetReportsExtended(aHandleReport, aHandleReportData,
                            aFinishReporting, aFinishReportingData,
                            aAnonymize,
                            /* minimize = */ false,
                            /* DMDident = */ EmptyString());
}

NS_IMETHODIMP
nsMemoryReporterManager::GetReportsExtended(
  nsIHandleReportCallback* aHandleReport,
  nsISupports* aHandleReportData,
  nsIFinishReportingCallback* aFinishReporting,
  nsISupports* aFinishReportingData,
  bool aAnonymize,
  bool aMinimize,
  const nsAString& aDMDDumpIdent)
{
  nsresult rv;

  // Memory reporters are not necessarily threadsafe, so this function must
  // be called from the main thread.
  if (!NS_IsMainThread()) {
    MOZ_CRASH();
  }

  uint32_t generation = mNextGeneration++;

  if (mGetReportsState) {
    // A request is in flight.  Don't start another one.  And don't report
    // an error;  just ignore it, and let the in-flight request finish.
    MEMORY_REPORTING_LOG("GetReports (gen=%u, s->gen=%u): abort\n",
                         generation, mGetReportsState->mGeneration);
    return NS_OK;
  }

  MEMORY_REPORTING_LOG("GetReports (gen=%u)\n", generation);

  uint32_t concurrency = Preferences::GetUint("memory.report_concurrency", 1);
  MOZ_ASSERT(concurrency >= 1);
  if (concurrency < 1) {
    concurrency = 1;
  }
  mGetReportsState = new GetReportsState(generation,
                                         aAnonymize,
                                         aMinimize,
                                         concurrency,
                                         aHandleReport,
                                         aHandleReportData,
                                         aFinishReporting,
                                         aFinishReportingData,
                                         aDMDDumpIdent);

  if (aMinimize) {
    rv = MinimizeMemoryUsage(NS_NewRunnableMethod(
      this, &nsMemoryReporterManager::StartGettingReports));
  } else {
    rv = StartGettingReports();
  }
  return rv;
}

nsresult
nsMemoryReporterManager::StartGettingReports()
{
  GetReportsState* s = mGetReportsState;
  nsresult rv;

  // Get reports for this process.
  FILE* parentDMDFile = nullptr;
#ifdef MOZ_DMD
  if (!s->mDMDDumpIdent.IsEmpty()) {
    rv = nsMemoryInfoDumper::OpenDMDFile(s->mDMDDumpIdent, getpid(),
                                                  &parentDMDFile);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      // Proceed with the memory report as if DMD were disabled.
      parentDMDFile = nullptr;
    }
  }
#endif
  GetReportsForThisProcessExtended(s->mHandleReport, s->mHandleReportData,
                                   s->mAnonymize, parentDMDFile);

  nsTArray<ContentParent*> childWeakRefs;
  ContentParent::GetAll(childWeakRefs);
  if (!childWeakRefs.IsEmpty()) {
    // Request memory reports from child processes.  This happens
    // after the parent report so that the parent's main thread will
    // be free to process the child reports, instead of causing them
    // to be buffered and consume (possibly scarce) memory.

    for (size_t i = 0; i < childWeakRefs.Length(); ++i) {
      s->mChildrenPending.AppendElement(childWeakRefs[i]);
    }

    nsCOMPtr<nsITimer> timer = do_CreateInstance(NS_TIMER_CONTRACTID);
    // Don't use NS_ENSURE_* here; can't return until the report is finished.
    if (NS_WARN_IF(!timer)) {
      FinishReporting();
      return NS_ERROR_FAILURE;
    }
    rv = timer->InitWithFuncCallback(TimeoutCallback,
                                     this, kTimeoutLengthMS,
                                     nsITimer::TYPE_ONE_SHOT);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      FinishReporting();
      return rv;
    }

    MOZ_ASSERT(!s->mTimer);
    s->mTimer.swap(timer);
  }

  // The parent's report is done; make note of that, and start
  // launching child process reports (if any).
  EndProcessReport(s->mGeneration, true);
  return NS_OK;
}

NS_IMETHODIMP
nsMemoryReporterManager::GetReportsForThisProcess(
  nsIHandleReportCallback* aHandleReport,
  nsISupports* aHandleReportData, bool aAnonymize)
{
  return GetReportsForThisProcessExtended(aHandleReport, aHandleReportData,
                                          aAnonymize, nullptr);
}

NS_IMETHODIMP
nsMemoryReporterManager::GetReportsForThisProcessExtended(
  nsIHandleReportCallback* aHandleReport, nsISupports* aHandleReportData,
  bool aAnonymize, FILE* aDMDFile)
{
  // Memory reporters are not necessarily threadsafe, so this function must
  // be called from the main thread.
  if (!NS_IsMainThread()) {
    MOZ_CRASH();
  }

#ifdef MOZ_DMD
  if (aDMDFile) {
    // Clear DMD's reportedness state before running the memory
    // reporters, to avoid spurious twice-reported warnings.
    dmd::ClearReports();
  }
#else
  MOZ_ASSERT(!aDMDFile);
#endif

  nsCOMArray<nsIMemoryReporter> allReporters;
  {
    mozilla::MutexAutoLock autoLock(mMutex);
    for (auto iter = mStrongReporters->Iter(); !iter.Done(); iter.Next()) {
      nsRefPtrHashKey<nsIMemoryReporter>* entry = iter.Get();
      allReporters.AppendElement(entry->GetKey());
    }
    for (auto iter = mWeakReporters->Iter(); !iter.Done(); iter.Next()) {
      nsPtrHashKey<nsIMemoryReporter>* entry = iter.Get();
      allReporters.AppendElement(entry->GetKey());
    }
  }
  for (uint32_t i = 0; i < allReporters.Length(); i++) {
    allReporters[i]->CollectReports(aHandleReport, aHandleReportData,
                                    aAnonymize);
  }

#ifdef MOZ_DMD
  if (aDMDFile) {
    return nsMemoryInfoDumper::DumpDMDToFile(aDMDFile);
  }
#endif

  return NS_OK;
}

nsMemoryReporterManager::GetReportsState*
nsMemoryReporterManager::GetStateForGeneration(uint32_t aGeneration)
{
  // Memory reporting only happens on the main thread.
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  GetReportsState* s = mGetReportsState;

  if (!s) {
    // If we reach here, then:
    //
    // - A child process reported back too late, and no subsequent request
    //   is in flight.
    //
    // So there's nothing to be done.  Just ignore it.
    MEMORY_REPORTING_LOG(
      "HandleChildReports: no request in flight (aGen=%u)\n",
      aGeneration);
    return nullptr;
  }

  if (aGeneration != s->mGeneration) {
    // If we reach here, a child process must have reported back, too late,
    // while a subsequent (higher-numbered) request is in flight.  Again,
    // ignore it.
    MOZ_ASSERT(aGeneration < s->mGeneration);
    MEMORY_REPORTING_LOG(
      "HandleChildReports: gen mismatch (aGen=%u, s->gen=%u)\n",
      aGeneration, s->mGeneration);
    return nullptr;
  }

  return s;
}

// This function has no return value.  If something goes wrong, there's no
// clear place to report the problem to, but that's ok -- we will end up
// hitting the timeout and executing TimeoutCallback().
void
nsMemoryReporterManager::HandleChildReport(
  uint32_t aGeneration,
  const dom::MemoryReport& aChildReport)
{
  GetReportsState* s = GetStateForGeneration(aGeneration);
  if (!s) {
    return;
  }

  // Child reports should have a non-empty process.
  MOZ_ASSERT(!aChildReport.process().IsEmpty());

  // If the call fails, ignore and continue.
  s->mHandleReport->Callback(aChildReport.process(),
                             aChildReport.path(),
                             aChildReport.kind(),
                             aChildReport.units(),
                             aChildReport.amount(),
                             aChildReport.desc(),
                             s->mHandleReportData);
}

/* static */ bool
nsMemoryReporterManager::StartChildReport(mozilla::dom::ContentParent* aChild,
                                          const GetReportsState* aState)
{
#ifdef MOZ_NUWA_PROCESS
  if (aChild->IsNuwaProcess()) {
    return false;
  }
#endif

  if (!aChild->IsAlive()) {
    MEMORY_REPORTING_LOG("StartChildReports (gen=%u): child exited before"
                         " its report was started\n",
                         aState->mGeneration);
    return false;
  }

  mozilla::dom::MaybeFileDesc dmdFileDesc = void_t();
#ifdef MOZ_DMD
  if (!aState->mDMDDumpIdent.IsEmpty()) {
    FILE *dmdFile = nullptr;
    nsresult rv = nsMemoryInfoDumper::OpenDMDFile(aState->mDMDDumpIdent,
                                                  aChild->Pid(), &dmdFile);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      // Proceed with the memory report as if DMD were disabled.
      dmdFile = nullptr;
    }
    if (dmdFile) {
      dmdFileDesc = mozilla::ipc::FILEToFileDescriptor(dmdFile);
      fclose(dmdFile);
    }
  }
#endif
  return aChild->SendPMemoryReportRequestConstructor(
    aState->mGeneration, aState->mAnonymize, aState->mMinimize, dmdFileDesc);
}

void
nsMemoryReporterManager::EndProcessReport(uint32_t aGeneration, bool aSuccess)
{
  GetReportsState* s = GetStateForGeneration(aGeneration);
  if (!s) {
    return;
  }

  MOZ_ASSERT(s->mNumProcessesRunning > 0);
  s->mNumProcessesRunning--;
  s->mNumProcessesCompleted++;
  MEMORY_REPORTING_LOG("HandleChildReports (aGen=%u): process %u %s"
                       " (%u running, %u pending)\n",
                       aGeneration, s->mNumProcessesCompleted,
                       aSuccess ? "completed" : "exited during report",
                       s->mNumProcessesRunning,
                       static_cast<unsigned>(s->mChildrenPending.Length()));

  // Start pending children up to the concurrency limit.
  while (s->mNumProcessesRunning < s->mConcurrencyLimit &&
         !s->mChildrenPending.IsEmpty()) {
    // Pop last element from s->mChildrenPending
    nsRefPtr<ContentParent> nextChild;
    nextChild.swap(s->mChildrenPending.LastElement());
    s->mChildrenPending.TruncateLength(s->mChildrenPending.Length() - 1);
    // Start report (if the child is still alive and not Nuwa).
    if (StartChildReport(nextChild, s)) {
      ++s->mNumProcessesRunning;
      MEMORY_REPORTING_LOG("HandleChildReports (aGen=%u): started child report"
                           " (%u running, %u pending)\n",
                           aGeneration, s->mNumProcessesRunning,
                           static_cast<unsigned>(s->mChildrenPending.Length()));
    }
  }

  // If all the child processes (if any) have reported, we can cancel
  // the timer (if started) and finish up.  Otherwise, just return.
  if (s->mNumProcessesRunning == 0) {
    MOZ_ASSERT(s->mChildrenPending.IsEmpty());
    if (s->mTimer) {
      s->mTimer->Cancel();
    }
    FinishReporting();
  }
}

/* static */ void
nsMemoryReporterManager::TimeoutCallback(nsITimer* aTimer, void* aData)
{
  nsMemoryReporterManager* mgr = static_cast<nsMemoryReporterManager*>(aData);
  GetReportsState* s = mgr->mGetReportsState;

  // Release assert because: if the pointer is null we're about to
  // crash regardless of DEBUG, and this way the compiler doesn't
  // complain about unused variables.
  MOZ_RELEASE_ASSERT(s, "mgr->mGetReportsState");
  MEMORY_REPORTING_LOG("TimeoutCallback (s->gen=%u; %u running, %u pending)\n",
                       s->mGeneration, s->mNumProcessesRunning,
                       static_cast<unsigned>(s->mChildrenPending.Length()));

  // We don't bother sending any kind of cancellation message to the child
  // processes that haven't reported back.
  mgr->FinishReporting();
}

nsresult
nsMemoryReporterManager::FinishReporting()
{
  // Memory reporting only happens on the main thread.
  if (!NS_IsMainThread()) {
    MOZ_CRASH();
  }

  MOZ_ASSERT(mGetReportsState);
  MEMORY_REPORTING_LOG("FinishReporting (s->gen=%u; %u processes reported)\n",
                       mGetReportsState->mGeneration,
                       mGetReportsState->mNumProcessesCompleted);

  // Call this before deleting |mGetReportsState|.  That way, if
  // |mFinishReportData| calls GetReports(), it will silently abort, as
  // required.
  nsresult rv = mGetReportsState->mFinishReporting->Callback(
    mGetReportsState->mFinishReportingData);

  delete mGetReportsState;
  mGetReportsState = nullptr;
  return rv;
}

nsMemoryReporterManager::GetReportsState::GetReportsState(
    uint32_t aGeneration, bool aAnonymize, bool aMinimize,
    uint32_t aConcurrencyLimit,
    nsIHandleReportCallback* aHandleReport,
    nsISupports* aHandleReportData,
    nsIFinishReportingCallback* aFinishReporting,
    nsISupports* aFinishReportingData,
    const nsAString& aDMDDumpIdent)
  : mGeneration(aGeneration)
  , mAnonymize(aAnonymize)
  , mMinimize(aMinimize)
  , mChildrenPending()
  , mNumProcessesRunning(1) // reporting starts with the parent
  , mNumProcessesCompleted(0)
  , mConcurrencyLimit(aConcurrencyLimit)
  , mHandleReport(aHandleReport)
  , mHandleReportData(aHandleReportData)
  , mFinishReporting(aFinishReporting)
  , mFinishReportingData(aFinishReportingData)
  , mDMDDumpIdent(aDMDDumpIdent)
{
}

static void
CrashIfRefcountIsZero(nsISupports* aObj)
{
  // This will probably crash if the object's refcount is 0.
  uint32_t refcnt = NS_ADDREF(aObj);
  if (refcnt <= 1) {
    MOZ_CRASH("CrashIfRefcountIsZero: refcount is zero");
  }
  NS_RELEASE(aObj);
}

nsresult
nsMemoryReporterManager::RegisterReporterHelper(
  nsIMemoryReporter* aReporter, bool aForce, bool aStrong)
{
  // This method is thread-safe.
  mozilla::MutexAutoLock autoLock(mMutex);

  if (mIsRegistrationBlocked && !aForce) {
    return NS_ERROR_FAILURE;
  }

  if (mStrongReporters->Contains(aReporter) ||
      mWeakReporters->Contains(aReporter)) {
    return NS_ERROR_FAILURE;
  }

  // If |aStrong| is true, |aReporter| may have a refcnt of 0, so we take
  // a kung fu death grip before calling PutEntry.  Otherwise, if PutEntry
  // addref'ed and released |aReporter| before finally addref'ing it for
  // good, it would free aReporter!  The kung fu death grip could itself be
  // problematic if PutEntry didn't addref |aReporter| (because then when the
  // death grip goes out of scope, we would delete the reporter).  In debug
  // mode, we check that this doesn't happen.
  //
  // If |aStrong| is false, we require that |aReporter| have a non-zero
  // refcnt.
  //
  if (aStrong) {
    nsCOMPtr<nsIMemoryReporter> kungFuDeathGrip = aReporter;
    mStrongReporters->PutEntry(aReporter);
    CrashIfRefcountIsZero(aReporter);
  } else {
    CrashIfRefcountIsZero(aReporter);
    nsCOMPtr<nsIXPConnectWrappedJS> jsComponent = do_QueryInterface(aReporter);
    if (jsComponent) {
      // We cannot allow non-native reporters (WrappedJS), since we'll be
      // holding onto a raw pointer, which would point to the wrapper,
      // and that wrapper is likely to go away as soon as this register
      // call finishes.  This would then lead to subsequent crashes in
      // CollectReports().
      return NS_ERROR_XPC_BAD_CONVERT_JS;
    }
    mWeakReporters->PutEntry(aReporter);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMemoryReporterManager::RegisterStrongReporter(nsIMemoryReporter* aReporter)
{
  return RegisterReporterHelper(aReporter, /* force = */ false,
                                /* strong = */ true);
}

NS_IMETHODIMP
nsMemoryReporterManager::RegisterWeakReporter(nsIMemoryReporter* aReporter)
{
  return RegisterReporterHelper(aReporter, /* force = */ false,
                                /* strong = */ false);
}

NS_IMETHODIMP
nsMemoryReporterManager::RegisterStrongReporterEvenIfBlocked(
  nsIMemoryReporter* aReporter)
{
  return RegisterReporterHelper(aReporter, /* force = */ true,
                                /* strong = */ true);
}

NS_IMETHODIMP
nsMemoryReporterManager::UnregisterStrongReporter(nsIMemoryReporter* aReporter)
{
  // This method is thread-safe.
  mozilla::MutexAutoLock autoLock(mMutex);

  MOZ_ASSERT(!mWeakReporters->Contains(aReporter));

  if (mStrongReporters->Contains(aReporter)) {
    mStrongReporters->RemoveEntry(aReporter);
    return NS_OK;
  }

  // We don't register new reporters when the block is in place, but we do
  // unregister existing reporters. This is so we don't keep holding strong
  // references that these reporters aren't expecting (which can keep them
  // alive longer than intended).
  if (mSavedStrongReporters && mSavedStrongReporters->Contains(aReporter)) {
    mSavedStrongReporters->RemoveEntry(aReporter);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsMemoryReporterManager::UnregisterWeakReporter(nsIMemoryReporter* aReporter)
{
  // This method is thread-safe.
  mozilla::MutexAutoLock autoLock(mMutex);

  MOZ_ASSERT(!mStrongReporters->Contains(aReporter));

  if (mWeakReporters->Contains(aReporter)) {
    mWeakReporters->RemoveEntry(aReporter);
    return NS_OK;
  }

  // We don't register new reporters when the block is in place, but we do
  // unregister existing reporters. This is so we don't keep holding weak
  // references that the old reporters aren't expecting (which can end up as
  // dangling pointers that lead to use-after-frees).
  if (mSavedWeakReporters && mSavedWeakReporters->Contains(aReporter)) {
    mSavedWeakReporters->RemoveEntry(aReporter);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsMemoryReporterManager::BlockRegistrationAndHideExistingReporters()
{
  // This method is thread-safe.
  mozilla::MutexAutoLock autoLock(mMutex);
  if (mIsRegistrationBlocked) {
    return NS_ERROR_FAILURE;
  }
  mIsRegistrationBlocked = true;

  // Hide the existing reporters, saving them for later restoration.
  MOZ_ASSERT(!mSavedStrongReporters);
  MOZ_ASSERT(!mSavedWeakReporters);
  mSavedStrongReporters = mStrongReporters;
  mSavedWeakReporters = mWeakReporters;
  mStrongReporters = new StrongReportersTable();
  mWeakReporters = new WeakReportersTable();

  return NS_OK;
}

NS_IMETHODIMP
nsMemoryReporterManager::UnblockRegistrationAndRestoreOriginalReporters()
{
  // This method is thread-safe.
  mozilla::MutexAutoLock autoLock(mMutex);
  if (!mIsRegistrationBlocked) {
    return NS_ERROR_FAILURE;
  }

  // Banish the current reporters, and restore the hidden ones.
  delete mStrongReporters;
  delete mWeakReporters;
  mStrongReporters = mSavedStrongReporters;
  mWeakReporters = mSavedWeakReporters;
  mSavedStrongReporters = nullptr;
  mSavedWeakReporters = nullptr;

  mIsRegistrationBlocked = false;
  return NS_OK;
}

// This is just a wrapper for int64_t that implements nsISupports, so it can be
// passed to nsIMemoryReporter::CollectReports.
class Int64Wrapper final : public nsISupports
{
  ~Int64Wrapper() {}

public:
  NS_DECL_ISUPPORTS
  Int64Wrapper() : mValue(0)
  {
  }
  int64_t mValue;
};

NS_IMPL_ISUPPORTS0(Int64Wrapper)

class ExplicitCallback final : public nsIHandleReportCallback
{
  ~ExplicitCallback() {}

public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD Callback(const nsACString& aProcess, const nsACString& aPath,
                      int32_t aKind, int32_t aUnits, int64_t aAmount,
                      const nsACString& aDescription,
                      nsISupports* aWrappedExplicit) override
  {
    // Using the "heap-allocated" reporter here instead of
    // nsMemoryReporterManager.heapAllocated goes against the usual
    // pattern.  But it's for a good reason:  in tests, we can easily
    // create artificial (i.e. deterministic) reporters -- which allows us
    // to precisely test nsMemoryReporterManager.explicit -- but we can't
    // do that for distinguished amounts.
    if (aPath.EqualsLiteral("heap-allocated") ||
        (aKind == nsIMemoryReporter::KIND_NONHEAP &&
         PromiseFlatCString(aPath).Find("explicit") == 0)) {
      Int64Wrapper* wrappedInt64 = static_cast<Int64Wrapper*>(aWrappedExplicit);
      wrappedInt64->mValue += aAmount;
    }
    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS(ExplicitCallback, nsIHandleReportCallback)

NS_IMETHODIMP
nsMemoryReporterManager::GetExplicit(int64_t* aAmount)
{
  if (NS_WARN_IF(!aAmount)) {
    return NS_ERROR_INVALID_ARG;
  }
  *aAmount = 0;
#ifndef HAVE_JEMALLOC_STATS
  return NS_ERROR_NOT_AVAILABLE;
#else

  // For each reporter we call CollectReports and filter out the
  // non-explicit, non-NONHEAP measurements (except for "heap-allocated").
  // That's lots of wasted work, and we used to have a GetExplicitNonHeap()
  // method which did this more efficiently, but it ended up being more
  // trouble than it was worth.

  nsRefPtr<ExplicitCallback> handleReport = new ExplicitCallback();
  nsRefPtr<Int64Wrapper> wrappedExplicitSize = new Int64Wrapper();

  // Anonymization doesn't matter here, because we're only summing all the
  // reported values. Enable it anyway because it's slightly faster, since it
  // doesn't have to get URLs, find notable strings, etc.
  GetReportsForThisProcess(handleReport, wrappedExplicitSize,
                           /* anonymize = */ true);

  *aAmount = wrappedExplicitSize->mValue;

  return NS_OK;
#endif // HAVE_JEMALLOC_STATS
}

NS_IMETHODIMP
nsMemoryReporterManager::GetVsize(int64_t* aVsize)
{
#ifdef HAVE_VSIZE_AND_RESIDENT_REPORTERS
  return VsizeDistinguishedAmount(aVsize);
#else
  *aVsize = 0;
  return NS_ERROR_NOT_AVAILABLE;
#endif
}

NS_IMETHODIMP
nsMemoryReporterManager::GetVsizeMaxContiguous(int64_t* aAmount)
{
#ifdef HAVE_VSIZE_MAX_CONTIGUOUS_REPORTER
  return VsizeMaxContiguousDistinguishedAmount(aAmount);
#else
  *aAmount = 0;
  return NS_ERROR_NOT_AVAILABLE;
#endif
}

NS_IMETHODIMP
nsMemoryReporterManager::GetResident(int64_t* aAmount)
{
#ifdef HAVE_VSIZE_AND_RESIDENT_REPORTERS
  return ResidentDistinguishedAmount(aAmount);
#else
  *aAmount = 0;
  return NS_ERROR_NOT_AVAILABLE;
#endif
}

NS_IMETHODIMP
nsMemoryReporterManager::GetResidentFast(int64_t* aAmount)
{
#ifdef HAVE_VSIZE_AND_RESIDENT_REPORTERS
  return ResidentFastDistinguishedAmount(aAmount);
#else
  *aAmount = 0;
  return NS_ERROR_NOT_AVAILABLE;
#endif
}

/*static*/ int64_t
nsMemoryReporterManager::ResidentFast()
{
#ifdef HAVE_VSIZE_AND_RESIDENT_REPORTERS
  int64_t amount;
  nsresult rv = ResidentFastDistinguishedAmount(&amount);
  NS_ENSURE_SUCCESS(rv, 0);
  return amount;
#else
  return 0;
#endif
}

NS_IMETHODIMP
nsMemoryReporterManager::GetResidentPeak(int64_t* aAmount)
{
#ifdef HAVE_RESIDENT_PEAK_REPORTER
  return ResidentPeakDistinguishedAmount(aAmount);
#else
  *aAmount = 0;
  return NS_ERROR_NOT_AVAILABLE;
#endif
}

/*static*/ int64_t
nsMemoryReporterManager::ResidentPeak()
{
#ifdef HAVE_RESIDENT_PEAK_REPORTER
  int64_t amount = 0;
  nsresult rv = ResidentPeakDistinguishedAmount(&amount);
  NS_ENSURE_SUCCESS(rv, 0);
  return amount;
#else
  return 0;
#endif
}

NS_IMETHODIMP
nsMemoryReporterManager::GetResidentUnique(int64_t* aAmount)
{
#ifdef HAVE_RESIDENT_UNIQUE_REPORTER
  return ResidentUniqueDistinguishedAmount(aAmount);
#else
  *aAmount = 0;
  return NS_ERROR_NOT_AVAILABLE;
#endif
}

/*static*/ int64_t
nsMemoryReporterManager::ResidentUnique()
{
#ifdef HAVE_RESIDENT_UNIQUE_REPORTER
  int64_t amount = 0;
  nsresult rv = ResidentUniqueDistinguishedAmount(&amount);
  NS_ENSURE_SUCCESS(rv, 0);
  return amount;
#else
  return 0;
#endif
}

NS_IMETHODIMP
nsMemoryReporterManager::GetHeapAllocated(int64_t* aAmount)
{
#ifdef HAVE_JEMALLOC_STATS
  jemalloc_stats_t stats;
  jemalloc_stats(&stats);
  *aAmount = stats.allocated;
  return NS_OK;
#else
  *aAmount = 0;
  return NS_ERROR_NOT_AVAILABLE;
#endif
}

// This has UNITS_PERCENTAGE, so it is multiplied by 100x.
NS_IMETHODIMP
nsMemoryReporterManager::GetHeapOverheadRatio(int64_t* aAmount)
{
#ifdef HAVE_JEMALLOC_STATS
  jemalloc_stats_t stats;
  jemalloc_stats(&stats);
  *aAmount = HeapOverheadRatio(&stats);
  return NS_OK;
#else
  *aAmount = 0;
  return NS_ERROR_NOT_AVAILABLE;
#endif
}

static nsresult
GetInfallibleAmount(InfallibleAmountFn aAmountFn, int64_t* aAmount)
{
  if (aAmountFn) {
    *aAmount = aAmountFn();
    return NS_OK;
  }
  *aAmount = 0;
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsMemoryReporterManager::GetJSMainRuntimeGCHeap(int64_t* aAmount)
{
  return GetInfallibleAmount(mAmountFns.mJSMainRuntimeGCHeap, aAmount);
}

NS_IMETHODIMP
nsMemoryReporterManager::GetJSMainRuntimeTemporaryPeak(int64_t* aAmount)
{
  return GetInfallibleAmount(mAmountFns.mJSMainRuntimeTemporaryPeak, aAmount);
}

NS_IMETHODIMP
nsMemoryReporterManager::GetJSMainRuntimeCompartmentsSystem(int64_t* aAmount)
{
  return GetInfallibleAmount(mAmountFns.mJSMainRuntimeCompartmentsSystem,
                             aAmount);
}

NS_IMETHODIMP
nsMemoryReporterManager::GetJSMainRuntimeCompartmentsUser(int64_t* aAmount)
{
  return GetInfallibleAmount(mAmountFns.mJSMainRuntimeCompartmentsUser,
                             aAmount);
}

NS_IMETHODIMP
nsMemoryReporterManager::GetImagesContentUsedUncompressed(int64_t* aAmount)
{
  return GetInfallibleAmount(mAmountFns.mImagesContentUsedUncompressed,
                             aAmount);
}

NS_IMETHODIMP
nsMemoryReporterManager::GetStorageSQLite(int64_t* aAmount)
{
  return GetInfallibleAmount(mAmountFns.mStorageSQLite, aAmount);
}

NS_IMETHODIMP
nsMemoryReporterManager::GetLowMemoryEventsVirtual(int64_t* aAmount)
{
  return GetInfallibleAmount(mAmountFns.mLowMemoryEventsVirtual, aAmount);
}

NS_IMETHODIMP
nsMemoryReporterManager::GetLowMemoryEventsPhysical(int64_t* aAmount)
{
  return GetInfallibleAmount(mAmountFns.mLowMemoryEventsPhysical, aAmount);
}

NS_IMETHODIMP
nsMemoryReporterManager::GetGhostWindows(int64_t* aAmount)
{
  return GetInfallibleAmount(mAmountFns.mGhostWindows, aAmount);
}

NS_IMETHODIMP
nsMemoryReporterManager::GetPageFaultsHard(int64_t* aAmount)
{
#ifdef HAVE_PAGE_FAULT_REPORTERS
  return PageFaultsHardDistinguishedAmount(aAmount);
#else
  *aAmount = 0;
  return NS_ERROR_NOT_AVAILABLE;
#endif
}

NS_IMETHODIMP
nsMemoryReporterManager::GetHasMozMallocUsableSize(bool* aHas)
{
  void* p = malloc(16);
  if (!p) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  size_t usable = moz_malloc_usable_size(p);
  free(p);
  *aHas = !!(usable > 0);
  return NS_OK;
}

NS_IMETHODIMP
nsMemoryReporterManager::GetIsDMDEnabled(bool* aIsEnabled)
{
#ifdef MOZ_DMD
  *aIsEnabled = true;
#else
  *aIsEnabled = false;
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsMemoryReporterManager::GetIsDMDRunning(bool* aIsRunning)
{
#ifdef MOZ_DMD
  *aIsRunning = dmd::IsRunning();
#else
  *aIsRunning = false;
#endif
  return NS_OK;
}

namespace {

/**
 * This runnable lets us implement
 * nsIMemoryReporterManager::MinimizeMemoryUsage().  We fire a heap-minimize
 * notification, spin the event loop, and repeat this process a few times.
 *
 * When this sequence finishes, we invoke the callback function passed to the
 * runnable's constructor.
 */
class MinimizeMemoryUsageRunnable : public nsRunnable
{
public:
  explicit MinimizeMemoryUsageRunnable(nsIRunnable* aCallback)
    : mCallback(aCallback)
    , mRemainingIters(sNumIters)
  {
  }

  NS_IMETHOD Run()
  {
    nsCOMPtr<nsIObserverService> os = services::GetObserverService();
    if (!os) {
      return NS_ERROR_FAILURE;
    }

    if (mRemainingIters == 0) {
      os->NotifyObservers(nullptr, "after-minimize-memory-usage",
                          MOZ_UTF16("MinimizeMemoryUsageRunnable"));
      if (mCallback) {
        mCallback->Run();
      }
      return NS_OK;
    }

    os->NotifyObservers(nullptr, "memory-pressure",
                        MOZ_UTF16("heap-minimize"));
    mRemainingIters--;
    NS_DispatchToMainThread(this);

    return NS_OK;
  }

private:
  // Send sNumIters heap-minimize notifications, spinning the event
  // loop after each notification (see bug 610166 comment 12 for an
  // explanation), because one notification doesn't cut it.
  static const uint32_t sNumIters = 3;

  nsCOMPtr<nsIRunnable> mCallback;
  uint32_t mRemainingIters;
};

} // namespace

NS_IMETHODIMP
nsMemoryReporterManager::MinimizeMemoryUsage(nsIRunnable* aCallback)
{
  nsRefPtr<MinimizeMemoryUsageRunnable> runnable =
    new MinimizeMemoryUsageRunnable(aCallback);

  return NS_DispatchToMainThread(runnable);
}

NS_IMETHODIMP
nsMemoryReporterManager::SizeOfTab(nsIDOMWindow* aTopWindow,
                                   int64_t* aJSObjectsSize,
                                   int64_t* aJSStringsSize,
                                   int64_t* aJSOtherSize,
                                   int64_t* aDomSize,
                                   int64_t* aStyleSize,
                                   int64_t* aOtherSize,
                                   int64_t* aTotalSize,
                                   double*  aJSMilliseconds,
                                   double*  aNonJSMilliseconds)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aTopWindow);
  nsCOMPtr<nsPIDOMWindow> piWindow = do_QueryInterface(aTopWindow);
  if (NS_WARN_IF(!global) || NS_WARN_IF(!piWindow)) {
    return NS_ERROR_FAILURE;
  }

  TimeStamp t1 = TimeStamp::Now();

  // Measure JS memory consumption (and possibly some non-JS consumption, via
  // |jsPrivateSize|).
  size_t jsObjectsSize, jsStringsSize, jsPrivateSize, jsOtherSize;
  nsresult rv = mSizeOfTabFns.mJS(global->GetGlobalJSObject(),
                                  &jsObjectsSize, &jsStringsSize,
                                  &jsPrivateSize, &jsOtherSize);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  TimeStamp t2 = TimeStamp::Now();

  // Measure non-JS memory consumption.
  size_t domSize, styleSize, otherSize;
  mSizeOfTabFns.mNonJS(piWindow, &domSize, &styleSize, &otherSize);

  TimeStamp t3 = TimeStamp::Now();

  *aTotalSize = 0;
#define DO(aN, n) { *aN = (n); *aTotalSize += (n); }
  DO(aJSObjectsSize, jsObjectsSize);
  DO(aJSStringsSize, jsStringsSize);
  DO(aJSOtherSize,   jsOtherSize);
  DO(aDomSize,       jsPrivateSize + domSize);
  DO(aStyleSize,     styleSize);
  DO(aOtherSize,     otherSize);
#undef DO

  *aJSMilliseconds    = (t2 - t1).ToMilliseconds();
  *aNonJSMilliseconds = (t3 - t2).ToMilliseconds();

  return NS_OK;
}

namespace mozilla {

nsresult
RegisterStrongMemoryReporter(nsIMemoryReporter* aReporter)
{
  // Hold a strong reference to the argument to make sure it gets released if
  // we return early below.
  nsCOMPtr<nsIMemoryReporter> reporter = aReporter;

  nsCOMPtr<nsIMemoryReporterManager> mgr =
    do_GetService("@mozilla.org/memory-reporter-manager;1");
  if (!mgr) {
    return NS_ERROR_FAILURE;
  }
  return mgr->RegisterStrongReporter(reporter);
}

nsresult
RegisterWeakMemoryReporter(nsIMemoryReporter* aReporter)
{
  nsCOMPtr<nsIMemoryReporterManager> mgr =
    do_GetService("@mozilla.org/memory-reporter-manager;1");
  if (!mgr) {
    return NS_ERROR_FAILURE;
  }
  return mgr->RegisterWeakReporter(aReporter);
}

nsresult
UnregisterStrongMemoryReporter(nsIMemoryReporter* aReporter)
{
  nsCOMPtr<nsIMemoryReporterManager> mgr =
    do_GetService("@mozilla.org/memory-reporter-manager;1");
  if (!mgr) {
    return NS_ERROR_FAILURE;
  }
  return mgr->UnregisterStrongReporter(aReporter);
}

nsresult
UnregisterWeakMemoryReporter(nsIMemoryReporter* aReporter)
{
  nsCOMPtr<nsIMemoryReporterManager> mgr =
    do_GetService("@mozilla.org/memory-reporter-manager;1");
  if (!mgr) {
    return NS_ERROR_FAILURE;
  }
  return mgr->UnregisterWeakReporter(aReporter);
}

#define GET_MEMORY_REPORTER_MANAGER(mgr)                                      \
  nsRefPtr<nsMemoryReporterManager> mgr =                                     \
    nsMemoryReporterManager::GetOrCreate();                                   \
  if (!mgr) {                                                                 \
    return NS_ERROR_FAILURE;                                                  \
  }

// Macro for generating functions that register distinguished amount functions
// with the memory reporter manager.
#define DEFINE_REGISTER_DISTINGUISHED_AMOUNT(kind, name)                      \
  nsresult                                                                    \
  Register##name##DistinguishedAmount(kind##AmountFn aAmountFn)               \
  {                                                                           \
    GET_MEMORY_REPORTER_MANAGER(mgr)                                          \
    mgr->mAmountFns.m##name = aAmountFn;                                      \
    return NS_OK;                                                             \
  }

#define DEFINE_UNREGISTER_DISTINGUISHED_AMOUNT(name)                          \
  nsresult                                                                    \
  Unregister##name##DistinguishedAmount()                                     \
  {                                                                           \
    GET_MEMORY_REPORTER_MANAGER(mgr)                                          \
    mgr->mAmountFns.m##name = nullptr;                                        \
    return NS_OK;                                                             \
  }

DEFINE_REGISTER_DISTINGUISHED_AMOUNT(Infallible, JSMainRuntimeGCHeap)
DEFINE_REGISTER_DISTINGUISHED_AMOUNT(Infallible, JSMainRuntimeTemporaryPeak)
DEFINE_REGISTER_DISTINGUISHED_AMOUNT(Infallible, JSMainRuntimeCompartmentsSystem)
DEFINE_REGISTER_DISTINGUISHED_AMOUNT(Infallible, JSMainRuntimeCompartmentsUser)

DEFINE_REGISTER_DISTINGUISHED_AMOUNT(Infallible, ImagesContentUsedUncompressed)

DEFINE_REGISTER_DISTINGUISHED_AMOUNT(Infallible, StorageSQLite)
DEFINE_UNREGISTER_DISTINGUISHED_AMOUNT(StorageSQLite)

DEFINE_REGISTER_DISTINGUISHED_AMOUNT(Infallible, LowMemoryEventsVirtual)
DEFINE_REGISTER_DISTINGUISHED_AMOUNT(Infallible, LowMemoryEventsPhysical)

DEFINE_REGISTER_DISTINGUISHED_AMOUNT(Infallible, GhostWindows)

#undef DEFINE_REGISTER_DISTINGUISHED_AMOUNT
#undef DEFINE_UNREGISTER_DISTINGUISHED_AMOUNT

#define DEFINE_REGISTER_SIZE_OF_TAB(name)                                     \
  nsresult                                                                    \
  Register##name##SizeOfTab(name##SizeOfTabFn aSizeOfTabFn)                   \
  {                                                                           \
    GET_MEMORY_REPORTER_MANAGER(mgr)                                          \
    mgr->mSizeOfTabFns.m##name = aSizeOfTabFn;                                \
    return NS_OK;                                                             \
  }

DEFINE_REGISTER_SIZE_OF_TAB(JS);
DEFINE_REGISTER_SIZE_OF_TAB(NonJS);

#undef DEFINE_REGISTER_SIZE_OF_TAB

#undef GET_MEMORY_REPORTER_MANAGER

} // namespace mozilla

#if defined(MOZ_DMD)

namespace mozilla {
namespace dmd {

class DoNothingCallback final : public nsIHandleReportCallback
{
public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD Callback(const nsACString& aProcess, const nsACString& aPath,
                      int32_t aKind, int32_t aUnits, int64_t aAmount,
                      const nsACString& aDescription,
                      nsISupports* aData) override
  {
    // Do nothing;  the reporter has already reported to DMD.
    return NS_OK;
  }

private:
  ~DoNothingCallback() {}
};

NS_IMPL_ISUPPORTS(DoNothingCallback, nsIHandleReportCallback)

void
RunReportersForThisProcess()
{
  nsCOMPtr<nsIMemoryReporterManager> mgr =
    do_GetService("@mozilla.org/memory-reporter-manager;1");

  nsRefPtr<DoNothingCallback> doNothing = new DoNothingCallback();

  mgr->GetReportsForThisProcess(doNothing, nullptr, /* anonymize = */ false);
}

} // namespace dmd
} // namespace mozilla

#endif  // defined(MOZ_DMD)

