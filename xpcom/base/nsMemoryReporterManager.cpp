/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAtomTable.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsServiceManagerUtils.h"
#include "nsMemoryReporterManager.h"
#include "nsISimpleEnumerator.h"
#include "nsThreadUtils.h"
#include "nsIDOMWindow.h"
#include "nsPIDOMWindow.h"
#include "nsIObserverService.h"
#include "nsIGlobalObject.h"
#if defined(XP_LINUX)
#include "nsMemoryInfoDumper.h"
#endif
#include "mozilla/Attributes.h"
#include "mozilla/PodOperations.h"
#include "mozilla/Services.h"
#include "mozilla/Telemetry.h"

#ifndef XP_WIN
#include <unistd.h>
#endif

using namespace mozilla;

#if defined(MOZ_MEMORY)
#  define HAVE_JEMALLOC_STATS 1
#  include "mozmemory.h"
#endif  // MOZ_MEMORY

#if defined(XP_LINUX)

static nsresult GetProcSelfStatmField(int aField, int64_t* aN)
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

#define HAVE_VSIZE_AND_RESIDENT_REPORTERS 1
static nsresult GetVsize(int64_t* aN)
{
    return GetProcSelfStatmField(0, aN);
}

static nsresult GetResident(int64_t* aN)
{
    return GetProcSelfStatmField(1, aN);
}

static nsresult GetResidentFast(int64_t* aN)
{
    return GetResident(aN);
}

#define HAVE_RESIDENT_UNIQUE_REPORTER
class ResidentUniqueReporter MOZ_FINAL : public MemoryUniReporter
{
public:
  ResidentUniqueReporter()
    : MemoryUniReporter("resident-unique", KIND_OTHER, UNITS_BYTES,
"Memory mapped by the process that is present in physical memory and not "
"shared with any other processes.  This is also known as the process's unique "
"set size (USS).  This is the amount of RAM we'd expect to be freed if we "
"closed this process.")
  {}

private:
  NS_IMETHOD GetAmount(int64_t *aAmount)
  {
    // You might be tempted to calculate USS by subtracting the "shared" value
    // from the "resident" value in /proc/<pid>/statm.  But at least on Linux,
    // statm's "shared" value actually counts pages backed by files, which has
    // little to do with whether the pages are actually shared.
    // /proc/self/smaps on the other hand appears to give us the correct
    // information.

    *aAmount = 0;

    FILE *f = fopen("/proc/self/smaps", "r");
    NS_ENSURE_STATE(f);

    int64_t total = 0;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
      long long val = 0;
      if (sscanf(line, "Private_Dirty: %lld kB", &val) == 1 ||
          sscanf(line, "Private_Clean: %lld kB", &val) == 1) {
        total += val * 1024; // convert from kB to bytes
      }
    }
    *aAmount = total;

    fclose(f);
    return NS_OK;
  }
};

#elif defined(__DragonFly__) || defined(__FreeBSD__) \
    || defined(__NetBSD__) || defined(__OpenBSD__)

#include <sys/param.h>
#include <sys/sysctl.h>
#if defined(__DragonFly__) || defined(__FreeBSD__)
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
#elif defined(__FreeBSD__)
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

static nsresult GetKinfoProcSelf(KINFO_PROC* aProc)
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
    if (sysctl(mib, miblen, aProc, &size, nullptr, 0))
        return NS_ERROR_FAILURE;

    return NS_OK;
}

#define HAVE_VSIZE_AND_RESIDENT_REPORTERS 1
static nsresult GetVsize(int64_t* aN)
{
    KINFO_PROC proc;
    nsresult rv = GetKinfoProcSelf(&proc);
    if (NS_SUCCEEDED(rv))
        *aN = KP_SIZE(proc);

    return rv;
}

static nsresult GetResident(int64_t* aN)
{
    KINFO_PROC proc;
    nsresult rv = GetKinfoProcSelf(&proc);
    if (NS_SUCCEEDED(rv))
        *aN = KP_RSS(proc);

    return rv;
}

static nsresult GetResidentFast(int64_t* aN)
{
    return GetResident(aN);
}

#elif defined(SOLARIS)

#include <procfs.h>
#include <fcntl.h>
#include <unistd.h>

static void XMappingIter(int64_t& vsize, int64_t& resident)
{
    vsize = -1;
    resident = -1;
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
                if (nmap >= n / sizeof (prxmap_t)) {
                    vsize = 0;
                    resident = 0;
                    for (int i = 0; i < n / sizeof (prxmap_t); i++) {
                        vsize += prmapp[i].pr_size;
                        resident += prmapp[i].pr_rss * prmapp[i].pr_pagesize;
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
static nsresult GetVsize(int64_t* aN)
{
    int64_t vsize, resident;
    XMappingIter(vsize, resident);
    if (vsize == -1) {
        return NS_ERROR_FAILURE;
    }
    *aN = vsize;
    return NS_OK;
}

static nsresult GetResident(int64_t* aN)
{
    int64_t vsize, resident;
    XMappingIter(vsize, resident);
    if (resident == -1) {
        return NS_ERROR_FAILURE;
    }
    *aN = resident;
    return NS_OK;
}

static nsresult GetResidentFast(int64_t* aN)
{
    return GetResident(aN);
}

#elif defined(XP_MACOSX)

#include <mach/mach_init.h>
#include <mach/task.h>

static bool GetTaskBasicInfo(struct task_basic_info* aTi)
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
static nsresult GetVsize(int64_t* aN)
{
    task_basic_info ti;
    if (!GetTaskBasicInfo(&ti))
        return NS_ERROR_FAILURE;

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
static nsresult GetResident(int64_t* aN, bool aDoPurge)
{
#ifdef HAVE_JEMALLOC_STATS
    if (aDoPurge) {
      Telemetry::AutoTimer<Telemetry::MEMORY_FREE_PURGED_PAGES_MS> timer;
      jemalloc_purge_freed_pages();
    }
#endif

    task_basic_info ti;
    if (!GetTaskBasicInfo(&ti))
        return NS_ERROR_FAILURE;

    *aN = ti.resident_size;
    return NS_OK;
}

static nsresult GetResidentFast(int64_t* aN)
{
    return GetResident(aN, /* doPurge = */ false);
}

static nsresult GetResident(int64_t* aN)
{
    return GetResident(aN, /* doPurge = */ true);
}

#elif defined(XP_WIN)

#include <windows.h>
#include <psapi.h>

#define HAVE_VSIZE_AND_RESIDENT_REPORTERS 1
static nsresult GetVsize(int64_t* aN)
{
    MEMORYSTATUSEX s;
    s.dwLength = sizeof(s);

    if (!GlobalMemoryStatusEx(&s)) {
        return NS_ERROR_FAILURE;
    }

    *aN = s.ullTotalVirtual - s.ullAvailVirtual;
    return NS_OK;
}

static nsresult GetResident(int64_t* aN)
{
    PROCESS_MEMORY_COUNTERS pmc;
    pmc.cb = sizeof(PROCESS_MEMORY_COUNTERS);

    if (!GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return NS_ERROR_FAILURE;
    }

    *aN = pmc.WorkingSetSize;
    return NS_OK;
}

static nsresult GetResidentFast(int64_t* aN)
{
    return GetResident(aN);
}

#define HAVE_PRIVATE_REPORTER
class PrivateReporter MOZ_FINAL : public MemoryUniReporter
{
public:
    PrivateReporter()
      : MemoryUniReporter("private", KIND_OTHER, UNITS_BYTES,
"Memory that cannot be shared with other processes, including memory that is "
"committed and marked MEM_PRIVATE, data that is not mapped, and executable "
"pages that have been written to.")
    {}

    NS_IMETHOD GetAmount(int64_t* aAmount)
    {
        PROCESS_MEMORY_COUNTERS_EX pmcex;
        pmcex.cb = sizeof(PROCESS_MEMORY_COUNTERS_EX);

        if (!GetProcessMemoryInfo(
                GetCurrentProcess(),
                (PPROCESS_MEMORY_COUNTERS) &pmcex, sizeof(pmcex))) {
            return NS_ERROR_FAILURE;
        }

        *aAmount = pmcex.PrivateUsage;
        return NS_OK;
    }
};

#endif  // XP_<PLATFORM>

#ifdef HAVE_VSIZE_AND_RESIDENT_REPORTERS
class VsizeReporter MOZ_FINAL : public MemoryUniReporter
{
public:
    VsizeReporter()
      : MemoryUniReporter("vsize", KIND_OTHER, UNITS_BYTES,
"Memory mapped by the process, including code and data segments, the heap, "
"thread stacks, memory explicitly mapped by the process via mmap and similar "
"operations, and memory shared with other processes. This is the vsize figure "
"as reported by 'top' and 'ps'.  This figure is of limited use on Mac, where "
"processes share huge amounts of memory with one another.  But even on other "
"operating systems, 'resident' is a much better measure of the memory "
"resources used by the process.")
    {}

    NS_IMETHOD GetAmount(int64_t* aAmount) { return GetVsize(aAmount); }
};

class ResidentReporter MOZ_FINAL : public MemoryUniReporter
{
public:
    ResidentReporter()
      : MemoryUniReporter("resident", KIND_OTHER, UNITS_BYTES,
"Memory mapped by the process that is present in physical memory, also known "
"as the resident set size (RSS).  This is the best single figure to use when "
"considering the memory resources used by the process, but it depends both on "
"other processes being run and details of the OS kernel and so is best used "
"for comparing the memory usage of a single process at different points in "
"time.")
    {}

    NS_IMETHOD GetAmount(int64_t* aAmount) { return GetResident(aAmount); }
};
#endif  // HAVE_VSIZE_AND_RESIDENT_REPORTERS

#ifdef XP_UNIX

#include <sys/resource.h>

#define HAVE_PAGE_FAULT_REPORTERS 1

class PageFaultsSoftReporter MOZ_FINAL : public MemoryUniReporter
{
public:
    PageFaultsSoftReporter()
      : MemoryUniReporter("page-faults-soft", KIND_OTHER,
                           UNITS_COUNT_CUMULATIVE,
"The number of soft page faults (also known as 'minor page faults') that "
"have occurred since the process started.  A soft page fault occurs when the "
"process tries to access a page which is present in physical memory but is "
"not mapped into the process's address space.  For instance, a process might "
"observe soft page faults when it loads a shared library which is already "
"present in physical memory. A process may experience many thousands of soft "
"page faults even when the machine has plenty of available physical memory, "
"and because the OS services a soft page fault without accessing the disk, "
"they impact performance much less than hard page faults.")
    {}

    NS_IMETHOD GetAmount(int64_t* aAmount)
    {
        struct rusage usage;
        int err = getrusage(RUSAGE_SELF, &usage);
        if (err != 0) {
            return NS_ERROR_FAILURE;
        }
        *aAmount = usage.ru_minflt;
        return NS_OK;
    }
};

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

class PageFaultsHardReporter MOZ_FINAL : public MemoryUniReporter
{
public:
    PageFaultsHardReporter()
      : MemoryUniReporter("page-faults-hard", KIND_OTHER,
                           UNITS_COUNT_CUMULATIVE,
"The number of hard page faults (also known as 'major page faults') that have "
"occurred since the process started.  A hard page fault occurs when a process "
"tries to access a page which is not present in physical memory. The "
"operating system must access the disk in order to fulfill a hard page fault. "
"When memory is plentiful, you should see very few hard page faults. But if "
"the process tries to use more memory than your machine has available, you "
"may see many thousands of hard page faults. Because accessing the disk is up "
"to a million times slower than accessing RAM, the program may run very "
"slowly when it is experiencing more than 100 or so hard page faults a second.")
    {}

    NS_IMETHOD GetAmount(int64_t* aAmount)
    {
        return PageFaultsHardDistinguishedAmount(aAmount);
    }
};
#endif  // HAVE_PAGE_FAULT_REPORTERS

/**
 ** memory reporter implementation for jemalloc and OSX malloc,
 ** to obtain info on total memory in use (that we know about,
 ** at least -- on OSX, there are sometimes other zones in use).
 **/

#ifdef HAVE_JEMALLOC_STATS

static int64_t
HeapAllocated()
{
    jemalloc_stats_t stats;
    jemalloc_stats(&stats);
    return (int64_t) stats.allocated;
}

// This has UNITS_PERCENTAGE, so it is multiplied by 100x.
static int64_t
HeapOverheadRatio()
{
    jemalloc_stats_t stats;
    jemalloc_stats(&stats);
    return (int64_t) 10000 *
      (stats.waste + stats.bookkeeping + stats.page_cache) /
      ((double)stats.allocated);
}

class HeapAllocatedReporter MOZ_FINAL : public MemoryUniReporter
{
public:
    HeapAllocatedReporter()
      : MemoryUniReporter("heap-allocated", KIND_OTHER, UNITS_BYTES,
"Memory mapped by the heap allocator that is currently allocated to the "
"application.  This may exceed the amount of memory requested by the "
"application because the allocator regularly rounds up request sizes. (The "
"exact amount requested is not recorded.)")
    {}
private:
    int64_t Amount() MOZ_OVERRIDE { return HeapAllocated(); }
};

class HeapOverheadWasteReporter MOZ_FINAL : public MemoryUniReporter
{
public:
    // We mark this and the other heap-overhead reporters as KIND_NONHEAP
    // because KIND_HEAP memory means "counted in heap-allocated", which this
    // is not.
    HeapOverheadWasteReporter()
      : MemoryUniReporter("explicit/heap-overhead/waste",
                           KIND_NONHEAP, UNITS_BYTES,
"Committed bytes which do not correspond to an active allocation and which the "
"allocator is not intentionally keeping alive (i.e., not 'heap-bookkeeping' or "
"'heap-page-cache').  Although the allocator will waste some space under any "
"circumstances, a large value here may indicate that the heap is highly "
"fragmented, or that allocator is performing poorly for some other reason.")
    {}
private:
    int64_t Amount() MOZ_OVERRIDE
    {
        jemalloc_stats_t stats;
        jemalloc_stats(&stats);
        return stats.waste;
    }
};

class HeapOverheadBookkeepingReporter MOZ_FINAL : public MemoryUniReporter
{
public:
    HeapOverheadBookkeepingReporter()
      : MemoryUniReporter("explicit/heap-overhead/bookkeeping",
                           KIND_NONHEAP, UNITS_BYTES,
"Committed bytes which the heap allocator uses for internal data structures.")
    {}
private:
    int64_t Amount() MOZ_OVERRIDE
    {
        jemalloc_stats_t stats;
        jemalloc_stats(&stats);
        return stats.bookkeeping;
    }
};

class HeapOverheadPageCacheReporter MOZ_FINAL : public MemoryUniReporter
{
public:
    HeapOverheadPageCacheReporter()
      : MemoryUniReporter("explicit/heap-overhead/page-cache",
                           KIND_NONHEAP, UNITS_BYTES,
"Memory which the allocator could return to the operating system, but hasn't. "
"The allocator keeps this memory around as an optimization, so it doesn't "
"have to ask the OS the next time it needs to fulfill a request. This value "
"is typically not larger than a few megabytes.")
    {}
private:
    int64_t Amount() MOZ_OVERRIDE
    {
        jemalloc_stats_t stats;
        jemalloc_stats(&stats);
        return (int64_t) stats.page_cache;
    }
};

class HeapCommittedReporter MOZ_FINAL : public MemoryUniReporter
{
public:
    HeapCommittedReporter()
      : MemoryUniReporter("heap-committed", KIND_OTHER, UNITS_BYTES,
"Memory mapped by the heap allocator that is committed, i.e. in physical "
"memory or paged to disk.  This value corresponds to 'heap-allocated' + "
"'heap-waste' + 'heap-bookkeeping' + 'heap-page-cache', but because "
"these values are read at different times, the result probably won't match "
"exactly.")
    {}
private:
    int64_t Amount() MOZ_OVERRIDE
    {
        jemalloc_stats_t stats;
        jemalloc_stats(&stats);
        return (int64_t) (stats.allocated + stats.waste +
                          stats.bookkeeping + stats.page_cache);
    }
};

class HeapOverheadRatioReporter MOZ_FINAL : public MemoryUniReporter
{
public:
    HeapOverheadRatioReporter()
      : MemoryUniReporter("heap-overhead-ratio", KIND_OTHER,
                           UNITS_PERCENTAGE,
"Ratio of committed, unused bytes to allocated bytes; i.e., "
"'heap-overhead' / 'heap-allocated'.  This measures the overhead of "
"the heap allocator relative to amount of memory allocated.")
    {}
private:
    int64_t Amount() MOZ_OVERRIDE { return HeapOverheadRatio(); }
};
#endif  // HAVE_JEMALLOC_STATS

// Why is this here?  At first glance, you'd think it could be defined and
// registered with nsMemoryReporterManager entirely within nsAtomTable.cpp.
// However, the obvious time to register it is when the table is initialized,
// and that happens before XPCOM components are initialized, which means the
// NS_RegisterMemoryReporter call fails.  So instead we do it here.
class AtomTablesReporter MOZ_FINAL : public MemoryUniReporter
{
public:
    AtomTablesReporter()
      : MemoryUniReporter("explicit/atom-tables", KIND_HEAP, UNITS_BYTES,
"Memory used by the dynamic and static atoms tables.")
    {}
private:
    int64_t Amount() MOZ_OVERRIDE
    {
        return NS_SizeOfAtomTablesIncludingThis(MallocSizeOf);
    }
};

#ifdef MOZ_DMD

namespace mozilla {
namespace dmd {

class DMDReporter MOZ_FINAL : public nsIMemoryReporter
{
public:
  DMDReporter()
  {}

  NS_DECL_ISUPPORTS

  NS_IMETHOD GetName(nsACString& aName)
  {
    aName.Assign("dmd");
    return NS_OK;
  }

  NS_IMETHOD CollectReports(nsIMemoryReporterCallback* aCallback,
                            nsISupports* aClosure)
  {
    dmd::Sizes sizes;
    dmd::SizeOf(&sizes);

#define REPORT(_path, _amount, _desc)                                         \
    do {                                                                      \
      nsresult rv;                                                            \
      rv = aCallback->Callback(EmptyCString(), NS_LITERAL_CSTRING(_path),     \
                               nsIMemoryReporter::KIND_HEAP,                  \
                               nsIMemoryReporter::UNITS_BYTES, _amount,       \
                               NS_LITERAL_CSTRING(_desc), aClosure);          \
      NS_ENSURE_SUCCESS(rv, rv);                                              \
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

    REPORT("explicit/dmd/block-table",
           sizes.mBlockTable,
           "Memory used by DMD's live block table.");

#undef REPORT

    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS1(DMDReporter, nsIMemoryReporter)

} // namespace dmd
} // namespace mozilla

#endif  // MOZ_DMD

/**
 ** nsMemoryReporterManager implementation
 **/

NS_IMPL_ISUPPORTS1(nsMemoryReporterManager, nsIMemoryReporterManager)

NS_IMETHODIMP
nsMemoryReporterManager::Init()
{
#if defined(HAVE_JEMALLOC_STATS) && defined(XP_LINUX)
    if (!jemalloc_stats)
        return NS_ERROR_FAILURE;
#endif

#ifdef HAVE_JEMALLOC_STATS
    RegisterReporter(new HeapAllocatedReporter);
    RegisterReporter(new HeapOverheadWasteReporter);
    RegisterReporter(new HeapOverheadBookkeepingReporter);
    RegisterReporter(new HeapOverheadPageCacheReporter);
    RegisterReporter(new HeapCommittedReporter);
    RegisterReporter(new HeapOverheadRatioReporter);
#endif

#ifdef HAVE_VSIZE_AND_RESIDENT_REPORTERS
    RegisterReporter(new VsizeReporter);
    RegisterReporter(new ResidentReporter);
#endif

#ifdef HAVE_RESIDENT_UNIQUE_REPORTER
    RegisterReporter(new ResidentUniqueReporter);
#endif

#ifdef HAVE_PAGE_FAULT_REPORTERS
    RegisterReporter(new PageFaultsSoftReporter);
    RegisterReporter(new PageFaultsHardReporter);
#endif

#ifdef HAVE_PRIVATE_REPORTER
    RegisterReporter(new PrivateReporter);
#endif

    RegisterReporter(new AtomTablesReporter);

#ifdef MOZ_DMD
    RegisterReporter(new mozilla::dmd::DMDReporter);
#endif

#if defined(XP_LINUX)
    nsMemoryInfoDumper::Initialize();
#endif

    return NS_OK;
}

namespace {

/**
 * HastableEnumerator takes an nsTHashtable<nsISupportsHashKey>& in its
 * constructor and creates an nsISimpleEnumerator from its contents.
 *
 * The resultant enumerator works over a copy of the hashtable, so it's safe to
 * mutate or destroy the hashtable after the enumerator is created.
 */

class HashtableEnumerator MOZ_FINAL : public nsISimpleEnumerator
{
public:
    HashtableEnumerator(nsTHashtable<nsISupportsHashKey>& aHashtable)
        : mIndex(0)
    {
        aHashtable.EnumerateEntries(EnumeratorFunc, this);
    }

    NS_DECL_ISUPPORTS
    NS_DECL_NSISIMPLEENUMERATOR

private:
    static PLDHashOperator
    EnumeratorFunc(nsISupportsHashKey* aEntry, void* aData);

    uint32_t mIndex;
    nsCOMArray<nsISupports> mArray;
};

NS_IMPL_ISUPPORTS1(HashtableEnumerator, nsISimpleEnumerator)

/* static */ PLDHashOperator
HashtableEnumerator::EnumeratorFunc(nsISupportsHashKey* aElem, void* aData)
{
    HashtableEnumerator* enumerator = static_cast<HashtableEnumerator*>(aData);
    enumerator->mArray.AppendObject(aElem->GetKey());
    return PL_DHASH_NEXT;
}

NS_IMETHODIMP
HashtableEnumerator::HasMoreElements(bool* aResult)
{
    *aResult = mIndex < mArray.Length();
    return NS_OK;
}

NS_IMETHODIMP
HashtableEnumerator::GetNext(nsISupports** aNext)
{
    if (mIndex < mArray.Length()) {
        nsCOMPtr<nsISupports> next = mArray.ObjectAt(mIndex);
        next.forget(aNext);
        mIndex++;
        return NS_OK;
    }

    *aNext = nullptr;
    return NS_ERROR_FAILURE;
}

} // anonymous namespace

nsMemoryReporterManager::nsMemoryReporterManager()
  : mMutex("nsMemoryReporterManager::mMutex"),
    mIsRegistrationBlocked(false)
{
    PodZero(&mAmountFns);
    PodZero(&mSizeOfTabFns);
}

nsMemoryReporterManager::~nsMemoryReporterManager()
{
}

NS_IMETHODIMP
nsMemoryReporterManager::EnumerateReporters(nsISimpleEnumerator** aResult)
{
    // Memory reporters are not necessarily threadsafe, so EnumerateReporters()
    // must be called from the main thread.
    if (!NS_IsMainThread()) {
        MOZ_CRASH();
    }

    mozilla::MutexAutoLock autoLock(mMutex);

    nsRefPtr<HashtableEnumerator> enumerator =
        new HashtableEnumerator(mReporters);
    enumerator.forget(aResult);
    return NS_OK;
}

static void
DebugAssertRefcountIsNonZero(nsISupports* aObj)
{
#ifdef DEBUG
    // This will probably crash if the object's refcount is 0.
    uint32_t refcnt = NS_ADDREF(aObj);
    MOZ_ASSERT(refcnt >= 2);
    NS_RELEASE(aObj);
#endif
}

nsresult
nsMemoryReporterManager::RegisterReporterHelper(
    nsIMemoryReporter* aReporter, bool aForce)
{
    // This method is thread-safe.
    mozilla::MutexAutoLock autoLock(mMutex);

    if ((mIsRegistrationBlocked && !aForce) || mReporters.Contains(aReporter)) {
        return NS_ERROR_FAILURE;
    }

    // This method needs to be safe even if |aReporter| has a refcnt of 0, so
    // we take a kung fu death grip before calling PutEntry.  Otherwise, if
    // PutEntry addref'ed and released |aReporter| before finally addref'ing it
    // for good, it would free aReporter!
    //
    // The kung fu death grip could itself be problematic if PutEntry didn't
    // addref |aReporter| (because then when the death grip goes out of scope,
    // we would delete the reporter).  In debug mode, we check that this
    // doesn't happen.

    {
        nsCOMPtr<nsIMemoryReporter> kungFuDeathGrip = aReporter;
        mReporters.PutEntry(aReporter);
    }

    DebugAssertRefcountIsNonZero(aReporter);

    return NS_OK;
}

NS_IMETHODIMP
nsMemoryReporterManager::RegisterReporter(nsIMemoryReporter* aReporter)
{
    return RegisterReporterHelper(aReporter, /* force = */ false);
}

NS_IMETHODIMP
nsMemoryReporterManager::RegisterReporterEvenIfBlocked(
    nsIMemoryReporter* aReporter)
{
    return RegisterReporterHelper(aReporter, /* force = */ true);
}

NS_IMETHODIMP
nsMemoryReporterManager::UnregisterReporter(nsIMemoryReporter* aReporter)
{
    // This method is thread-safe.
    mozilla::MutexAutoLock autoLock(mMutex);

    if (!mReporters.Contains(aReporter)) {
        return NS_ERROR_FAILURE;
    }

    mReporters.RemoveEntry(aReporter);
    return NS_OK;
}

NS_IMETHODIMP
nsMemoryReporterManager::BlockRegistration()
{
    // This method is thread-safe.
    mozilla::MutexAutoLock autoLock(mMutex);
    if (mIsRegistrationBlocked) {
        return NS_ERROR_FAILURE;
    }
    mIsRegistrationBlocked = true;
    return NS_OK;
}

NS_IMETHODIMP
nsMemoryReporterManager::UnblockRegistration()
{
    // This method is thread-safe.
    mozilla::MutexAutoLock autoLock(mMutex);
    if (!mIsRegistrationBlocked) {
        return NS_ERROR_FAILURE;
    }
    mIsRegistrationBlocked = false;
    return NS_OK;
}

// This is just a wrapper for int64_t that implements nsISupports, so it can be
// passed to nsIMemoryReporter::CollectReports.
class Int64Wrapper MOZ_FINAL : public nsISupports {
public:
    NS_DECL_ISUPPORTS
    Int64Wrapper() : mValue(0) { }
    int64_t mValue;
};
NS_IMPL_ISUPPORTS0(Int64Wrapper)

class ExplicitCallback MOZ_FINAL : public nsIMemoryReporterCallback
{
public:
    NS_DECL_ISUPPORTS

    NS_IMETHOD Callback(const nsACString& aProcess, const nsACString& aPath,
                        int32_t aKind, int32_t aUnits, int64_t aAmount,
                        const nsACString& aDescription,
                        nsISupports* aWrappedExplicit)
    {
        // Using the "heap-allocated" reporter here instead of
        // nsMemoryReporterManager.heapAllocated goes against the usual
        // pattern.  But it's for a good reason:  in tests, we can easily
        // create artificial (i.e. deterministic) reporters -- which allows us
        // to precisely test nsMemoryReporterManager.explicit -- but we can't
        // do that for distinguished amounts.
        if (aPath.Equals("heap-allocated") ||
            (aKind == nsIMemoryReporter::KIND_NONHEAP &&
             PromiseFlatCString(aPath).Find("explicit") == 0))
        {
            Int64Wrapper* wrappedInt64 =
                static_cast<Int64Wrapper*>(aWrappedExplicit);
            wrappedInt64->mValue += aAmount;
        }
        return NS_OK;
    }
};
NS_IMPL_ISUPPORTS1(ExplicitCallback, nsIMemoryReporterCallback)

NS_IMETHODIMP
nsMemoryReporterManager::GetExplicit(int64_t* aAmount)
{
    NS_ENSURE_ARG_POINTER(aAmount);
    *aAmount = 0;
#ifndef HAVE_JEMALLOC_STATS
    return NS_ERROR_NOT_AVAILABLE;
#else
    bool more;

    // For each reporter we call CollectReports and filter out the
    // non-explicit, non-NONHEAP measurements (except for "heap-allocated").
    // That's lots of wasted work, and we used to have a GetExplicitNonHeap()
    // method which did this more efficiently, but it ended up being more
    // trouble than it was worth.

    nsRefPtr<ExplicitCallback> cb = new ExplicitCallback();
    nsRefPtr<Int64Wrapper> wrappedExplicitSize = new Int64Wrapper();

    nsCOMPtr<nsISimpleEnumerator> e;
    EnumerateReporters(getter_AddRefs(e));
    while (NS_SUCCEEDED(e->HasMoreElements(&more)) && more) {
        nsCOMPtr<nsIMemoryReporter> r;
        e->GetNext(getter_AddRefs(r));
        r->CollectReports(cb, wrappedExplicitSize);
    }

    *aAmount = wrappedExplicitSize->mValue;

    return NS_OK;
#endif // HAVE_JEMALLOC_STATS
}

NS_IMETHODIMP
nsMemoryReporterManager::GetVsize(int64_t* aVsize)
{
#ifdef HAVE_VSIZE_AND_RESIDENT_REPORTERS
    return ::GetVsize(aVsize);
#else
    *aResident = 0;
    return NS_ERROR_NOT_AVAILABLE;
#endif
}

NS_IMETHODIMP
nsMemoryReporterManager::GetResident(int64_t* aAmount)
{
#ifdef HAVE_VSIZE_AND_RESIDENT_REPORTERS
    return ::GetResident(aAmount);
#else
    *aAmount = 0;
    return NS_ERROR_NOT_AVAILABLE;
#endif
}

NS_IMETHODIMP
nsMemoryReporterManager::GetResidentFast(int64_t* aAmount)
{
#ifdef HAVE_VSIZE_AND_RESIDENT_REPORTERS
    return ::GetResidentFast(aAmount);
#else
    *aAmount = 0;
    return NS_ERROR_NOT_AVAILABLE;
#endif
}

NS_IMETHODIMP
nsMemoryReporterManager::GetHeapAllocated(int64_t* aAmount)
{
#ifdef HAVE_JEMALLOC_STATS
    *aAmount = HeapAllocated();
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
    *aAmount = HeapOverheadRatio();
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

namespace {

/**
 * This runnable lets us implement nsIMemoryReporterManager::MinimizeMemoryUsage().
 * We fire a heap-minimize notification, spin the event loop, and repeat this
 * process a few times.
 *
 * When this sequence finishes, we invoke the callback function passed to the
 * runnable's constructor.
 */
class MinimizeMemoryUsageRunnable : public nsCancelableRunnable
{
public:
  MinimizeMemoryUsageRunnable(nsIRunnable* aCallback)
    : mCallback(aCallback)
    , mRemainingIters(sNumIters)
    , mCanceled(false)
  {}

  NS_IMETHOD Run()
  {
    if (mCanceled) {
      return NS_OK;
    }

    nsCOMPtr<nsIObserverService> os = services::GetObserverService();
    if (!os) {
      return NS_ERROR_FAILURE;
    }

    if (mRemainingIters == 0) {
      os->NotifyObservers(nullptr, "after-minimize-memory-usage",
                          NS_LITERAL_STRING("MinimizeMemoryUsageRunnable").get());
      if (mCallback) {
        mCallback->Run();
      }
      return NS_OK;
    }

    os->NotifyObservers(nullptr, "memory-pressure",
                        NS_LITERAL_STRING("heap-minimize").get());
    mRemainingIters--;
    NS_DispatchToMainThread(this);

    return NS_OK;
  }

  NS_IMETHOD Cancel()
  {
    if (mCanceled) {
      return NS_ERROR_UNEXPECTED;
    }

    mCanceled = true;

    return NS_OK;
  }

private:
  // Send sNumIters heap-minimize notifications, spinning the event
  // loop after each notification (see bug 610166 comment 12 for an
  // explanation), because one notification doesn't cut it.
  static const uint32_t sNumIters = 3;

  nsCOMPtr<nsIRunnable> mCallback;
  uint32_t mRemainingIters;
  bool mCanceled;
};

} // anonymous namespace

NS_IMETHODIMP
nsMemoryReporterManager::MinimizeMemoryUsage(nsIRunnable* aCallback,
                                             nsICancelableRunnable** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  nsRefPtr<nsICancelableRunnable> runnable =
    new MinimizeMemoryUsageRunnable(aCallback);
  NS_ADDREF(*aResult = runnable);

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
    NS_ENSURE_TRUE(!!global && !!piWindow, NS_ERROR_FAILURE);

    TimeStamp t1 = TimeStamp::Now();

    // Measure JS memory consumption (and possibly some non-JS consumption, via
    // |jsPrivateSize|).
    size_t jsObjectsSize, jsStringsSize, jsPrivateSize, jsOtherSize;
    nsresult rv = mSizeOfTabFns.mJS(global->GetGlobalJSObject(),
                                    &jsObjectsSize, &jsStringsSize,
                                    &jsPrivateSize, &jsOtherSize);
    NS_ENSURE_SUCCESS(rv, rv);

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

// Most memory reporters don't need thread safety, but some do.  Make them all
// thread-safe just to be safe.  Memory reporters are created and destroyed
// infrequently enough that the performance cost should be negligible.
NS_IMPL_ISUPPORTS1(MemoryUniReporter, nsIMemoryReporter)

nsresult
NS_RegisterMemoryReporter(nsIMemoryReporter* aReporter)
{
    nsCOMPtr<nsIMemoryReporterManager> mgr = do_GetService("@mozilla.org/memory-reporter-manager;1");
    if (!mgr) {
        return NS_ERROR_FAILURE;
    }
    return mgr->RegisterReporter(aReporter);
}

nsresult
NS_UnregisterMemoryReporter(nsIMemoryReporter* aReporter)
{
    nsCOMPtr<nsIMemoryReporterManager> mgr = do_GetService("@mozilla.org/memory-reporter-manager;1");
    if (!mgr) {
        return NS_ERROR_FAILURE;
    }
    return mgr->UnregisterReporter(aReporter);
}

namespace mozilla {

#define GET_MEMORY_REPORTER_MANAGER(mgr)                                      \
    nsCOMPtr<nsIMemoryReporterManager> imgr =                                 \
        do_GetService("@mozilla.org/memory-reporter-manager;1");              \
    nsRefPtr<nsMemoryReporterManager> mgr =                                   \
        static_cast<nsMemoryReporterManager*>(imgr.get());                    \
    if (!mgr) {                                                               \
        return NS_ERROR_FAILURE;                                              \
    }

// Macro for generating functions that register distinguished amount functions
// with the memory reporter manager.
#define DEFINE_REGISTER_DISTINGUISHED_AMOUNT(kind, name)                      \
    nsresult                                                                  \
    Register##name##DistinguishedAmount(kind##AmountFn aAmountFn)             \
    {                                                                         \
        GET_MEMORY_REPORTER_MANAGER(mgr)                                      \
        mgr->mAmountFns.m##name = aAmountFn;                                  \
        return NS_OK;                                                         \
    }

#define DEFINE_UNREGISTER_DISTINGUISHED_AMOUNT(name)                          \
    nsresult                                                                  \
    Unregister##name##DistinguishedAmount()                                   \
    {                                                                         \
        GET_MEMORY_REPORTER_MANAGER(mgr)                                      \
        mgr->mAmountFns.m##name = nullptr;                                    \
        return NS_OK;                                                         \
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
    nsresult                                                                  \
    Register##name##SizeOfTab(name##SizeOfTabFn aSizeOfTabFn)                 \
    {                                                                         \
        GET_MEMORY_REPORTER_MANAGER(mgr)                                      \
        mgr->mSizeOfTabFns.m##name = aSizeOfTabFn;                            \
        return NS_OK;                                                         \
    }

DEFINE_REGISTER_SIZE_OF_TAB(JS);
DEFINE_REGISTER_SIZE_OF_TAB(NonJS);

#undef DEFINE_REGISTER_SIZE_OF_TAB

#undef GET_MEMORY_REPORTER_MANAGER

}

#if defined(MOZ_DMD)

namespace mozilla {
namespace dmd {

class NullReporterCallback : public nsIMemoryReporterCallback
{
public:
    NS_DECL_ISUPPORTS

    NS_IMETHOD Callback(const nsACString& aProcess, const nsACString& aPath,
                        int32_t aKind, int32_t aUnits, int64_t aAmount,
                        const nsACString& aDescription,
                        nsISupports* aData)
    {
        // Do nothing;  the reporter has already reported to DMD.
        return NS_OK;
    }
};
NS_IMPL_ISUPPORTS1(
  NullReporterCallback
, nsIMemoryReporterCallback
)

void
RunReporters()
{
    nsCOMPtr<nsIMemoryReporterManager> mgr =
        do_GetService("@mozilla.org/memory-reporter-manager;1");

    nsRefPtr<NullReporterCallback> cb = new NullReporterCallback();

    bool more;
    nsCOMPtr<nsISimpleEnumerator> e;
    mgr->EnumerateReporters(getter_AddRefs(e));
    while (NS_SUCCEEDED(e->HasMoreElements(&more)) && more) {
        nsCOMPtr<nsIMemoryReporter> r;
        e->GetNext(getter_AddRefs(r));
        r->CollectReports(cb, nullptr);
    }
}

} // namespace dmd
} // namespace mozilla

#endif  // defined(MOZ_DMD)

