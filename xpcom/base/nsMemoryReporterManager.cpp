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
#include "nsProxyRelease.h"
#include "nsServiceManagerUtils.h"
#include "nsMemoryReporterManager.h"
#include "nsITimer.h"
#include "nsThreadUtils.h"
#include "nsPIDOMWindow.h"
#include "nsIObserverService.h"
#include "nsIGlobalObject.h"
#include "nsIXPConnect.h"
#ifdef MOZ_GECKO_PROFILER
#include "GeckoProfilerReporter.h"
#endif
#if defined(XP_UNIX) || defined(MOZ_DMD)
#include "nsMemoryInfoDumper.h"
#endif
#include "nsNetCID.h"
#include "nsThread.h"
#include "mozilla/Attributes.h"
#include "mozilla/MemoryReportingProcess.h"
#include "mozilla/PodOperations.h"
#include "mozilla/Preferences.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/Services.h"
#include "mozilla/Telemetry.h"
#include "mozilla/UniquePtrExtensions.h"
#include "mozilla/dom/MemoryReportTypes.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/gfx/GPUProcessManager.h"
#include "mozilla/ipc/FileDescriptorUtils.h"

#ifdef XP_WIN
#include "mozilla/MemoryInfo.h"

#include <process.h>
#ifndef getpid
#define getpid _getpid
#endif
#else
#include <unistd.h>
#endif

using namespace mozilla;
using namespace dom;

#if defined(MOZ_MEMORY)
#  define HAVE_JEMALLOC_STATS 1
#  include "mozmemory.h"
#endif  // MOZ_MEMORY

#if defined(XP_LINUX)

#include "mozilla/MemoryMapping.h"

#include <malloc.h>
#include <string.h>
#include <stdlib.h>

static MOZ_MUST_USE nsresult
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

static MOZ_MUST_USE nsresult
GetProcSelfSmapsPrivate(int64_t* aN)
{
  // You might be tempted to calculate USS by subtracting the "shared" value
  // from the "resident" value in /proc/<pid>/statm. But at least on Linux,
  // statm's "shared" value actually counts pages backed by files, which has
  // little to do with whether the pages are actually shared. /proc/self/smaps
  // on the other hand appears to give us the correct information.

  nsTArray<MemoryMapping> mappings(1024);
  MOZ_TRY(GetMemoryMappings(mappings));

  int64_t amount = 0;
  for (auto& mapping : mappings) {
    amount += mapping.Private_Clean();
    amount += mapping.Private_Dirty();
  }
  *aN = amount;
  return NS_OK;
}

#define HAVE_VSIZE_AND_RESIDENT_REPORTERS 1
static MOZ_MUST_USE nsresult
VsizeDistinguishedAmount(int64_t* aN)
{
  return GetProcSelfStatmField(0, aN);
}

static MOZ_MUST_USE nsresult
ResidentDistinguishedAmount(int64_t* aN)
{
  return GetProcSelfStatmField(1, aN);
}

static MOZ_MUST_USE nsresult
ResidentFastDistinguishedAmount(int64_t* aN)
{
  return ResidentDistinguishedAmount(aN);
}

#define HAVE_RESIDENT_UNIQUE_REPORTER 1
static MOZ_MUST_USE nsresult
ResidentUniqueDistinguishedAmount(int64_t* aN)
{
  return GetProcSelfSmapsPrivate(aN);
}

#ifdef HAVE_MALLINFO
#define HAVE_SYSTEM_HEAP_REPORTER 1
static MOZ_MUST_USE nsresult
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
#endif

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

static MOZ_MUST_USE nsresult
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
static MOZ_MUST_USE nsresult
VsizeDistinguishedAmount(int64_t* aN)
{
  KINFO_PROC proc;
  nsresult rv = GetKinfoProcSelf(&proc);
  if (NS_SUCCEEDED(rv)) {
    *aN = KP_SIZE(proc);
  }
  return rv;
}

static MOZ_MUST_USE nsresult
ResidentDistinguishedAmount(int64_t* aN)
{
  KINFO_PROC proc;
  nsresult rv = GetKinfoProcSelf(&proc);
  if (NS_SUCCEEDED(rv)) {
    *aN = KP_RSS(proc);
  }
  return rv;
}

static MOZ_MUST_USE nsresult
ResidentFastDistinguishedAmount(int64_t* aN)
{
  return ResidentDistinguishedAmount(aN);
}

#ifdef __FreeBSD__
#include <libutil.h>
#include <algorithm>

static MOZ_MUST_USE nsresult
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
static MOZ_MUST_USE nsresult
PrivateDistinguishedAmount(int64_t* aN)
{
  int64_t priv;
  nsresult rv = GetKinfoVmentrySelf(&priv, nullptr);
  NS_ENSURE_SUCCESS(rv, rv);
  *aN = priv * getpagesize();
  return NS_OK;
}

#define HAVE_VSIZE_MAX_CONTIGUOUS_REPORTER 1
static MOZ_MUST_USE nsresult
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

static void
XMappingIter(int64_t& aVsize, int64_t& aResident, int64_t& aShared)
{
  aVsize = -1;
  aResident = -1;
  aShared = -1;
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
          aShared = 0;
          for (int i = 0; i < n / sizeof(prxmap_t); i++) {
            aVsize += prmapp[i].pr_size;
            aResident += prmapp[i].pr_rss * prmapp[i].pr_pagesize;
            if (prmapp[i].pr_mflags & MA_SHARED) {
              aShared += prmapp[i].pr_rss * prmapp[i].pr_pagesize;
            }
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
static MOZ_MUST_USE nsresult
VsizeDistinguishedAmount(int64_t* aN)
{
  int64_t vsize, resident, shared;
  XMappingIter(vsize, resident, shared);
  if (vsize == -1) {
    return NS_ERROR_FAILURE;
  }
  *aN = vsize;
  return NS_OK;
}

static MOZ_MUST_USE nsresult
ResidentDistinguishedAmount(int64_t* aN)
{
  int64_t vsize, resident, shared;
  XMappingIter(vsize, resident, shared);
  if (resident == -1) {
    return NS_ERROR_FAILURE;
  }
  *aN = resident;
  return NS_OK;
}

static MOZ_MUST_USE nsresult
ResidentFastDistinguishedAmount(int64_t* aN)
{
  return ResidentDistinguishedAmount(aN);
}

#define HAVE_RESIDENT_UNIQUE_REPORTER 1
static MOZ_MUST_USE nsresult
ResidentUniqueDistinguishedAmount(int64_t* aN)
{
  int64_t vsize, resident, shared;
  XMappingIter(vsize, resident, shared);
  if (resident == -1) {
    return NS_ERROR_FAILURE;
  }
  *aN = resident - shared;
  return NS_OK;
}

#elif defined(XP_MACOSX)

#include <mach/mach_init.h>
#include <mach/mach_vm.h>
#include <mach/shared_region.h>
#include <mach/task.h>
#include <sys/sysctl.h>

static MOZ_MUST_USE bool
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
static MOZ_MUST_USE nsresult
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
static MOZ_MUST_USE nsresult
ResidentDistinguishedAmountHelper(int64_t* aN, bool aDoPurge)
{
#ifdef HAVE_JEMALLOC_STATS
  if (aDoPurge) {
    Telemetry::AutoTimer<Telemetry::MEMORY_FREE_PURGED_PAGES_MS> timer;
    jemalloc_purge_freed_pages();
  }
#endif

  task_basic_info ti;
  if (!GetTaskBasicInfo(&ti)) {
    return NS_ERROR_FAILURE;
  }
  *aN = ti.resident_size;
  return NS_OK;
}

static MOZ_MUST_USE nsresult
ResidentFastDistinguishedAmount(int64_t* aN)
{
  return ResidentDistinguishedAmountHelper(aN, /* doPurge = */ false);
}

static MOZ_MUST_USE nsresult
ResidentDistinguishedAmount(int64_t* aN)
{
  return ResidentDistinguishedAmountHelper(aN, /* doPurge = */ true);
}

#define HAVE_RESIDENT_UNIQUE_REPORTER 1

static bool
InSharedRegion(mach_vm_address_t aAddr, cpu_type_t aType)
{
  mach_vm_address_t base;
  mach_vm_address_t size;

  switch (aType) {
    case CPU_TYPE_ARM:
      base = SHARED_REGION_BASE_ARM;
      size = SHARED_REGION_SIZE_ARM;
      break;
    case CPU_TYPE_I386:
      base = SHARED_REGION_BASE_I386;
      size = SHARED_REGION_SIZE_I386;
      break;
    case CPU_TYPE_X86_64:
      base = SHARED_REGION_BASE_X86_64;
      size = SHARED_REGION_SIZE_X86_64;
      break;
    default:
      return false;
  }

  return base <= aAddr && aAddr < (base + size);
}

static MOZ_MUST_USE nsresult
ResidentUniqueDistinguishedAmount(int64_t* aN)
{
  if (!aN) {
    return NS_ERROR_FAILURE;
  }

  cpu_type_t cpu_type;
  size_t len = sizeof(cpu_type);
  if (sysctlbyname("sysctl.proc_cputype", &cpu_type, &len, NULL, 0) != 0) {
    return NS_ERROR_FAILURE;
  }

  // Roughly based on libtop_update_vm_regions in
  // http://www.opensource.apple.com/source/top/top-100.1.2/libtop.c
  size_t privatePages = 0;
  mach_vm_size_t size = 0;
  for (mach_vm_address_t addr = MACH_VM_MIN_ADDRESS; ; addr += size) {
    vm_region_top_info_data_t info;
    mach_msg_type_number_t infoCount = VM_REGION_TOP_INFO_COUNT;
    mach_port_t objectName;

    kern_return_t kr =
        mach_vm_region(mach_task_self(), &addr, &size, VM_REGION_TOP_INFO,
                       reinterpret_cast<vm_region_info_t>(&info),
                       &infoCount, &objectName);
    if (kr == KERN_INVALID_ADDRESS) {
      // Done iterating VM regions.
      break;
    } else if (kr != KERN_SUCCESS) {
      return NS_ERROR_FAILURE;
    }

    if (InSharedRegion(addr, cpu_type) && info.share_mode != SM_PRIVATE) {
        continue;
    }

    switch (info.share_mode) {
      case SM_LARGE_PAGE:
        // NB: Large pages are not shareable and always resident.
      case SM_PRIVATE:
        privatePages += info.private_pages_resident;
        privatePages += info.shared_pages_resident;
        break;
      case SM_COW:
        privatePages += info.private_pages_resident;
        if (info.ref_count == 1) {
          // Treat copy-on-write pages as private if they only have one reference.
          privatePages += info.shared_pages_resident;
        }
        break;
      case SM_SHARED:
      default:
        break;
    }
  }

  vm_size_t pageSize;
  if (host_page_size(mach_host_self(), &pageSize) != KERN_SUCCESS) {
    pageSize = PAGE_SIZE;
  }

  *aN = privatePages * pageSize;
  return NS_OK;
}

#elif defined(XP_WIN)

#include <windows.h>
#include <psapi.h>
#include <algorithm>

#define HAVE_VSIZE_AND_RESIDENT_REPORTERS 1
static MOZ_MUST_USE nsresult
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

static MOZ_MUST_USE nsresult
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

static MOZ_MUST_USE nsresult
ResidentFastDistinguishedAmount(int64_t* aN)
{
  return ResidentDistinguishedAmount(aN);
}

#define HAVE_RESIDENT_UNIQUE_REPORTER 1

static MOZ_MUST_USE nsresult
ResidentUniqueDistinguishedAmount(int64_t* aN)
{
  // Determine how many entries we need.
  PSAPI_WORKING_SET_INFORMATION tmp;
  DWORD tmpSize = sizeof(tmp);
  memset(&tmp, 0, tmpSize);

  HANDLE proc = GetCurrentProcess();
  QueryWorkingSet(proc, &tmp, tmpSize);

  // Fudge the size in case new entries are added between calls.
  size_t entries = tmp.NumberOfEntries * 2;

  if (!entries) {
    return NS_ERROR_FAILURE;
  }

  DWORD infoArraySize = tmpSize + (entries * sizeof(PSAPI_WORKING_SET_BLOCK));
  UniqueFreePtr<PSAPI_WORKING_SET_INFORMATION> infoArray(
      static_cast<PSAPI_WORKING_SET_INFORMATION*>(malloc(infoArraySize)));

  if (!infoArray) {
    return NS_ERROR_FAILURE;
  }

  if (!QueryWorkingSet(proc, infoArray.get(), infoArraySize)) {
    return NS_ERROR_FAILURE;
  }

  entries = static_cast<size_t>(infoArray->NumberOfEntries);
  size_t privatePages = 0;
  for (size_t i = 0; i < entries; i++) {
    // Count shared pages that only one process is using as private.
    if (!infoArray->WorkingSetInfo[i].Shared ||
        infoArray->WorkingSetInfo[i].ShareCount <= 1) {
      privatePages++;
    }
  }

  SYSTEM_INFO si;
  GetSystemInfo(&si);

  *aN = privatePages * si.dwPageSize;
  return NS_OK;
}

#define HAVE_VSIZE_MAX_CONTIGUOUS_REPORTER 1
static MOZ_MUST_USE nsresult
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
static MOZ_MUST_USE nsresult
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
static MOZ_MUST_USE nsresult
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

    // Bug 1235982: When Control Flow Guard is enabled for the process,
    // GetProcessHeap may return some protected heaps that are in read-only
    // memory and thus crash in HeapLock. Ignore such heaps.
    MEMORY_BASIC_INFORMATION mbi = {0};
    if (VirtualQuery(heap, &mbi, sizeof(mbi)) && mbi.Protect == PAGE_READONLY) {
      continue;
    }

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

struct SegmentKind
{
  DWORD mState;
  DWORD mType;
  DWORD mProtect;
  int mIsStack;
};

struct SegmentEntry : public PLDHashEntryHdr
{
  static PLDHashNumber HashKey(const void* aKey)
  {
    auto kind = static_cast<const SegmentKind*>(aKey);
    return mozilla::HashGeneric(kind->mState, kind->mType, kind->mProtect,
                                kind->mIsStack);
  }

  static bool MatchEntry(const PLDHashEntryHdr* aEntry, const void* aKey)
  {
    auto kind = static_cast<const SegmentKind*>(aKey);
    auto entry = static_cast<const SegmentEntry*>(aEntry);
    return kind->mState == entry->mKind.mState &&
           kind->mType == entry->mKind.mType &&
           kind->mProtect == entry->mKind.mProtect &&
           kind->mIsStack == entry->mKind.mIsStack;
  }

  static void InitEntry(PLDHashEntryHdr* aEntry, const void* aKey)
  {
    auto kind = static_cast<const SegmentKind*>(aKey);
    auto entry = static_cast<SegmentEntry*>(aEntry);
    entry->mKind = *kind;
    entry->mCount = 0;
    entry->mSize = 0;
  }

  static const PLDHashTableOps Ops;

  SegmentKind mKind;  // The segment kind.
  uint32_t mCount;    // The number of segments of this kind.
  size_t mSize;       // The combined size of segments of this kind.
};

/* static */ const PLDHashTableOps SegmentEntry::Ops = {
  SegmentEntry::HashKey,
  SegmentEntry::MatchEntry,
  PLDHashTable::MoveEntryStub,
  PLDHashTable::ClearEntryStub,
  SegmentEntry::InitEntry
};

class WindowsAddressSpaceReporter final : public nsIMemoryReporter
{
  ~WindowsAddressSpaceReporter() {}

public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                            nsISupports* aData, bool aAnonymize) override
  {
    // First iterate over all the segments and record how many of each kind
    // there were and their aggregate sizes. We use a hash table for this
    // because there are a couple of dozen different kinds possible.

    PLDHashTable table(&SegmentEntry::Ops, sizeof(SegmentEntry));
    MEMORY_BASIC_INFORMATION info = { 0 };
    bool isPrevSegStackGuard = false;
    for (size_t currentAddress = 0; ; ) {
      if (!VirtualQuery((LPCVOID)currentAddress, &info, sizeof(info))) {
        // Something went wrong, just return whatever we've got already.
        break;
      }

      size_t size = info.RegionSize;

      // Note that |type| and |protect| are ignored in some cases.
      DWORD state = info.State;
      DWORD type =
        (state == MEM_RESERVE || state == MEM_COMMIT) ? info.Type : 0;
      DWORD protect = (state == MEM_COMMIT) ? info.Protect : 0;
      bool isStack = isPrevSegStackGuard &&
                     state == MEM_COMMIT &&
                     type == MEM_PRIVATE &&
                     protect == PAGE_READWRITE;

      SegmentKind kind = { state, type, protect, isStack ? 1 : 0 };
      auto entry =
        static_cast<SegmentEntry*>(table.Add(&kind, mozilla::fallible));
      if (entry) {
        entry->mCount += 1;
        entry->mSize += size;
      }

      isPrevSegStackGuard = info.State == MEM_COMMIT &&
                            info.Type == MEM_PRIVATE &&
                            info.Protect == (PAGE_READWRITE|PAGE_GUARD);

      size_t lastAddress = currentAddress;
      currentAddress += size;

      // If we overflow, we've examined all of the address space.
      if (currentAddress < lastAddress) {
        break;
      }
    }

    // Then iterate over the hash table and report the details for each segment
    // kind.

    for (auto iter = table.Iter(); !iter.Done(); iter.Next()) {
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

      auto entry = static_cast<const SegmentEntry*>(iter.Get());

      nsCString path("address-space");

      switch (entry->mKind.mState) {
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
        switch (entry->mKind.mType) {
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
        DWORD protect = entry->mKind.mProtect;
        // Basic attributes. Exactly one of these should be set.
        if (protect & PAGE_EXECUTE) {
          path.AppendLiteral("/execute");
        }
        if (protect & PAGE_EXECUTE_READ) {
          path.AppendLiteral("/execute-read");
        }
        if (protect & PAGE_EXECUTE_READWRITE) {
          path.AppendLiteral("/execute-readwrite");
        }
        if (protect & PAGE_EXECUTE_WRITECOPY) {
          path.AppendLiteral("/execute-writecopy");
        }
        if (protect & PAGE_NOACCESS) {
          path.AppendLiteral("/noaccess");
        }
        if (protect & PAGE_READONLY) {
          path.AppendLiteral("/readonly");
        }
        if (protect & PAGE_READWRITE) {
          path.AppendLiteral("/readwrite");
        }
        if (protect & PAGE_WRITECOPY) {
          path.AppendLiteral("/writecopy");
        }

        // Modifiers. At most one of these should be set.
        if (protect & PAGE_GUARD) {
          path.AppendLiteral("+guard");
        }
        if (protect & PAGE_NOCACHE) {
          path.AppendLiteral("+nocache");
        }
        if (protect & PAGE_WRITECOMBINE) {
          path.AppendLiteral("+writecombine");
        }

        // Annotate likely stack segments, too.
        if (entry->mKind.mIsStack) {
          path.AppendLiteral("+stack");
        }
      }

      // Append the segment count.
      path.AppendPrintf("(segments=%u)", entry->mCount);

      aHandleReport->Callback(
        EmptyCString(), path, KIND_OTHER, UNITS_BYTES, entry->mSize,
        NS_LITERAL_CSTRING("From MEMORY_BASIC_INFORMATION."), aData);
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

  NS_IMETHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                            nsISupports* aData, bool aAnonymize) override
  {
    int64_t amount;
    if (NS_SUCCEEDED(VsizeMaxContiguousDistinguishedAmount(&amount))) {
      MOZ_COLLECT_REPORT(
        "vsize-max-contiguous", KIND_OTHER, UNITS_BYTES, amount,
        "Size of the maximum contiguous block of available virtual memory.");
    }
    return NS_OK;
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

  NS_IMETHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                            nsISupports* aData, bool aAnonymize) override
  {
    int64_t amount;
    if (NS_SUCCEEDED(PrivateDistinguishedAmount(&amount))) {
      MOZ_COLLECT_REPORT(
        "private", KIND_OTHER, UNITS_BYTES, amount,
"Memory that cannot be shared with other processes, including memory that is "
"committed and marked MEM_PRIVATE, data that is not mapped, and executable "
"pages that have been written to.");
    }
    return NS_OK;
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

  NS_IMETHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                            nsISupports* aData, bool aAnonymize) override
  {
    int64_t amount;
    if (NS_SUCCEEDED(VsizeDistinguishedAmount(&amount))) {
      MOZ_COLLECT_REPORT(
        "vsize", KIND_OTHER, UNITS_BYTES, amount,
"Memory mapped by the process, including code and data segments, the heap, "
"thread stacks, memory explicitly mapped by the process via mmap and similar "
"operations, and memory shared with other processes. This is the vsize figure "
"as reported by 'top' and 'ps'.  This figure is of limited use on Mac, where "
"processes share huge amounts of memory with one another.  But even on other "
"operating systems, 'resident' is a much better measure of the memory "
"resources used by the process.");
    }
    return NS_OK;
  }
};
NS_IMPL_ISUPPORTS(VsizeReporter, nsIMemoryReporter)

class ResidentReporter final : public nsIMemoryReporter
{
  ~ResidentReporter() {}

public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                            nsISupports* aData, bool aAnonymize) override
  {
    int64_t amount;
    if (NS_SUCCEEDED(ResidentDistinguishedAmount(&amount))) {
      MOZ_COLLECT_REPORT(
        "resident", KIND_OTHER, UNITS_BYTES, amount,
"Memory mapped by the process that is present in physical memory, also known "
"as the resident set size (RSS).  This is the best single figure to use when "
"considering the memory resources used by the process, but it depends both on "
"other processes being run and details of the OS kernel and so is best used "
"for comparing the memory usage of a single process at different points in "
"time.");
    }
    return NS_OK;
  }
};
NS_IMPL_ISUPPORTS(ResidentReporter, nsIMemoryReporter)

#endif  // HAVE_VSIZE_AND_RESIDENT_REPORTERS

#ifdef HAVE_RESIDENT_UNIQUE_REPORTER
class ResidentUniqueReporter final : public nsIMemoryReporter
{
  ~ResidentUniqueReporter() {}

public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                            nsISupports* aData, bool aAnonymize) override
  {
    int64_t amount = 0;
    if (NS_SUCCEEDED(ResidentUniqueDistinguishedAmount(&amount))) {
      MOZ_COLLECT_REPORT(
        "resident-unique", KIND_OTHER, UNITS_BYTES, amount,
"Memory mapped by the process that is present in physical memory and not "
"shared with any other processes.  This is also known as the process's unique "
"set size (USS).  This is the amount of RAM we'd expect to be freed if we "
"closed this process.");
    }
    return NS_OK;
  }
};
NS_IMPL_ISUPPORTS(ResidentUniqueReporter, nsIMemoryReporter)

#endif // HAVE_RESIDENT_UNIQUE_REPORTER

#ifdef HAVE_SYSTEM_HEAP_REPORTER

class SystemHeapReporter final : public nsIMemoryReporter
{
  ~SystemHeapReporter() {}

public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                            nsISupports* aData, bool aAnonymize) override
  {
    int64_t amount;
    if (NS_SUCCEEDED(SystemHeapSize(&amount))) {
      MOZ_COLLECT_REPORT(
        "system-heap-allocated", KIND_OTHER, UNITS_BYTES, amount,
"Memory used by the system allocator that is currently allocated to the "
"application. This is distinct from the jemalloc heap that Firefox uses for "
"most or all of its heap allocations. Ideally this number is zero, but "
"on some platforms we cannot force every heap allocation through jemalloc.");
    }
    return NS_OK;
  }
};
NS_IMPL_ISUPPORTS(SystemHeapReporter, nsIMemoryReporter)

#endif // HAVE_SYSTEM_HEAP_REPORTER

#ifdef XP_UNIX

#include <sys/resource.h>

#define HAVE_RESIDENT_PEAK_REPORTER 1

static MOZ_MUST_USE nsresult
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

  NS_IMETHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                            nsISupports* aData, bool aAnonymize) override
  {
    int64_t amount = 0;
    if (NS_SUCCEEDED(ResidentPeakDistinguishedAmount(&amount))) {
      MOZ_COLLECT_REPORT(
        "resident-peak", KIND_OTHER, UNITS_BYTES, amount,
"The peak 'resident' value for the lifetime of the process.");
    }
    return NS_OK;
  }
};
NS_IMPL_ISUPPORTS(ResidentPeakReporter, nsIMemoryReporter)

#define HAVE_PAGE_FAULT_REPORTERS 1

class PageFaultsSoftReporter final : public nsIMemoryReporter
{
  ~PageFaultsSoftReporter() {}

public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                            nsISupports* aData, bool aAnonymize) override
  {
    struct rusage usage;
    int err = getrusage(RUSAGE_SELF, &usage);
    if (err == 0) {
      int64_t amount = usage.ru_minflt;
      MOZ_COLLECT_REPORT(
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
    return NS_OK;
  }
};
NS_IMPL_ISUPPORTS(PageFaultsSoftReporter, nsIMemoryReporter)

static MOZ_MUST_USE nsresult
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

  NS_IMETHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                            nsISupports* aData, bool aAnonymize) override
  {
    int64_t amount = 0;
    if (NS_SUCCEEDED(PageFaultsHardDistinguishedAmount(&amount))) {
      MOZ_COLLECT_REPORT(
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
    return NS_OK;
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

static size_t
HeapOverhead(jemalloc_stats_t* aStats)
{
  return aStats->waste + aStats->bookkeeping +
         aStats->page_cache + aStats->bin_unused;
}

// This has UNITS_PERCENTAGE, so it is multiplied by 100x *again* on top of the
// 100x for the percentage.
static int64_t
HeapOverheadFraction(jemalloc_stats_t* aStats)
{
  size_t heapOverhead = HeapOverhead(aStats);
  size_t heapCommitted = aStats->allocated + heapOverhead;
  return int64_t(10000 * (heapOverhead / (double)heapCommitted));
}

class JemallocHeapReporter final : public nsIMemoryReporter
{
  ~JemallocHeapReporter() {}

public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                            nsISupports* aData, bool aAnonymize) override
  {
    jemalloc_stats_t stats;
    jemalloc_stats(&stats);

    MOZ_COLLECT_REPORT(
      "heap-committed/allocated", KIND_OTHER, UNITS_BYTES, stats.allocated,
"Memory mapped by the heap allocator that is currently allocated to the "
"application.  This may exceed the amount of memory requested by the "
"application because the allocator regularly rounds up request sizes. (The "
"exact amount requested is not recorded.)");

    MOZ_COLLECT_REPORT(
      "heap-allocated", KIND_OTHER, UNITS_BYTES, stats.allocated,
"The same as 'heap-committed/allocated'.");

    // We mark this and the other heap-overhead reporters as KIND_NONHEAP
    // because KIND_HEAP memory means "counted in heap-allocated", which
    // this is not.
    MOZ_COLLECT_REPORT(
      "explicit/heap-overhead/bin-unused", KIND_NONHEAP, UNITS_BYTES,
      stats.bin_unused,
"Unused bytes due to fragmentation in the bins used for 'small' (<= 2 KiB) "
"allocations. These bytes will be used if additional allocations occur.");

    if (stats.waste > 0) {
      MOZ_COLLECT_REPORT(
        "explicit/heap-overhead/waste", KIND_NONHEAP, UNITS_BYTES,
        stats.waste,
"Committed bytes which do not correspond to an active allocation and which the "
"allocator is not intentionally keeping alive (i.e., not "
"'explicit/heap-overhead/{bookkeeping,page-cache,bin-unused}').");
    }

    MOZ_COLLECT_REPORT(
      "explicit/heap-overhead/bookkeeping", KIND_NONHEAP, UNITS_BYTES,
      stats.bookkeeping,
"Committed bytes which the heap allocator uses for internal data structures.");

    MOZ_COLLECT_REPORT(
      "explicit/heap-overhead/page-cache", KIND_NONHEAP, UNITS_BYTES,
      stats.page_cache,
"Memory which the allocator could return to the operating system, but hasn't. "
"The allocator keeps this memory around as an optimization, so it doesn't "
"have to ask the OS the next time it needs to fulfill a request. This value "
"is typically not larger than a few megabytes.");

    MOZ_COLLECT_REPORT(
      "heap-committed/overhead", KIND_OTHER, UNITS_BYTES,
      HeapOverhead(&stats),
"The sum of 'explicit/heap-overhead/*'.");

    MOZ_COLLECT_REPORT(
      "heap-mapped", KIND_OTHER, UNITS_BYTES, stats.mapped,
"Amount of memory currently mapped. Includes memory that is uncommitted, i.e. "
"neither in physical memory nor paged to disk.");

    MOZ_COLLECT_REPORT(
      "heap-chunksize", KIND_OTHER, UNITS_BYTES, stats.chunksize,
      "Size of chunks.");

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

  NS_IMETHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                            nsISupports* aData, bool aAnonymize) override
  {
    AtomsSizes sizes;
    NS_AddSizeOfAtoms(MallocSizeOf, sizes);

    MOZ_COLLECT_REPORT(
      "explicit/atoms/table", KIND_HEAP, UNITS_BYTES, sizes.mTable,
      "Memory used by the atom table.");

    MOZ_COLLECT_REPORT(
      "explicit/atoms/dynamic-objects-and-chars", KIND_HEAP, UNITS_BYTES,
      sizes.mDynamicAtoms,
      "Memory used by dynamic atom objects and chars (which are stored "
      "at the end of each atom object).");

    return NS_OK;
  }
};
NS_IMPL_ISUPPORTS(AtomTablesReporter, nsIMemoryReporter)

#if defined(XP_LINUX) || defined(XP_WIN)
class ThreadStacksReporter final : public nsIMemoryReporter
{
  ~ThreadStacksReporter() = default;

public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                            nsISupports* aData, bool aAnonymize) override
  {
#ifdef XP_LINUX
    nsTArray<MemoryMapping> mappings(1024);
    MOZ_TRY(GetMemoryMappings(mappings));
#endif

    // Enumerating over active threads requires holding a lock, so we collect
    // info on all threads, and then call our reporter callbacks after releasing
    // the lock.
    struct ThreadData
    {
      nsCString mName;
      uint32_t mThreadId;
      size_t mPrivateSize;
    };
    AutoTArray<ThreadData, 32> threads;

    for (auto* thread : nsThread::Enumerate()) {
      if (!thread->StackBase()) {
        continue;
      }

#ifdef XP_LINUX
      int idx = mappings.BinaryIndexOf(thread->StackBase());
      if (idx < 0) {
        continue;
      }
      // Referenced() is the combined size of all pages in the region which have
      // ever been touched, and are therefore consuming memory. For stack
      // regions, these pages are guaranteed to be un-shared unless we fork
      // after creating threads (which we don't).
      size_t privateSize = mappings[idx].Referenced();

      // On Linux, we have to be very careful matching memory regions to thread
      // stacks.
      //
      // To begin with, the kernel only reports VM stats for regions of all
      // adjacent pages with the same flags, protection, and backing file.
      // There's no way to get finer-grained usage information for a subset of
      // those pages.
      //
      // Stack segments always have a guard page at the bottom of the stack
      // (assuming we only support stacks that grow down), so there's no danger
      // of them being merged with other stack regions. At the top, there's no
      // protection page, and no way to allocate one without using pthreads
      // directly and allocating our own stacks. So we get around the problem by
      // adding an extra VM flag (NOHUGEPAGES) to our stack region, which we
      // don't expect to be set on any heap regions. But this is not fool-proof.
      //
      // A second kink is that different C libraries (and different versions
      // thereof) report stack base locations and sizes differently with regard
      // to the guard page. For the libraries that include the guard page in the
      // stack size base pointer, we need to adjust those values to compensate.
      // But it's possible that our logic will get out of sync with library
      // changes, or someone will compile with an unexpected library.
      //
      //
      // The upshot of all of this is that there may be configurations that our
      // special cases don't cover. And if there are, we want to know about it.
      // So assert that total size of the memory region we're reporting actually
      // matches the allocated size of the thread stack.
#ifndef ANDROID
      MOZ_ASSERT(mappings[idx].Size() == thread->StackSize(),
                 "Mapping region size doesn't match stack allocation size");
#endif
#else
      auto memInfo = MemoryInfo::Get(thread->StackBase(), thread->StackSize());
      size_t privateSize = memInfo.Committed();
#endif

      threads.AppendElement(ThreadData{
        nsCString(PR_GetThreadName(thread->GetPRThread())),
        thread->ThreadId(),
        // On Linux, it's possible (but unlikely) that our stack region will
        // have been merged with adjacent heap regions, in which case we'll get
        // combined size information for both. So we take the minimum of the
        // reported private size and the requested stack size to avoid the
        // possible of majorly over-reporting in that case.
        std::min(privateSize, thread->StackSize()),
      });
    }

    for (auto& thread : threads) {
      nsPrintfCString path("explicit/thread-stacks/%s (tid=%u)",
                           thread.mName.get(), thread.mThreadId);

      aHandleReport->Callback(
          EmptyCString(), path,
          KIND_NONHEAP, UNITS_BYTES,
          thread.mPrivateSize,
          NS_LITERAL_CSTRING("The sizes of thread stacks which have been "
                             "committed to memory."),
          aData);
    }

    return NS_OK;
  }
};
NS_IMPL_ISUPPORTS(ThreadStacksReporter, nsIMemoryReporter)
#endif

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

  NS_IMETHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                            nsISupports* aData, bool aAnonymize) override
  {
    MOZ_COLLECT_REPORT(
      "explicit/deadlock-detector", KIND_HEAP, UNITS_BYTES,
      BlockingResourceBase::SizeOfDeadlockDetector(MallocSizeOf),
      "Memory used by the deadlock detector.");

    return NS_OK;
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

    MOZ_COLLECT_REPORT(
      "explicit/dmd/stack-traces/used", KIND_HEAP, UNITS_BYTES,
      sizes.mStackTracesUsed,
      "Memory used by stack traces which correspond to at least "
      "one heap block DMD is tracking.");

    MOZ_COLLECT_REPORT(
      "explicit/dmd/stack-traces/unused", KIND_HEAP, UNITS_BYTES,
      sizes.mStackTracesUnused,
      "Memory used by stack traces which don't correspond to any heap "
      "blocks DMD is currently tracking.");

    MOZ_COLLECT_REPORT(
      "explicit/dmd/stack-traces/table", KIND_HEAP, UNITS_BYTES,
      sizes.mStackTraceTable,
      "Memory used by DMD's stack trace table.");

    MOZ_COLLECT_REPORT(
      "explicit/dmd/live-block-table", KIND_HEAP, UNITS_BYTES,
      sizes.mLiveBlockTable,
      "Memory used by DMD's live block table.");

    MOZ_COLLECT_REPORT(
      "explicit/dmd/dead-block-list", KIND_HEAP, UNITS_BYTES,
      sizes.mDeadBlockTable,
      "Memory used by DMD's dead block list.");

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

NS_IMPL_ISUPPORTS(nsMemoryReporterManager, nsIMemoryReporterManager, nsIMemoryReporter)

NS_IMETHODIMP
nsMemoryReporterManager::Init()
{
  if (!NS_IsMainThread()) {
    MOZ_CRASH();
  }

  // Under normal circumstances this function is only called once. However,
  // we've (infrequently) seen memory report dumps in crash reports that
  // suggest that this function is sometimes called multiple times. That in
  // turn means that multiple reporters of each kind are registered, which
  // leads to duplicated reports of individual measurements such as "resident",
  // "vsize", etc.
  //
  // It's unclear how these multiple calls can occur. The only plausible theory
  // so far is badly-written extensions, because this function is callable from
  // JS code via nsIMemoryReporter.idl.
  //
  // Whatever the cause, it's a bad thing. So we protect against it with the
  // following check.
  static bool isInited = false;
  if (isInited) {
    NS_WARNING("nsMemoryReporterManager::Init() has already been called!");
    return NS_OK;
  }
  isInited = true;

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

#if defined(XP_LINUX) || defined(XP_WIN)
  RegisterStrongReporter(new ThreadStacksReporter());
#endif

#ifdef DEBUG
  RegisterStrongReporter(new DeadlockDetectorReporter());
#endif

#ifdef MOZ_GECKO_PROFILER
  // We have to register this here rather than in profiler_init() because
  // profiler_init() runs prior to nsMemoryReporterManager's creation.
  RegisterStrongReporter(new GeckoProfilerReporter());
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

  // Report our own memory usage as well.
  RegisterWeakReporter(this);

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
  , mPendingProcessesState(nullptr)
  , mPendingReportersState(nullptr)
#ifdef HAVE_JEMALLOC_STATS
  , mThreadPool(do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID))
#endif
{
}

nsMemoryReporterManager::~nsMemoryReporterManager()
{
  delete mStrongReporters;
  delete mWeakReporters;
  NS_ASSERTION(!mSavedStrongReporters, "failed to restore strong reporters");
  NS_ASSERTION(!mSavedWeakReporters, "failed to restore weak reporters");
}

NS_IMETHODIMP
nsMemoryReporterManager::CollectReports(nsIHandleReportCallback* aHandleReport,
                                        nsISupports* aData, bool aAnonymize)
{
  size_t n = MallocSizeOf(this);
  n += mStrongReporters->ShallowSizeOfIncludingThis(MallocSizeOf);
  n += mWeakReporters->ShallowSizeOfIncludingThis(MallocSizeOf);

  MOZ_COLLECT_REPORT(
    "explicit/memory-reporter-manager", KIND_HEAP, UNITS_BYTES,
    n,
    "Memory used by the memory reporter infrastructure.");

  return NS_OK;
}

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

  if (mPendingProcessesState) {
    // A request is in flight.  Don't start another one.  And don't report
    // an error;  just ignore it, and let the in-flight request finish.
    MEMORY_REPORTING_LOG("GetReports (gen=%u, s->gen=%u): abort\n",
                         generation, mPendingProcessesState->mGeneration);
    return NS_OK;
  }

  MEMORY_REPORTING_LOG("GetReports (gen=%u)\n", generation);

  uint32_t concurrency = Preferences::GetUint("memory.report_concurrency", 1);
  MOZ_ASSERT(concurrency >= 1);
  if (concurrency < 1) {
    concurrency = 1;
  }
  mPendingProcessesState = new PendingProcessesState(generation,
                                                     aAnonymize,
                                                     aMinimize,
                                                     concurrency,
                                                     aHandleReport,
                                                     aHandleReportData,
                                                     aFinishReporting,
                                                     aFinishReportingData,
                                                     aDMDDumpIdent);

  if (aMinimize) {
    nsCOMPtr<nsIRunnable> callback =
      NewRunnableMethod("nsMemoryReporterManager::StartGettingReports",
                        this,
                        &nsMemoryReporterManager::StartGettingReports);
    rv = MinimizeMemoryUsage(callback);
  } else {
    rv = StartGettingReports();
  }
  return rv;
}

nsresult
nsMemoryReporterManager::StartGettingReports()
{
  PendingProcessesState* s = mPendingProcessesState;
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

  // This is async.
  GetReportsForThisProcessExtended(s->mHandleReport, s->mHandleReportData,
                                   s->mAnonymize, parentDMDFile,
                                   s->mFinishReporting, s->mFinishReportingData);

  nsTArray<dom::ContentParent*> childWeakRefs;
  dom::ContentParent::GetAll(childWeakRefs);
  if (!childWeakRefs.IsEmpty()) {
    // Request memory reports from child processes.  This happens
    // after the parent report so that the parent's main thread will
    // be free to process the child reports, instead of causing them
    // to be buffered and consume (possibly scarce) memory.

    for (size_t i = 0; i < childWeakRefs.Length(); ++i) {
      s->mChildrenPending.AppendElement(childWeakRefs[i]);
    }
  }

  if (gfx::GPUProcessManager* gpu = gfx::GPUProcessManager::Get()) {
    if (RefPtr<MemoryReportingProcess> proc = gpu->GetProcessMemoryReporter()) {
      s->mChildrenPending.AppendElement(proc.forget());
    }
  }

  if (!s->mChildrenPending.IsEmpty()) {
    nsCOMPtr<nsITimer> timer;
    rv = NS_NewTimerWithFuncCallback(
      getter_AddRefs(timer),
      TimeoutCallback,
      this,
      kTimeoutLengthMS,
      nsITimer::TYPE_ONE_SHOT,
      "nsMemoryReporterManager::StartGettingReports");
    if (NS_WARN_IF(NS_FAILED(rv))) {
      FinishReporting();
      return rv;
    }

    MOZ_ASSERT(!s->mTimer);
    s->mTimer.swap(timer);
  }

  return NS_OK;
}

void
nsMemoryReporterManager::DispatchReporter(
  nsIMemoryReporter* aReporter, bool aIsAsync,
  nsIHandleReportCallback* aHandleReport,
  nsISupports* aHandleReportData,
  bool aAnonymize)
{
  MOZ_ASSERT(mPendingReportersState);

  // Grab refs to everything used in the lambda function.
  RefPtr<nsMemoryReporterManager> self = this;
  nsCOMPtr<nsIMemoryReporter> reporter = aReporter;
  nsCOMPtr<nsIHandleReportCallback> handleReport = aHandleReport;
  nsCOMPtr<nsISupports> handleReportData = aHandleReportData;

  nsCOMPtr<nsIRunnable> event = NS_NewRunnableFunction(
    "nsMemoryReporterManager::DispatchReporter",
    [self, reporter, aIsAsync, handleReport, handleReportData, aAnonymize]() {
      reporter->CollectReports(handleReport, handleReportData, aAnonymize);
      if (!aIsAsync) {
        self->EndReport();
      }
    });

  NS_DispatchToMainThread(event);
  mPendingReportersState->mReportsPending++;
}

NS_IMETHODIMP
nsMemoryReporterManager::GetReportsForThisProcessExtended(
  nsIHandleReportCallback* aHandleReport, nsISupports* aHandleReportData,
  bool aAnonymize, FILE* aDMDFile,
  nsIFinishReportingCallback* aFinishReporting,
  nsISupports* aFinishReportingData)
{
  // Memory reporters are not necessarily threadsafe, so this function must
  // be called from the main thread.
  if (!NS_IsMainThread()) {
    MOZ_CRASH();
  }

  if (NS_WARN_IF(mPendingReportersState)) {
    // Report is already in progress.
    return NS_ERROR_IN_PROGRESS;
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

  mPendingReportersState = new PendingReportersState(
      aFinishReporting, aFinishReportingData, aDMDFile);

  {
    mozilla::MutexAutoLock autoLock(mMutex);

    for (auto iter = mStrongReporters->Iter(); !iter.Done(); iter.Next()) {
      DispatchReporter(iter.Key(), iter.Data(),
                       aHandleReport, aHandleReportData, aAnonymize);
    }

    for (auto iter = mWeakReporters->Iter(); !iter.Done(); iter.Next()) {
      nsCOMPtr<nsIMemoryReporter> reporter = iter.Key();
      DispatchReporter(reporter, iter.Data(),
                       aHandleReport, aHandleReportData, aAnonymize);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMemoryReporterManager::EndReport()
{
  if (--mPendingReportersState->mReportsPending == 0) {
#ifdef MOZ_DMD
    if (mPendingReportersState->mDMDFile) {
      nsMemoryInfoDumper::DumpDMDToFile(mPendingReportersState->mDMDFile);
    }
#endif
    if (mPendingProcessesState) {
      // This is the parent process.
      EndProcessReport(mPendingProcessesState->mGeneration, true);
    } else {
      mPendingReportersState->mFinishReporting->Callback(
          mPendingReportersState->mFinishReportingData);
    }

    delete mPendingReportersState;
    mPendingReportersState = nullptr;
  }

  return NS_OK;
}

nsMemoryReporterManager::PendingProcessesState*
nsMemoryReporterManager::GetStateForGeneration(uint32_t aGeneration)
{
  // Memory reporting only happens on the main thread.
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  PendingProcessesState* s = mPendingProcessesState;

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
  PendingProcessesState* s = GetStateForGeneration(aGeneration);
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
nsMemoryReporterManager::StartChildReport(mozilla::MemoryReportingProcess* aChild,
                                          const PendingProcessesState* aState)
{
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
  return aChild->SendRequestMemoryReport(
    aState->mGeneration, aState->mAnonymize, aState->mMinimize, dmdFileDesc);
}

void
nsMemoryReporterManager::EndProcessReport(uint32_t aGeneration, bool aSuccess)
{
  PendingProcessesState* s = GetStateForGeneration(aGeneration);
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
    RefPtr<MemoryReportingProcess> nextChild;
    nextChild.swap(s->mChildrenPending.LastElement());
    s->mChildrenPending.TruncateLength(s->mChildrenPending.Length() - 1);
    // Start report (if the child is still alive).
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
  PendingProcessesState* s = mgr->mPendingProcessesState;

  // Release assert because: if the pointer is null we're about to
  // crash regardless of DEBUG, and this way the compiler doesn't
  // complain about unused variables.
  MOZ_RELEASE_ASSERT(s, "mgr->mPendingProcessesState");
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

  MOZ_ASSERT(mPendingProcessesState);
  MEMORY_REPORTING_LOG("FinishReporting (s->gen=%u; %u processes reported)\n",
                       mPendingProcessesState->mGeneration,
                       mPendingProcessesState->mNumProcessesCompleted);

  // Call this before deleting |mPendingProcessesState|.  That way, if
  // |mFinishReportData| calls GetReports(), it will silently abort, as
  // required.
  nsresult rv = mPendingProcessesState->mFinishReporting->Callback(
    mPendingProcessesState->mFinishReportingData);

  delete mPendingProcessesState;
  mPendingProcessesState = nullptr;
  return rv;
}

nsMemoryReporterManager::PendingProcessesState::PendingProcessesState(
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
  nsIMemoryReporter* aReporter, bool aForce, bool aStrong, bool aIsAsync)
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
    mStrongReporters->Put(aReporter, aIsAsync);
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
    mWeakReporters->Put(aReporter, aIsAsync);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMemoryReporterManager::RegisterStrongReporter(nsIMemoryReporter* aReporter)
{
  return RegisterReporterHelper(aReporter, /* force = */ false,
                                /* strong = */ true,
                                /* async = */ false);
}

NS_IMETHODIMP
nsMemoryReporterManager::RegisterStrongAsyncReporter(nsIMemoryReporter* aReporter)
{
  return RegisterReporterHelper(aReporter, /* force = */ false,
                                /* strong = */ true,
                                /* async = */ true);
}

NS_IMETHODIMP
nsMemoryReporterManager::RegisterWeakReporter(nsIMemoryReporter* aReporter)
{
  return RegisterReporterHelper(aReporter, /* force = */ false,
                                /* strong = */ false,
                                /* async = */ false);
}

NS_IMETHODIMP
nsMemoryReporterManager::RegisterWeakAsyncReporter(nsIMemoryReporter* aReporter)
{
  return RegisterReporterHelper(aReporter, /* force = */ false,
                                /* strong = */ false,
                                /* async = */ true);
}

NS_IMETHODIMP
nsMemoryReporterManager::RegisterStrongReporterEvenIfBlocked(
  nsIMemoryReporter* aReporter)
{
  return RegisterReporterHelper(aReporter, /* force = */ true,
                                /* strong = */ true,
                                /* async = */ false);
}

NS_IMETHODIMP
nsMemoryReporterManager::UnregisterStrongReporter(nsIMemoryReporter* aReporter)
{
  // This method is thread-safe.
  mozilla::MutexAutoLock autoLock(mMutex);

  MOZ_ASSERT(!mWeakReporters->Contains(aReporter));

  if (mStrongReporters->Contains(aReporter)) {
    mStrongReporters->Remove(aReporter);
    return NS_OK;
  }

  // We don't register new reporters when the block is in place, but we do
  // unregister existing reporters. This is so we don't keep holding strong
  // references that these reporters aren't expecting (which can keep them
  // alive longer than intended).
  if (mSavedStrongReporters && mSavedStrongReporters->Contains(aReporter)) {
    mSavedStrongReporters->Remove(aReporter);
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
    mWeakReporters->Remove(aReporter);
    return NS_OK;
  }

  // We don't register new reporters when the block is in place, but we do
  // unregister existing reporters. This is so we don't keep holding weak
  // references that the old reporters aren't expecting (which can end up as
  // dangling pointers that lead to use-after-frees).
  if (mSavedWeakReporters && mSavedWeakReporters->Contains(aReporter)) {
    mSavedWeakReporters->Remove(aReporter);
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

NS_IMETHODIMP
nsMemoryReporterManager::GetHeapAllocatedAsync(nsIHeapAllocatedCallback *aCallback)
{
#ifdef HAVE_JEMALLOC_STATS
  if (!mThreadPool) {
    return NS_ERROR_UNEXPECTED;
  }

  RefPtr<nsIMemoryReporterManager> self{this};
  nsMainThreadPtrHandle<nsIHeapAllocatedCallback> mainThreadCallback(
    new nsMainThreadPtrHolder<nsIHeapAllocatedCallback>("HeapAllocatedCallback",
                                                        aCallback));

  nsCOMPtr<nsIRunnable> getHeapAllocatedRunnable = NS_NewRunnableFunction(
    "nsMemoryReporterManager::GetHeapAllocatedAsync",
    [self, mainThreadCallback]() mutable {
      MOZ_ASSERT(!NS_IsMainThread());

      int64_t heapAllocated = 0;
      nsresult rv = self->GetHeapAllocated(&heapAllocated);

      nsCOMPtr<nsIRunnable> resultCallbackRunnable = NS_NewRunnableFunction(
        "nsMemoryReporterManager::GetHeapAllocatedAsync",
        [mainThreadCallback, heapAllocated, rv]() mutable {
          MOZ_ASSERT(NS_IsMainThread());

          if (NS_FAILED(rv)) {
            mainThreadCallback->Callback(0);
            return;
          }

          mainThreadCallback->Callback(heapAllocated);
        });  // resultCallbackRunnable.

      Unused << NS_DispatchToMainThread(resultCallbackRunnable);
    }); // getHeapAllocatedRunnable.

  return mThreadPool->Dispatch(getHeapAllocatedRunnable, NS_DISPATCH_NORMAL);
#else
  return NS_ERROR_NOT_AVAILABLE;
#endif
}

// This has UNITS_PERCENTAGE, so it is multiplied by 100x.
NS_IMETHODIMP
nsMemoryReporterManager::GetHeapOverheadFraction(int64_t* aAmount)
{
#ifdef HAVE_JEMALLOC_STATS
  jemalloc_stats_t stats;
  jemalloc_stats(&stats);
  *aAmount = HeapOverheadFraction(&stats);
  return NS_OK;
#else
  *aAmount = 0;
  return NS_ERROR_NOT_AVAILABLE;
#endif
}

static MOZ_MUST_USE nsresult
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
nsMemoryReporterManager::GetJSMainRuntimeRealmsSystem(int64_t* aAmount)
{
  return GetInfallibleAmount(mAmountFns.mJSMainRuntimeRealmsSystem,
                             aAmount);
}

NS_IMETHODIMP
nsMemoryReporterManager::GetJSMainRuntimeRealmsUser(int64_t* aAmount)
{
  return GetInfallibleAmount(mAmountFns.mJSMainRuntimeRealmsUser,
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
nsMemoryReporterManager::GetLowMemoryEventsCommitSpace(int64_t* aAmount)
{
  return GetInfallibleAmount(mAmountFns.mLowMemoryEventsCommitSpace, aAmount);
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
class MinimizeMemoryUsageRunnable : public Runnable
{
public:
  explicit MinimizeMemoryUsageRunnable(nsIRunnable* aCallback)
    : mozilla::Runnable("MinimizeMemoryUsageRunnable")
    , mCallback(aCallback)
    , mRemainingIters(sNumIters)
  {
  }

  NS_IMETHOD Run() override
  {
    nsCOMPtr<nsIObserverService> os = services::GetObserverService();
    if (!os) {
      return NS_ERROR_FAILURE;
    }

    if (mRemainingIters == 0) {
      os->NotifyObservers(nullptr, "after-minimize-memory-usage",
                          u"MinimizeMemoryUsageRunnable");
      if (mCallback) {
        mCallback->Run();
      }
      return NS_OK;
    }

    os->NotifyObservers(nullptr, "memory-pressure", u"heap-minimize");
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
  RefPtr<MinimizeMemoryUsageRunnable> runnable =
    new MinimizeMemoryUsageRunnable(aCallback);

  return NS_DispatchToMainThread(runnable);
}

NS_IMETHODIMP
nsMemoryReporterManager::SizeOfTab(mozIDOMWindowProxy* aTopWindow,
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
  auto* piWindow = nsPIDOMWindowOuter::From(aTopWindow);
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
  rv = mSizeOfTabFns.mNonJS(piWindow, &domSize, &styleSize, &otherSize);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

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

#define GET_MEMORY_REPORTER_MANAGER(mgr)                                      \
  RefPtr<nsMemoryReporterManager> mgr =                                       \
    nsMemoryReporterManager::GetOrCreate();                                   \
  if (!mgr) {                                                                 \
    return NS_ERROR_FAILURE;                                                  \
  }

nsresult
RegisterStrongMemoryReporter(nsIMemoryReporter* aReporter)
{
  // Hold a strong reference to the argument to make sure it gets released if
  // we return early below.
  nsCOMPtr<nsIMemoryReporter> reporter = aReporter;
  GET_MEMORY_REPORTER_MANAGER(mgr)
  return mgr->RegisterStrongReporter(reporter);
}

nsresult
RegisterStrongAsyncMemoryReporter(nsIMemoryReporter* aReporter)
{
  // Hold a strong reference to the argument to make sure it gets released if
  // we return early below.
  nsCOMPtr<nsIMemoryReporter> reporter = aReporter;
  GET_MEMORY_REPORTER_MANAGER(mgr)
  return mgr->RegisterStrongAsyncReporter(reporter);
}

nsresult
RegisterWeakMemoryReporter(nsIMemoryReporter* aReporter)
{
  GET_MEMORY_REPORTER_MANAGER(mgr)
  return mgr->RegisterWeakReporter(aReporter);
}

nsresult
RegisterWeakAsyncMemoryReporter(nsIMemoryReporter* aReporter)
{
  GET_MEMORY_REPORTER_MANAGER(mgr)
  return mgr->RegisterWeakAsyncReporter(aReporter);
}

nsresult
UnregisterStrongMemoryReporter(nsIMemoryReporter* aReporter)
{
  GET_MEMORY_REPORTER_MANAGER(mgr)
  return mgr->UnregisterStrongReporter(aReporter);
}

nsresult
UnregisterWeakMemoryReporter(nsIMemoryReporter* aReporter)
{
  GET_MEMORY_REPORTER_MANAGER(mgr)
  return mgr->UnregisterWeakReporter(aReporter);
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

// Macro for generating functions that unregister distinguished amount
// functions with the memory reporter manager.
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
DEFINE_REGISTER_DISTINGUISHED_AMOUNT(Infallible, JSMainRuntimeRealmsSystem)
DEFINE_REGISTER_DISTINGUISHED_AMOUNT(Infallible, JSMainRuntimeRealmsUser)

DEFINE_REGISTER_DISTINGUISHED_AMOUNT(Infallible, ImagesContentUsedUncompressed)
DEFINE_UNREGISTER_DISTINGUISHED_AMOUNT(ImagesContentUsedUncompressed)

DEFINE_REGISTER_DISTINGUISHED_AMOUNT(Infallible, StorageSQLite)
DEFINE_UNREGISTER_DISTINGUISHED_AMOUNT(StorageSQLite)

DEFINE_REGISTER_DISTINGUISHED_AMOUNT(Infallible, LowMemoryEventsVirtual)
DEFINE_REGISTER_DISTINGUISHED_AMOUNT(Infallible, LowMemoryEventsCommitSpace)
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
