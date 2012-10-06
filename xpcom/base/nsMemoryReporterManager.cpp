/* -*- Mode: C++; tab-width: 50; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAtomTable.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsMemoryReporterManager.h"
#include "nsArrayEnumerator.h"
#include "nsIConsoleService.h"
#include "nsISimpleEnumerator.h"
#include "nsIFile.h"
#include "nsIFileStreams.h"
#include "nsPrintfCString.h"
#include "nsThreadUtils.h"
#include "nsIObserverService.h"
#include "nsThread.h"
#include "nsGZFileWriter.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Attributes.h"
#include "mozilla/Services.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/FileUtils.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/unused.h"

#ifdef XP_WIN
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h>
#include <fcntl.h>
#endif

using namespace mozilla;
using namespace mozilla::dom;

#if defined(MOZ_MEMORY)
#  define HAVE_JEMALLOC_STATS 1
#  include "jemalloc.h"
#endif  // MOZ_MEMORY

#ifdef XP_UNIX

#include <sys/time.h>
#include <sys/resource.h>

#define HAVE_PAGE_FAULT_REPORTERS 1
static nsresult GetHardPageFaults(int64_t *n)
{
    struct rusage usage;
    int err = getrusage(RUSAGE_SELF, &usage);
    if (err != 0) {
        return NS_ERROR_FAILURE;
    }
    *n = usage.ru_majflt;
    return NS_OK;
}

static nsresult GetSoftPageFaults(int64_t *n)
{
    struct rusage usage;
    int err = getrusage(RUSAGE_SELF, &usage);
    if (err != 0) {
        return NS_ERROR_FAILURE;
    }
    *n = usage.ru_minflt;
    return NS_OK;
}

#endif  // HAVE_PAGE_FAULT_REPORTERS

#if defined(XP_LINUX)

#include <unistd.h>
static nsresult GetProcSelfStatmField(int field, int64_t *n)
{
    // There are more than two fields, but we're only interested in the first
    // two.
    static const int MAX_FIELD = 2;
    size_t fields[MAX_FIELD];
    MOZ_ASSERT(field < MAX_FIELD, "bad field number");
    FILE *f = fopen("/proc/self/statm", "r");
    if (f) {
        int nread = fscanf(f, "%zu %zu", &fields[0], &fields[1]);
        fclose(f);
        if (nread == MAX_FIELD) {
            *n = fields[field] * getpagesize();
            return NS_OK;
        } 
    }
    return NS_ERROR_FAILURE;
}

#define HAVE_VSIZE_AND_RESIDENT_REPORTERS 1
static nsresult GetVsize(int64_t *n)
{
    return GetProcSelfStatmField(0, n);
}

static nsresult GetResident(int64_t *n)
{
    return GetProcSelfStatmField(1, n);
}

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

static nsresult GetKinfoProcSelf(KINFO_PROC *proc)
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
    if (sysctl(mib, miblen, proc, &size, NULL, 0))
        return NS_ERROR_FAILURE;

    return NS_OK;
}

#define HAVE_VSIZE_AND_RESIDENT_REPORTERS 1
static nsresult GetVsize(int64_t *n)
{
    KINFO_PROC proc;
    nsresult rv = GetKinfoProcSelf(&proc);
    if (NS_SUCCEEDED(rv))
        *n = KP_SIZE(proc);

    return rv;
}

static nsresult GetResident(int64_t *n)
{
    KINFO_PROC proc;
    nsresult rv = GetKinfoProcSelf(&proc);
    if (NS_SUCCEEDED(rv))
        *n = KP_RSS(proc);

    return rv;
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
    prxmap_t *prmapp = NULL;
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
static nsresult GetVsize(int64_t *n)
{
    int64_t vsize, resident;
    XMappingIter(vsize, resident);
    if (vsize == -1) {
        return NS_ERROR_FAILURE;
    }
    *n = vsize;
    return NS_OK;
}

static nsresult GetResident(int64_t *n)
{
    int64_t vsize, resident;
    XMappingIter(vsize, resident);
    if (resident == -1) {
        return NS_ERROR_FAILURE;
    }
    *n = resident;
    return NS_OK;
}

#elif defined(XP_MACOSX)

#include <mach/mach_init.h>
#include <mach/task.h>

static bool GetTaskBasicInfo(struct task_basic_info *ti)
{
    mach_msg_type_number_t count = TASK_BASIC_INFO_COUNT;
    kern_return_t kr = task_info(mach_task_self(), TASK_BASIC_INFO,
                                 (task_info_t)ti, &count);
    return kr == KERN_SUCCESS;
}

// The VSIZE figure on Mac includes huge amounts of shared memory and is always
// absurdly high, eg. 2GB+ even at start-up.  But both 'top' and 'ps' report
// it, so we might as well too.
#define HAVE_VSIZE_AND_RESIDENT_REPORTERS 1
static nsresult GetVsize(int64_t *n)
{
    task_basic_info ti;
    if (!GetTaskBasicInfo(&ti))
        return NS_ERROR_FAILURE;

    *n = ti.virtual_size;
    return NS_OK;
}

static nsresult GetResident(int64_t *n)
{
#ifdef HAVE_JEMALLOC_STATS
    // If we're using jemalloc on Mac, we need to instruct jemalloc to purge
    // the pages it has madvise(MADV_FREE)'d before we read our RSS.  The OS
    // will take away MADV_FREE'd pages when there's memory pressure, so they
    // shouldn't count against our RSS.
    //
    // Purging these pages shouldn't take more than 10ms or so, but we want to
    // keep an eye on it since GetResident() is called on each Telemetry ping.
    {
      Telemetry::AutoTimer<Telemetry::MEMORY_FREE_PURGED_PAGES_MS> timer;
      jemalloc_purge_freed_pages();
    }
#endif

    task_basic_info ti;
    if (!GetTaskBasicInfo(&ti))
        return NS_ERROR_FAILURE;

    *n = ti.resident_size;
    return NS_OK;
}

#elif defined(XP_WIN)

#include <windows.h>
#include <psapi.h>

#define HAVE_VSIZE_AND_RESIDENT_REPORTERS 1
static nsresult GetVsize(int64_t *n)
{
    MEMORYSTATUSEX s;
    s.dwLength = sizeof(s);

    if (!GlobalMemoryStatusEx(&s)) {
        return NS_ERROR_FAILURE;
    }

    *n = s.ullTotalVirtual - s.ullAvailVirtual;
    return NS_OK;
}

static nsresult GetResident(int64_t *n)
{
    PROCESS_MEMORY_COUNTERS pmc;
    pmc.cb = sizeof(PROCESS_MEMORY_COUNTERS);

    if (!GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return NS_ERROR_FAILURE;
    }

    *n = pmc.WorkingSetSize;
    return NS_OK;
}

#define HAVE_PRIVATE_REPORTER
static nsresult GetPrivate(int64_t *n)
{
    PROCESS_MEMORY_COUNTERS_EX pmcex;
    pmcex.cb = sizeof(PROCESS_MEMORY_COUNTERS_EX);

    if (!GetProcessMemoryInfo(GetCurrentProcess(),
                              (PPROCESS_MEMORY_COUNTERS) &pmcex, sizeof(pmcex)))
    {
        return NS_ERROR_FAILURE;
    }

    *n = pmcex.PrivateUsage;
    return NS_OK;
}

NS_FALLIBLE_MEMORY_REPORTER_IMPLEMENT(Private,
    "private",
    KIND_OTHER,
    UNITS_BYTES,
    GetPrivate,
    "Memory that cannot be shared with other processes, including memory that "
    "is committed and marked MEM_PRIVATE, data that is not mapped, and "
    "executable pages that have been written to.")

#endif  // XP_<PLATFORM>

#ifdef HAVE_VSIZE_AND_RESIDENT_REPORTERS
NS_FALLIBLE_MEMORY_REPORTER_IMPLEMENT(Vsize,
    "vsize",
    KIND_OTHER,
    UNITS_BYTES,
    GetVsize,
    "Memory mapped by the process, including code and data segments, the "
    "heap, thread stacks, memory explicitly mapped by the process via mmap "
    "and similar operations, and memory shared with other processes. "
    "This is the vsize figure as reported by 'top' and 'ps'.  This figure is of "
    "limited use on Mac, where processes share huge amounts of memory with one "
    "another.  But even on other operating systems, 'resident' is a much better "
    "measure of the memory resources used by the process.")

NS_FALLIBLE_MEMORY_REPORTER_IMPLEMENT(Resident,
    "resident",
    KIND_OTHER,
    UNITS_BYTES,
    GetResident,
    "Memory mapped by the process that is present in physical memory, "
    "also known as the resident set size (RSS).  This is the best single "
    "figure to use when considering the memory resources used by the process, "
    "but it depends both on other processes being run and details of the OS "
    "kernel and so is best used for comparing the memory usage of a single "
    "process at different points in time.")
#endif  // HAVE_VSIZE_AND_RESIDENT_REPORTERS

#ifdef HAVE_PAGE_FAULT_REPORTERS
NS_FALLIBLE_MEMORY_REPORTER_IMPLEMENT(PageFaultsSoft,
    "page-faults-soft",
    KIND_OTHER,
    UNITS_COUNT_CUMULATIVE,
    GetSoftPageFaults,
    "The number of soft page faults (also known as 'minor page faults') that "
    "have occurred since the process started.  A soft page fault occurs when the "
    "process tries to access a page which is present in physical memory but is "
    "not mapped into the process's address space.  For instance, a process might "
    "observe soft page faults when it loads a shared library which is already "
    "present in physical memory. A process may experience many thousands of soft "
    "page faults even when the machine has plenty of available physical memory, "
    "and because the OS services a soft page fault without accessing the disk, "
    "they impact performance much less than hard page faults.")

NS_FALLIBLE_MEMORY_REPORTER_IMPLEMENT(PageFaultsHard,
    "page-faults-hard",
    KIND_OTHER,
    UNITS_COUNT_CUMULATIVE,
    GetHardPageFaults,
    "The number of hard page faults (also known as 'major page faults') that "
    "have occurred since the process started.  A hard page fault occurs when a "
    "process tries to access a page which is not present in physical memory. "
    "The operating system must access the disk in order to fulfill a hard page "
    "fault. When memory is plentiful, you should see very few hard page faults. "
    "But if the process tries to use more memory than your machine has "
    "available, you may see many thousands of hard page faults. Because "
    "accessing the disk is up to a million times slower than accessing RAM, "
    "the program may run very slowly when it is experiencing more than 100 or "
    "so hard page faults a second.")
#endif  // HAVE_PAGE_FAULT_REPORTERS

/**
 ** memory reporter implementation for jemalloc and OSX malloc,
 ** to obtain info on total memory in use (that we know about,
 ** at least -- on OSX, there are sometimes other zones in use).
 **/

#if HAVE_JEMALLOC_STATS

#define HAVE_HEAP_ALLOCATED_REPORTERS 1

static int64_t GetHeapUnused()
{
    jemalloc_stats_t stats;
    jemalloc_stats(&stats);
    return (int64_t) (stats.mapped - stats.allocated);
}

static int64_t GetHeapAllocated()
{
    jemalloc_stats_t stats;
    jemalloc_stats(&stats);
    return (int64_t) stats.allocated;
}

static int64_t GetHeapCommitted()
{
    jemalloc_stats_t stats;
    jemalloc_stats(&stats);
    return (int64_t) stats.committed;
}

static int64_t GetHeapCommittedUnused()
{
    jemalloc_stats_t stats;
    jemalloc_stats(&stats);
    return stats.committed - stats.allocated;
}

static int64_t GetHeapCommittedUnusedRatio()
{
    jemalloc_stats_t stats;
    jemalloc_stats(&stats);
    return (int64_t) 10000 * (stats.committed - stats.allocated) /
                              ((double)stats.allocated);
}

static int64_t GetHeapDirty()
{
    jemalloc_stats_t stats;
    jemalloc_stats(&stats);
    return (int64_t) stats.dirty;
}

NS_MEMORY_REPORTER_IMPLEMENT(HeapCommitted,
    "heap-committed",
    KIND_OTHER,
    UNITS_BYTES,
    GetHeapCommitted,
    "Memory mapped by the heap allocator that is committed, i.e. in physical "
    "memory or paged to disk.  When heap-committed is larger than "
    "heap-allocated, the difference between the two values is likely due to "
    "external fragmentation; that is, the allocator allocated a large block of "
    "memory and is unable to decommit it because a small part of that block is "
    "currently in use.")

NS_MEMORY_REPORTER_IMPLEMENT(HeapCommittedUnused,
    "heap-committed-unused",
    KIND_OTHER,
    UNITS_BYTES,
    GetHeapCommittedUnused,
    "Committed bytes which do not correspond to an active allocation; i.e., "
    "'heap-committed' - 'heap-allocated'.  Although the allocator will waste some "
    "space under any circumstances, a large value here may indicate that the "
    "heap is highly fragmented.")

NS_MEMORY_REPORTER_IMPLEMENT(HeapCommittedUnusedRatio,
    "heap-committed-unused-ratio",
    KIND_OTHER,
    UNITS_PERCENTAGE,
    GetHeapCommittedUnusedRatio,
    "Ratio of committed, unused bytes to allocated bytes; i.e., "
    "'heap-committed-unused' / 'heap-allocated'.  This measures the overhead "
    "of the heap allocator relative to amount of memory allocated.")

NS_MEMORY_REPORTER_IMPLEMENT(HeapDirty,
    "heap-dirty",
    KIND_OTHER,
    UNITS_BYTES,
    GetHeapDirty,
    "Memory which the allocator could return to the operating system, but "
    "hasn't.  The allocator keeps this memory around as an optimization, so it "
    "doesn't have to ask the OS the next time it needs to fulfill a request. "
    "This value is typically not larger than a few megabytes.")

#elif defined(XP_MACOSX) && !defined(MOZ_MEMORY)
#include <malloc/malloc.h>

#define HAVE_HEAP_ALLOCATED_REPORTERS 1

static int64_t GetHeapUnused()
{
    struct mstats stats = mstats();
    return stats.bytes_total - stats.bytes_used;
}

static int64_t GetHeapAllocated()
{
    struct mstats stats = mstats();
    return stats.bytes_used;
}

// malloc_zone_statistics() crashes when run under DMD because Valgrind doesn't
// intercept it.  This measurement isn't important for DMD, so don't even try
// to get it.
#ifndef MOZ_DMD
#define HAVE_HEAP_ZONE0_REPORTERS 1
static int64_t GetHeapZone0Committed()
{
    malloc_statistics_t stats;
    malloc_zone_statistics(malloc_default_zone(), &stats);
    return stats.size_in_use;
}

static int64_t GetHeapZone0Used()
{
    malloc_statistics_t stats;
    malloc_zone_statistics(malloc_default_zone(), &stats);
    return stats.size_allocated;
}

NS_MEMORY_REPORTER_IMPLEMENT(HeapZone0Committed,
    "heap-zone0-committed",
    KIND_OTHER,
    UNITS_BYTES,
    GetHeapZone0Committed,
    "Memory mapped by the heap allocator that is committed in the default "
    "zone.")

NS_MEMORY_REPORTER_IMPLEMENT(HeapZone0Used,
    "heap-zone0-used",
    KIND_OTHER,
    UNITS_BYTES,
    GetHeapZone0Used,
    "Memory mapped by the heap allocator in the default zone that is "
    "allocated to the application.")
#endif  // MOZ_DMD

#endif

#ifdef HAVE_HEAP_ALLOCATED_REPORTERS
NS_MEMORY_REPORTER_IMPLEMENT(HeapUnused,
    "heap-unused",
    KIND_OTHER,
    UNITS_BYTES,
    GetHeapUnused,
    "Memory mapped by the heap allocator that is not part of an active "
    "allocation. Much of this memory may be uncommitted -- that is, it does not "
    "take up space in physical memory or in the swap file.")

NS_MEMORY_REPORTER_IMPLEMENT(HeapAllocated,
    "heap-allocated",
    KIND_OTHER,
    UNITS_BYTES,
    GetHeapAllocated,
    "Memory mapped by the heap allocator that is currently allocated to the "
    "application.  This may exceed the amount of memory requested by the "
    "application because the allocator regularly rounds up request sizes. (The "
    "exact amount requested is not recorded.)")

// The computation of "explicit" fails if "heap-allocated" isn't available,
// which is why this is depends on HAVE_HEAP_ALLOCATED_AND_EXPLICIT_REPORTERS.
#define HAVE_EXPLICIT_REPORTER 1
static nsresult GetExplicit(int64_t *n)
{
    nsCOMPtr<nsIMemoryReporterManager> mgr = do_GetService("@mozilla.org/memory-reporter-manager;1");
    if (mgr == nullptr)
        return NS_ERROR_FAILURE;

    return mgr->GetExplicit(n);
}

NS_FALLIBLE_MEMORY_REPORTER_IMPLEMENT(Explicit,
    "explicit",
    KIND_OTHER,
    UNITS_BYTES,
    GetExplicit,
    "This is the same measurement as the root of the 'explicit' tree.  "
    "However, it is measured at a different time and so gives slightly "
    "different results.")
#endif  // HAVE_HEAP_ALLOCATED_REPORTERS

NS_MEMORY_REPORTER_MALLOC_SIZEOF_FUN(AtomTableMallocSizeOf, "atom-table")

static int64_t GetAtomTableSize() {
  return NS_SizeOfAtomTablesIncludingThis(AtomTableMallocSizeOf);
}

// Why is this here?  At first glance, you'd think it could be defined and
// registered with nsMemoryReporterManager entirely within nsAtomTable.cpp.
// However, the obvious time to register it is when the table is initialized,
// and that happens before XPCOM components are initialized, which means the
// NS_RegisterMemoryReporter call fails.  So instead we do it here.
NS_MEMORY_REPORTER_IMPLEMENT(AtomTable,
    "explicit/atom-tables",
    KIND_HEAP,
    UNITS_BYTES,
    GetAtomTableSize,
    "Memory used by the dynamic and static atoms tables.")

/**
 ** nsMemoryReporterManager implementation
 **/

NS_IMPL_THREADSAFE_ISUPPORTS1(nsMemoryReporterManager, nsIMemoryReporterManager)

namespace {

class DumpMemoryReportsRunnable : public nsRunnable
{
public:
  DumpMemoryReportsRunnable(const nsAString& aIdentifier,
                            bool aMinimizeMemoryUsage,
                            bool aDumpChildProcesses)

      : mIdentifier(aIdentifier)
      , mMinimizeMemoryUsage(aMinimizeMemoryUsage)
      , mDumpChildProcesses(aDumpChildProcesses)
  {}

  NS_IMETHOD Run()
  {
      nsCOMPtr<nsIMemoryReporterManager> mgr =
          do_GetService("@mozilla.org/memory-reporter-manager;1");
      NS_ENSURE_STATE(mgr);
      mgr->DumpMemoryReportsToFile(mIdentifier,
                                   mMinimizeMemoryUsage,
                                   mDumpChildProcesses);
      return NS_OK;
  }

private:
  const nsString mIdentifier;
  const bool mMinimizeMemoryUsage;
  const bool mDumpChildProcesses;
};

} // anonymous namespace

#ifdef XP_LINUX // {
namespace {

/*
 * The following code supports dumping about:memory upon receiving a signal.
 *
 * We listen for the signals SIGRTMIN and SIGRTMIN +1.  (The latter causes us
 * to minimize memory usage before dumping about:memory.)
 *
 * When we receive one of these signals, we write the signal number to a pipe.
 * The IO thread then notices that the pipe has been written to, and kicks off
 * a DumpMemoryReports task on the main thread.
 *
 * This scheme is similar to using signalfd(), except it's portable and it
 * doesn't require the use of sigprocmask, which is problematic because it
 * masks signals received by child processes.
 *
 * In theory, we could use Chromium's MessageLoopForIO::CatchSignal() for this.
 * But that uses libevent, which does not handle the realtime signals (bug
 * 794074).
 */

// It turns out that at least on some systems, SIGRTMIN is not a compile-time
// constant, so these have to be set at runtime.
static int sDumpAboutMemorySignum;         // SIGRTMIN
static int sDumpAboutMemoryAfterMMUSignum; // SIGRTMIN + 1

// This is the write-end of a pipe that we use to notice when a
// dump-about-memory signal occurs.
static int sDumpAboutMemoryPipeWriteFd;

void
DumpAboutMemorySignalHandler(int aSignum)
{
  // This is a signal handler, so everything in here needs to be
  // async-signal-safe.  Be careful!

  if (sDumpAboutMemoryPipeWriteFd != 0) {
    uint8_t signum = static_cast<int>(aSignum);
    write(sDumpAboutMemoryPipeWriteFd, &signum, sizeof(signum));
  }
}

class SignalPipeWatcher : public MessageLoopForIO::Watcher
{
public:
  SignalPipeWatcher()
  {}

  ~SignalPipeWatcher()
  {
    // This is somewhat paranoid, but we want to avoid the race condition where
    // we close sDumpAboutMemoryPipeWriteFd before setting it to 0, then we
    // reuse that fd for some other file, and then the signal handler runs.
    int pipeWriteFd = sDumpAboutMemoryPipeWriteFd;
    PR_ATOMIC_SET(&sDumpAboutMemoryPipeWriteFd, 0);

    // Stop watching the pipe's file descriptor /before/ we close it!
    mReadWatcher.StopWatchingFileDescriptor();

    close(pipeWriteFd);
    close(mPipeReadFd);
  }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SignalPipeWatcher)

  bool Start()
  {
    MOZ_ASSERT(XRE_GetIOMessageLoop() == MessageLoopForIO::current());

    sDumpAboutMemorySignum = SIGRTMIN;
    sDumpAboutMemoryAfterMMUSignum = SIGRTMIN + 1;

    // Create a pipe.  When we receive a signal in our signal handler, we'll
    // write the signum to the write-end of this pipe.
    int pipeFds[2];
    if (pipe(pipeFds)) {
        NS_WARNING("Failed to create pipe.");
        return false;
    }

    // Close this pipe on calls to exec().
    fcntl(pipeFds[0], F_SETFD, FD_CLOEXEC);
    fcntl(pipeFds[1], F_SETFD, FD_CLOEXEC);

    mPipeReadFd = pipeFds[0];
    sDumpAboutMemoryPipeWriteFd = pipeFds[1];

    struct sigaction action;
    memset(&action, 0, sizeof(action));
    sigemptyset(&action.sa_mask);
    action.sa_handler = DumpAboutMemorySignalHandler;

    if (sigaction(sDumpAboutMemorySignum, &action, nullptr)) {
      NS_WARNING("Failed to register about:memory dump signal handler.");
    }
    if (sigaction(sDumpAboutMemoryAfterMMUSignum, &action, nullptr)) {
      NS_WARNING("Failed to register about:memory dump after MMU signal handler.");
    }

    // Start watching the read end of the pipe on the IO thread.
    return MessageLoopForIO::current()->WatchFileDescriptor(
        mPipeReadFd, /* persistent = */ true,
        MessageLoopForIO::WATCH_READ,
        &mReadWatcher, this);
  }

  virtual void OnFileCanReadWithoutBlocking(int aFd)
  {
    MOZ_ASSERT(XRE_GetIOMessageLoop() == MessageLoopForIO::current());

    uint8_t signum;
    ssize_t numReceived = read(aFd, &signum, sizeof(signum));
    if (numReceived != sizeof(signum)) {
      NS_WARNING("Error reading from buffer in "
                 "SignalPipeWatcher::OnFileCanReadWithoutBlocking.");
      return;
    }

    if (signum != sDumpAboutMemorySignum &&
        signum != sDumpAboutMemoryAfterMMUSignum) {
      NS_WARNING("Got unexpected signum.");
      return;
    }

    // Dump about:memory (but run this on the main thread!).
    nsRefPtr<DumpMemoryReportsRunnable> runnable =
      new DumpMemoryReportsRunnable(
          /* identifier = */ EmptyString(),
          signum == sDumpAboutMemoryAfterMMUSignum,
          /* dumpChildProcesses = */ true);
    NS_DispatchToMainThread(runnable);
  }

  virtual void OnFileCanWriteWithoutBlocking(int aFd)
  {}

private:
  int mPipeReadFd;
  MessageLoopForIO::FileDescriptorWatcher mReadWatcher;
};

StaticRefPtr<SignalPipeWatcher> sSignalPipeWatcher;

void
InitializeDumpAboutMemoryWatcher()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!sSignalPipeWatcher);

  sSignalPipeWatcher = new SignalPipeWatcher();
  ClearOnShutdown(&sSignalPipeWatcher);

  XRE_GetIOMessageLoop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(sSignalPipeWatcher.get(),
                        &SignalPipeWatcher::Start));
}

} // anonymous namespace
#endif // } XP_LINUX

NS_IMETHODIMP
nsMemoryReporterManager::Init()
{
#if HAVE_JEMALLOC_STATS && defined(XP_LINUX)
    if (!jemalloc_stats)
        return NS_ERROR_FAILURE;
#endif

#define REGISTER(_x)  RegisterReporter(new NS_MEMORY_REPORTER_NAME(_x))

#ifdef HAVE_HEAP_ALLOCATED_REPORTERS
    REGISTER(HeapAllocated);
    REGISTER(HeapUnused);
#endif

#ifdef HAVE_EXPLICIT_REPORTER
    REGISTER(Explicit);
#endif

#ifdef HAVE_VSIZE_AND_RESIDENT_REPORTERS
    REGISTER(Vsize);
    REGISTER(Resident);
#endif

#ifdef HAVE_PAGE_FAULT_REPORTERS
    REGISTER(PageFaultsSoft);
    REGISTER(PageFaultsHard);
#endif

#ifdef HAVE_PRIVATE_REPORTER
    REGISTER(Private);
#endif

#if defined(HAVE_JEMALLOC_STATS)
    REGISTER(HeapCommitted);
    REGISTER(HeapCommittedUnused);
    REGISTER(HeapCommittedUnusedRatio);
    REGISTER(HeapDirty);
#elif defined(HAVE_HEAP_ZONE0_REPORTERS)
    REGISTER(HeapZone0Committed);
    REGISTER(HeapZone0Used);
#endif

    REGISTER(AtomTable);

#if defined(XP_LINUX)
    InitializeDumpAboutMemoryWatcher();
#endif

    return NS_OK;
}

nsMemoryReporterManager::nsMemoryReporterManager()
  : mMutex("nsMemoryReporterManager::mMutex")
{
}

nsMemoryReporterManager::~nsMemoryReporterManager()
{
}

NS_IMETHODIMP
nsMemoryReporterManager::EnumerateReporters(nsISimpleEnumerator **result)
{
    nsresult rv;
    mozilla::MutexAutoLock autoLock(mMutex);
    rv = NS_NewArrayEnumerator(result, mReporters);
    return rv;
}

NS_IMETHODIMP
nsMemoryReporterManager::EnumerateMultiReporters(nsISimpleEnumerator **result)
{
    nsresult rv;
    mozilla::MutexAutoLock autoLock(mMutex);
    rv = NS_NewArrayEnumerator(result, mMultiReporters);
    return rv;
}

NS_IMETHODIMP
nsMemoryReporterManager::RegisterReporter(nsIMemoryReporter *reporter)
{
    mozilla::MutexAutoLock autoLock(mMutex);
    if (mReporters.IndexOf(reporter) != -1)
        return NS_ERROR_FAILURE;

    mReporters.AppendObject(reporter);
    return NS_OK;
}

NS_IMETHODIMP
nsMemoryReporterManager::RegisterMultiReporter(nsIMemoryMultiReporter *reporter)
{
    mozilla::MutexAutoLock autoLock(mMutex);
    if (mMultiReporters.IndexOf(reporter) != -1)
        return NS_ERROR_FAILURE;

    mMultiReporters.AppendObject(reporter);
    return NS_OK;
}

NS_IMETHODIMP
nsMemoryReporterManager::UnregisterReporter(nsIMemoryReporter *reporter)
{
    mozilla::MutexAutoLock autoLock(mMutex);
    if (!mReporters.RemoveObject(reporter))
        return NS_ERROR_FAILURE;

    return NS_OK;
}

NS_IMETHODIMP
nsMemoryReporterManager::UnregisterMultiReporter(nsIMemoryMultiReporter *reporter)
{
    mozilla::MutexAutoLock autoLock(mMutex);
    if (!mMultiReporters.RemoveObject(reporter))
        return NS_ERROR_FAILURE;

    return NS_OK;
}

NS_IMETHODIMP
nsMemoryReporterManager::GetResident(int64_t *aResident)
{
#if HAVE_VSIZE_AND_RESIDENT_REPORTERS
    return ::GetResident(aResident);
#else
    *aResident = 0;
    return NS_ERROR_NOT_AVAILABLE;
#endif
}

struct MemoryReport {
    MemoryReport(const nsACString &path, int64_t amount) 
    : path(path), amount(amount)
    {
        MOZ_COUNT_CTOR(MemoryReport);
    }
    MemoryReport(const MemoryReport& rhs)
    : path(rhs.path), amount(rhs.amount)
    {
        MOZ_COUNT_CTOR(MemoryReport);
    }
    ~MemoryReport() 
    {
        MOZ_COUNT_DTOR(MemoryReport);
    }
    const nsCString path;
    int64_t amount;
};

#ifdef DEBUG
// This is just a wrapper for int64_t that implements nsISupports, so it can be
// passed to nsIMemoryMultiReporter::CollectReports.
class Int64Wrapper MOZ_FINAL : public nsISupports {
public:
    NS_DECL_ISUPPORTS
    Int64Wrapper() : mValue(0) { }
    int64_t mValue;
};
NS_IMPL_ISUPPORTS0(Int64Wrapper)

class ExplicitNonHeapCountingCallback MOZ_FINAL : public nsIMemoryMultiReporterCallback
{
public:
    NS_DECL_ISUPPORTS

    NS_IMETHOD Callback(const nsACString &aProcess, const nsACString &aPath,
                        int32_t aKind, int32_t aUnits, int64_t aAmount,
                        const nsACString &aDescription,
                        nsISupports *aWrappedExplicitNonHeap)
    {
        if (aKind == nsIMemoryReporter::KIND_NONHEAP &&
            PromiseFlatCString(aPath).Find("explicit") == 0 &&
            aAmount != int64_t(-1))
        {
            Int64Wrapper *wrappedPRInt64 =
                static_cast<Int64Wrapper *>(aWrappedExplicitNonHeap);
            wrappedPRInt64->mValue += aAmount;
        }
        return NS_OK;
    }
};
NS_IMPL_ISUPPORTS1(
  ExplicitNonHeapCountingCallback
, nsIMemoryMultiReporterCallback
)
#endif

NS_IMETHODIMP
nsMemoryReporterManager::GetExplicit(int64_t *aExplicit)
{
    NS_ENSURE_ARG_POINTER(aExplicit);
    *aExplicit = 0;
#ifndef HAVE_EXPLICIT_REPORTER
    return NS_ERROR_NOT_AVAILABLE;
#else
    nsresult rv;
    bool more;

    // Get "heap-allocated" and all the KIND_NONHEAP measurements from normal
    // (i.e. non-multi) "explicit" reporters.
    int64_t heapAllocated = int64_t(-1);
    int64_t explicitNonHeapNormalSize = 0;
    nsCOMPtr<nsISimpleEnumerator> e;
    EnumerateReporters(getter_AddRefs(e));
    while (NS_SUCCEEDED(e->HasMoreElements(&more)) && more) {
        nsCOMPtr<nsIMemoryReporter> r;
        e->GetNext(getter_AddRefs(r));

        int32_t kind;
        rv = r->GetKind(&kind);
        NS_ENSURE_SUCCESS(rv, rv);

        nsCString path;
        rv = r->GetPath(path);
        NS_ENSURE_SUCCESS(rv, rv);

        // We're only interested in NONHEAP explicit reporters and
        // the 'heap-allocated' reporter.
        if (kind == nsIMemoryReporter::KIND_NONHEAP &&
            path.Find("explicit") == 0)
        {
            // Just skip any NONHEAP reporters that fail, because
            // "heap-allocated" is the most important one.
            int64_t amount;
            rv = r->GetAmount(&amount);
            if (NS_SUCCEEDED(rv)) {
                explicitNonHeapNormalSize += amount;
            }
        } else if (path.Equals("heap-allocated")) {
            // If we don't have "heap-allocated", give up, because the result
            // would be horribly inaccurate.
            rv = r->GetAmount(&heapAllocated);
            NS_ENSURE_SUCCESS(rv, rv);
        }
    }

    // For each multi-reporter we could call CollectReports and filter out the
    // non-explicit, non-NONHEAP measurements.  But that's lots of wasted work,
    // so we instead use GetExplicitNonHeap() which exists purely for this
    // purpose.
    //
    // (Actually, in debug builds we also do it the slow way and compare the
    // result to the result obtained from GetExplicitNonHeap().  This
    // guarantees the two measurement paths are equivalent.  This is wise
    // because it's easy for memory reporters to have bugs.)

    int64_t explicitNonHeapMultiSize = 0;
    nsCOMPtr<nsISimpleEnumerator> e2;
    EnumerateMultiReporters(getter_AddRefs(e2));
    while (NS_SUCCEEDED(e2->HasMoreElements(&more)) && more) {
      nsCOMPtr<nsIMemoryMultiReporter> r;
      e2->GetNext(getter_AddRefs(r));
      int64_t n;
      rv = r->GetExplicitNonHeap(&n);
      NS_ENSURE_SUCCESS(rv, rv);
      explicitNonHeapMultiSize += n;
    }

#ifdef DEBUG
    nsRefPtr<ExplicitNonHeapCountingCallback> cb =
      new ExplicitNonHeapCountingCallback();
    nsRefPtr<Int64Wrapper> wrappedExplicitNonHeapMultiSize2 =
      new Int64Wrapper();
    nsCOMPtr<nsISimpleEnumerator> e3;
    EnumerateMultiReporters(getter_AddRefs(e3));
    while (NS_SUCCEEDED(e3->HasMoreElements(&more)) && more) {
      nsCOMPtr<nsIMemoryMultiReporter> r;
      e3->GetNext(getter_AddRefs(r));
      r->CollectReports(cb, wrappedExplicitNonHeapMultiSize2);
    }
    int64_t explicitNonHeapMultiSize2 = wrappedExplicitNonHeapMultiSize2->mValue;

    // Check the two measurements give the same result.  This was an
    // NS_ASSERTION but they occasionally don't match due to races (bug
    // 728990).
    if (explicitNonHeapMultiSize != explicitNonHeapMultiSize2) {
        NS_WARNING(nsPrintfCString("The two measurements of 'explicit' memory "
                                   "usage don't match (%lld vs %lld)",
                                   explicitNonHeapMultiSize,
                                   explicitNonHeapMultiSize2).get());
    }
#endif  // DEBUG

    *aExplicit = heapAllocated + explicitNonHeapNormalSize + explicitNonHeapMultiSize;
    return NS_OK;
#endif // HAVE_HEAP_ALLOCATED_AND_EXPLICIT_REPORTERS
}

NS_IMETHODIMP
nsMemoryReporterManager::GetHasMozMallocUsableSize(bool *aHas)
{
    void *p = malloc(16);
    if (!p) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    size_t usable = moz_malloc_usable_size(p);
    free(p);
    *aHas = !!(usable > 0);
    return NS_OK;
}

NS_IMETHODIMP
nsMemoryReporterManager::DumpMemoryReportsToFile(
    const nsAString& aIdentifier,
    bool aMinimizeMemoryUsage,
    bool aDumpChildProcesses)
{
    // If the identifier is empty, set it to the number of whole seconds since
    // the epoch.  This identifier will appear in our memory report as well as
    // our children's, allowing us to identify which files are from the same
    // memory report request.
    nsString identifier(aIdentifier);
    if (identifier.IsEmpty()) {
        identifier.AppendInt(PR_Now() / 1000000);
    }

    // Kick off memory report dumps in our child processes, if applicable.  We
    // do this before doing our own report because writing a report may be I/O
    // bound, in which case we want to busy the CPU with other reports while we
    // work on our own.
    if (aDumpChildProcesses) {
        nsTArray<ContentParent*> children;
        ContentParent::GetAll(children);
        for (uint32_t i = 0; i < children.Length(); i++) {
            unused << children[i]->SendDumpMemoryReportsToFile(
                identifier, aMinimizeMemoryUsage, aDumpChildProcesses);
        }
    }

    if (aMinimizeMemoryUsage) {
        // Minimize memory usage, then run DumpMemoryReportsToFile again.
        nsRefPtr<DumpMemoryReportsRunnable> callback =
            new DumpMemoryReportsRunnable(identifier,
                                          /* minimizeMemoryUsage = */ false,
                                          /* dumpChildProcesses = */ false);
        return MinimizeMemoryUsage(callback);
    }

    return DumpMemoryReportsToFileImpl(identifier);
}

#define DUMP(o, s) \
    do { \
        nsresult rv = (o)->Write(s); \
        NS_ENSURE_SUCCESS(rv, rv); \
    } while (0)

static nsresult
DumpReport(nsIGZFileWriter *aWriter, bool aIsFirst,
           const nsACString &aProcess, const nsACString &aPath, int32_t aKind,
           int32_t aUnits, int64_t aAmount, const nsACString &aDescription)
{
    DUMP(aWriter, aIsFirst ? "[" : ",");

    // We only want to dump reports for this process.  If |aProcess| is
    // non-NULL that means we've received it from another process in response
    // to a "child-memory-reporter-request" event;  ignore such reports.
    if (!aProcess.IsEmpty()) {
        return NS_OK;
    }

    // Generate the process identifier, which is of the form "$PROCESS_NAME
    // (pid $PID)", or just "(pid $PID)" if we don't have a process name.  If
    // we're the main process, we let $PROCESS_NAME be "Main Process".
    nsAutoCString processId;
    if (XRE_GetProcessType() == GeckoProcessType_Default) {
      // We're the main process.
      processId.AssignLiteral("Main Process ");
    } else if (ContentChild *cc = ContentChild::GetSingleton()) {
      // Try to get the process name from ContentChild.
      nsAutoString processName;
      cc->GetProcessName(processName);
      processId.Assign(NS_ConvertUTF16toUTF8(processName));
      if (!processId.IsEmpty()) {
        processId.AppendLiteral(" ");
      }
    }

    // Add the PID to the identifier.
    unsigned pid = getpid();
    processId.Append(nsPrintfCString("(pid %u)", pid));

    DUMP(aWriter, "\n    {\"process\": \"");
    DUMP(aWriter, processId);

    DUMP(aWriter, "\", \"path\": \"");
    nsCString path(aPath);
    path.ReplaceSubstring("\\", "\\\\");    // escape backslashes for JSON
    DUMP(aWriter, path);

    DUMP(aWriter, "\", \"kind\": ");
    DUMP(aWriter, nsPrintfCString("%d", aKind));

    DUMP(aWriter, ", \"units\": ");
    DUMP(aWriter, nsPrintfCString("%d", aUnits));

    DUMP(aWriter, ", \"amount\": ");
    DUMP(aWriter, nsPrintfCString("%lld", aAmount));

    nsCString description(aDescription);
    description.ReplaceSubstring("\\", "\\\\");    /* <backslash> --> \\ */
    description.ReplaceSubstring("\"", "\\\"");    // " --> \"
    description.ReplaceSubstring("\n", "\\n");     // <newline> --> \n
    DUMP(aWriter, ", \"description\": \"");
    DUMP(aWriter, description);
    DUMP(aWriter, "\"}");

    return NS_OK;
}

class DumpMultiReporterCallback MOZ_FINAL : public nsIMemoryMultiReporterCallback
{
public:
    NS_DECL_ISUPPORTS

    NS_IMETHOD Callback(const nsACString &aProcess, const nsACString &aPath,
                        int32_t aKind, int32_t aUnits, int64_t aAmount,
                        const nsACString &aDescription,
                        nsISupports *aData)
    {
        nsCOMPtr<nsIGZFileWriter> writer = do_QueryInterface(aData);
        NS_ENSURE_TRUE(writer, NS_ERROR_FAILURE);

        // The |isFirst = false| assumes that at least one single reporter is
        // present and so will have been processed in
        // DumpMemoryReportsToFileImpl() below.
        return DumpReport(writer, /* isFirst = */ false, aProcess, aPath,
                          aKind, aUnits, aAmount, aDescription);
        return NS_OK;
    }
};

NS_IMPL_ISUPPORTS1(
  DumpMultiReporterCallback
, nsIMemoryMultiReporterCallback
)

nsresult
nsMemoryReporterManager::DumpMemoryReportsToFileImpl(
    const nsAString& aIdentifier)
{
    // Open a new file named something like
    //
    //   incomplete-memory-report-<-identifier>-<pid>-42.json.gz
    //
    // in NS_OS_TEMP_DIR for writing.  When we're finished writing the report,
    // we'll rename this file and get rid of the "incomplete-" prefix.
    //
    // We do this because we don't want scripts which poll the filesystem
    // looking for memory report dumps to grab a file before we're finished
    // writing to it.

    nsCOMPtr<nsIFile> tmpFile;
    nsresult rv =
        NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(tmpFile));
    NS_ENSURE_SUCCESS(rv, rv);

    // Note that |filename| is missing the "incomplete-" prefix; we'll tack
    // that on in a moment.
    nsAutoCString filename;
    filename.AppendLiteral("memory-report");
    if (!aIdentifier.IsEmpty()) {
        filename.AppendLiteral("-");
        filename.Append(NS_ConvertUTF16toUTF8(aIdentifier));
    }
    filename.AppendLiteral("-");
    filename.AppendInt(getpid());
    filename.AppendLiteral(".json.gz");

    rv = tmpFile->AppendNative(NS_LITERAL_CSTRING("incomplete-") + filename);
    NS_ENSURE_SUCCESS(rv, rv);
   
    rv = tmpFile->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0600);
    NS_ENSURE_SUCCESS(rv, rv);

    nsRefPtr<nsGZFileWriter> writer = new nsGZFileWriter();
    rv = writer->Init(tmpFile);
    NS_ENSURE_SUCCESS(rv, rv);

    // Dump the memory reports to the file.

    // Increment this number if the format changes.
    DUMP(writer, "{\n  \"version\": 1,\n");

    DUMP(writer, "  \"hasMozMallocUsableSize\": ");

    bool hasMozMallocUsableSize;
    GetHasMozMallocUsableSize(&hasMozMallocUsableSize);
    DUMP(writer, hasMozMallocUsableSize ? "true" : "false");
    DUMP(writer, ",\n");
    DUMP(writer, "  \"reports\": ");

    // Process single reporters.
    bool isFirst = true;
    bool more;
    nsCOMPtr<nsISimpleEnumerator> e;
    EnumerateReporters(getter_AddRefs(e));
    while (NS_SUCCEEDED(e->HasMoreElements(&more)) && more) {
        nsCOMPtr<nsIMemoryReporter> r;
        e->GetNext(getter_AddRefs(r));

        nsCString process;
        rv = r->GetProcess(process);
        NS_ENSURE_SUCCESS(rv, rv);

        nsCString path;
        rv = r->GetPath(path);
        NS_ENSURE_SUCCESS(rv, rv);

        int32_t kind;
        rv = r->GetKind(&kind);
        NS_ENSURE_SUCCESS(rv, rv);

        int32_t units;
        rv = r->GetUnits(&units);
        NS_ENSURE_SUCCESS(rv, rv);

        int64_t amount;
        rv = r->GetAmount(&amount);
        NS_ENSURE_SUCCESS(rv, rv);

        nsCString description;
        rv = r->GetDescription(description);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = DumpReport(writer, isFirst, process, path, kind, units, amount,
                        description);
        NS_ENSURE_SUCCESS(rv, rv);

        isFirst = false;
    }

    // Process multi-reporters.
    nsCOMPtr<nsISimpleEnumerator> e2;
    EnumerateMultiReporters(getter_AddRefs(e2));
    nsRefPtr<DumpMultiReporterCallback> cb = new DumpMultiReporterCallback();
    while (NS_SUCCEEDED(e2->HasMoreElements(&more)) && more) {
      nsCOMPtr<nsIMemoryMultiReporter> r;
      e2->GetNext(getter_AddRefs(r));
      r->CollectReports(cb, writer);
    }

    DUMP(writer, "\n  ]\n}");

    rv = writer->Finish();
    NS_ENSURE_SUCCESS(rv, rv);

    // Rename the file, now that we're done dumping the report.  The file's
    // ultimate destination is "memory-report<-identifier>-<pid>.json.gz".

    nsCOMPtr<nsIFile> dstFile;
    rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(dstFile));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = dstFile->AppendNative(filename);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = dstFile->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0600);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoString dstFileName;
    rv = dstFile->GetLeafName(dstFileName);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = tmpFile->MoveTo(/* directory */ nullptr, dstFileName);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIConsoleService> cs =
        do_GetService(NS_CONSOLESERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsString path;
    tmpFile->GetPath(path);
    NS_ENSURE_SUCCESS(rv, rv);

    nsString msg =
        NS_LITERAL_STRING("nsIMemoryReporterManager::dumpReports() dumped reports to ");
    msg.Append(path);
    return cs->LogStringMessage(msg.get());
}

#undef DUMP

namespace {

/**
 * This runnable lets us implement nsIMemoryReporterManager::MinimizeMemoryUsage().
 * We fire a heap-minimize notification, spin the event loop, and repeat this
 * process a few times.
 *
 * When this sequence finishes, we invoke the callback function passed to the
 * runnable's constructor.
 */
class MinimizeMemoryUsageRunnable : public nsRunnable
{
public:
  MinimizeMemoryUsageRunnable(nsIRunnable* aCallback)
    : mCallback(aCallback)
    , mRemainingIters(sNumIters)
  {}

  NS_IMETHOD Run()
  {
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

private:
  // Send sNumIters heap-minimize notifications, spinning the event
  // loop after each notification (see bug 610166 comment 12 for an
  // explanation), because one notification doesn't cut it.
  static const uint32_t sNumIters = 3;

  nsCOMPtr<nsIRunnable> mCallback;
  uint32_t mRemainingIters;
};

} // anonymous namespace

NS_IMETHODIMP
nsMemoryReporterManager::MinimizeMemoryUsage(nsIRunnable* aCallback)
{
  nsRefPtr<MinimizeMemoryUsageRunnable> runnable =
    new MinimizeMemoryUsageRunnable(aCallback);
  return NS_DispatchToMainThread(runnable);
}

NS_IMPL_ISUPPORTS1(nsMemoryReporter, nsIMemoryReporter)

nsMemoryReporter::nsMemoryReporter(nsACString& process,
                                   nsACString& path,
                                   int32_t kind,
                                   int32_t units,
                                   int64_t amount,
                                   nsACString& desc)
: mProcess(process)
, mPath(path)
, mKind(kind)
, mUnits(units)
, mAmount(amount)
, mDesc(desc)
{
}

nsMemoryReporter::~nsMemoryReporter()
{
}

NS_IMETHODIMP nsMemoryReporter::GetProcess(nsACString &aProcess)
{
    aProcess.Assign(mProcess);
    return NS_OK;
}

NS_IMETHODIMP nsMemoryReporter::GetPath(nsACString &aPath)
{
    aPath.Assign(mPath);
    return NS_OK;
}

NS_IMETHODIMP nsMemoryReporter::GetKind(int32_t *aKind)
{
    *aKind = mKind;
    return NS_OK;
}

NS_IMETHODIMP nsMemoryReporter::GetUnits(int32_t *aUnits)
{
  *aUnits = mUnits;
  return NS_OK;
}

NS_IMETHODIMP nsMemoryReporter::GetAmount(int64_t *aAmount)
{
    *aAmount = mAmount;
    return NS_OK;
}

NS_IMETHODIMP nsMemoryReporter::GetDescription(nsACString &aDescription)
{
    aDescription.Assign(mDesc);
    return NS_OK;
}

nsresult
NS_RegisterMemoryReporter (nsIMemoryReporter *reporter)
{
    nsCOMPtr<nsIMemoryReporterManager> mgr = do_GetService("@mozilla.org/memory-reporter-manager;1");
    if (mgr == nullptr)
        return NS_ERROR_FAILURE;
    return mgr->RegisterReporter(reporter);
}

nsresult
NS_RegisterMemoryMultiReporter (nsIMemoryMultiReporter *reporter)
{
    nsCOMPtr<nsIMemoryReporterManager> mgr = do_GetService("@mozilla.org/memory-reporter-manager;1");
    if (mgr == nullptr)
        return NS_ERROR_FAILURE;
    return mgr->RegisterMultiReporter(reporter);
}

nsresult
NS_UnregisterMemoryReporter (nsIMemoryReporter *reporter)
{
    nsCOMPtr<nsIMemoryReporterManager> mgr = do_GetService("@mozilla.org/memory-reporter-manager;1");
    if (mgr == nullptr)
        return NS_ERROR_FAILURE;
    return mgr->UnregisterReporter(reporter);
}

nsresult
NS_UnregisterMemoryMultiReporter (nsIMemoryMultiReporter *reporter)
{
    nsCOMPtr<nsIMemoryReporterManager> mgr = do_GetService("@mozilla.org/memory-reporter-manager;1");
    if (mgr == nullptr)
        return NS_ERROR_FAILURE;
    return mgr->UnregisterMultiReporter(reporter);
}

namespace mozilla {

#ifdef MOZ_DMD

class NullMultiReporterCallback : public nsIMemoryMultiReporterCallback
{
public:
    NS_DECL_ISUPPORTS

    NS_IMETHOD Callback(const nsACString &aProcess, const nsACString &aPath,
                        int32_t aKind, int32_t aUnits, int64_t aAmount,
                        const nsACString &aDescription,
                        nsISupports *aData)
    {
        // Do nothing;  the reporter has already reported to DMD.
        return NS_OK;
    }
};
NS_IMPL_ISUPPORTS1(
  NullMultiReporterCallback
, nsIMemoryMultiReporterCallback
)

void
DMDCheckAndDump()
{
    nsCOMPtr<nsIMemoryReporterManager> mgr =
        do_GetService("@mozilla.org/memory-reporter-manager;1");

    // Do vanilla reporters.
    nsCOMPtr<nsISimpleEnumerator> e;
    mgr->EnumerateReporters(getter_AddRefs(e));
    bool more;
    while (NS_SUCCEEDED(e->HasMoreElements(&more)) && more) {
        nsCOMPtr<nsIMemoryReporter> r;
        e->GetNext(getter_AddRefs(r));

        // Just getting the amount is enough for the reporter to report to DMD.
        int64_t amount;
        (void)r->GetAmount(&amount);
    }

    // Do multi-reporters.
    nsCOMPtr<nsISimpleEnumerator> e2;
    mgr->EnumerateMultiReporters(getter_AddRefs(e2));
    nsRefPtr<NullMultiReporterCallback> cb = new NullMultiReporterCallback();
    while (NS_SUCCEEDED(e2->HasMoreElements(&more)) && more) {
      nsCOMPtr<nsIMemoryMultiReporter> r;
      e2->GetNext(getter_AddRefs(r));
      r->CollectReports(cb, nullptr);
    }

    VALGRIND_DMD_CHECK_REPORTING;
}

#endif  /* defined(MOZ_DMD) */

}
