/* -*- Mode: C++; tab-width: 50; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * mozilla.org
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsServiceManagerUtils.h"
#include "nsMemoryReporterManager.h"
#include "nsArrayEnumerator.h"
#include "nsISimpleEnumerator.h"

#if defined(XP_LINUX) || defined(XP_MACOSX)

#include <sys/time.h>
#include <sys/resource.h>

static PRInt64 GetHardPageFaults()
{
  struct rusage usage;
  int err = getrusage(RUSAGE_SELF, &usage);
  if (err != 0) {
    return PRInt64(-1);
  }

  return usage.ru_majflt;
}

static PRInt64 GetSoftPageFaults()
{
  struct rusage usage;
  int err = getrusage(RUSAGE_SELF, &usage);
  if (err != 0) {
    return PRInt64(-1);
  }

  return usage.ru_minflt;
}

#endif

#if defined(XP_LINUX)

#include <unistd.h>
static PRInt64 GetProcSelfStatmField(int n)
{
    // There are more than two fields, but we're only interested in the first
    // two.
    static const int MAX_FIELD = 2;
    size_t fields[MAX_FIELD];
    NS_ASSERTION(n < MAX_FIELD, "bad field number");
    FILE *f = fopen("/proc/self/statm", "r");
    if (f) {
        int nread = fscanf(f, "%lu %lu", &fields[0], &fields[1]);
        fclose(f);
        return (PRInt64) ((nread == MAX_FIELD) ? fields[n]*getpagesize() : -1);
    }
    return (PRInt64) -1;
}

static PRInt64 GetVsize()
{
    return GetProcSelfStatmField(0);
}

static PRInt64 GetResident()
{
    return GetProcSelfStatmField(1);
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
static PRInt64 GetVsize()
{
    task_basic_info ti;
    return (PRInt64) (GetTaskBasicInfo(&ti) ? ti.virtual_size : -1);
}

static PRInt64 GetResident()
{
    task_basic_info ti;
    return (PRInt64) (GetTaskBasicInfo(&ti) ? ti.resident_size : -1);
}

#elif defined(XP_WIN)

#include <windows.h>
#include <psapi.h>

static PRInt64 GetVsize()
{
  MEMORYSTATUSEX s;
  s.dwLength = sizeof(s);

  bool success = GlobalMemoryStatusEx(&s);
  if (!success)
    return -1;

  return s.ullTotalVirtual - s.ullAvailVirtual;
}

#if MOZ_WINSDK_TARGETVER >= MOZ_NTDDI_LONGHORN
static PRInt64 GetPrivate()
{
    PROCESS_MEMORY_COUNTERS_EX pmcex;
    pmcex.cb = sizeof(PROCESS_MEMORY_COUNTERS_EX);

    if (!GetProcessMemoryInfo(GetCurrentProcess(),
                              (PPROCESS_MEMORY_COUNTERS) &pmcex, sizeof(pmcex)))
    return (PRInt64) -1;

    return pmcex.PrivateUsage;
}

NS_MEMORY_REPORTER_IMPLEMENT(Private,
    "private",
    KIND_OTHER,
    UNITS_BYTES,
    GetPrivate,
    "Memory that cannot be shared with other processes, including memory that "
    "is committed and marked MEM_PRIVATE, data that is not mapped, and "
    "executable pages that have been written to.")
#endif

static PRInt64 GetResident()
{
  PROCESS_MEMORY_COUNTERS pmc;
  pmc.cb = sizeof(PROCESS_MEMORY_COUNTERS);

  if (!GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
      return (PRInt64) -1;

  return pmc.WorkingSetSize;
}

#else

static PRInt64 GetResident()
{
    return (PRInt64) -1;
}

#endif

#if defined(XP_LINUX) || defined(XP_MACOSX) || defined(XP_WIN)
NS_MEMORY_REPORTER_IMPLEMENT(Vsize,
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
#endif

#if defined(XP_LINUX) || defined(XP_MACOSX)
NS_MEMORY_REPORTER_IMPLEMENT(PageFaultsSoft,
    "page-faults-soft",
    KIND_OTHER,
    UNITS_COUNT_CUMULATIVE,
    GetSoftPageFaults,
    "The number of soft page faults (also known as \"minor page faults\") that "
    "have occurred since the process started.  A soft page fault occurs when the "
    "process tries to access a page which is present in physical memory but is "
    "not mapped into the process's address space.  For instance, a process might "
    "observe soft page faults when it loads a shared library which is already "
    "present in physical memory. A process may experience many thousands of soft "
    "page faults even when the machine has plenty of available physical memory, "
    "and because the OS services a soft page fault without accessing the disk, "
    "they impact performance much less than hard page faults.")

NS_MEMORY_REPORTER_IMPLEMENT(PageFaultsHard,
    "page-faults-hard",
    KIND_OTHER,
    UNITS_COUNT_CUMULATIVE,
    GetHardPageFaults,
    "The number of hard page faults (also known as \"major page faults\") that "
    "have occurred since the process started.  A hard page fault occurs when a "
    "process tries to access a page which is not present in physical memory. "
    "The operating system must access the disk in order to fulfill a hard page "
    "fault. When memory is plentiful, you should see very few hard page faults. "
    "But if the process tries to use more memory than your machine has "
    "available, you may see many thousands of hard page faults. Because "
    "accessing the disk is up to a million times slower than accessing RAM, "
    "the program may run very slowly when it is experiencing more than 100 or "
    "so hard page faults a second.")
#endif

NS_MEMORY_REPORTER_IMPLEMENT(Resident,
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

/**
 ** memory reporter implementation for jemalloc and OSX malloc,
 ** to obtain info on total memory in use (that we know about,
 ** at least -- on OSX, there are sometimes other zones in use).
 **/

#if defined(MOZ_MEMORY)
#  if defined(XP_WIN) || defined(SOLARIS) || defined(ANDROID) || defined(XP_MACOSX)
#    define HAVE_JEMALLOC_STATS 1
#    include "jemalloc.h"
#  elif defined(XP_LINUX)
#    define HAVE_JEMALLOC_STATS 1
#    include "jemalloc_types.h"
// jemalloc is directly linked into firefox-bin; libxul doesn't link
// with it.  So if we tried to use jemalloc_stats directly here, it
// wouldn't be defined.  Instead, we don't include the jemalloc header
// and weakly link against jemalloc_stats.
extern "C" {
extern void jemalloc_stats(jemalloc_stats_t* stats)
  NS_VISIBILITY_DEFAULT __attribute__((weak));
}
#  endif  // XP_LINUX
#endif  // MOZ_MEMORY

#if HAVE_JEMALLOC_STATS

static PRInt64 GetHeapUnallocated()
{
    jemalloc_stats_t stats;
    jemalloc_stats(&stats);
    return (PRInt64) stats.mapped - stats.allocated;
}

static PRInt64 GetHeapAllocated()
{
    jemalloc_stats_t stats;
    jemalloc_stats(&stats);
    return (PRInt64) stats.allocated;
}

static PRInt64 GetHeapCommitted()
{
    jemalloc_stats_t stats;
    jemalloc_stats(&stats);
    return (PRInt64) stats.committed;
}

static PRInt64 GetHeapDirty()
{
    jemalloc_stats_t stats;
    jemalloc_stats(&stats);
    return (PRInt64) stats.dirty;
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

static PRInt64 GetHeapUnallocated()
{
    struct mstats stats = mstats();
    return (PRInt64) (stats.bytes_total - stats.bytes_used);
}

static PRInt64 GetHeapAllocated()
{
    struct mstats stats = mstats();
    return (PRInt64) stats.bytes_used;
}

static PRInt64 GetHeapZone0Committed()
{
    malloc_statistics_t stats;
    malloc_zone_statistics(malloc_default_zone(), &stats);
    return stats.size_in_use;
}

static PRInt64 GetHeapZone0Used()
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

#else

static PRInt64 GetHeapAllocated()
{
    return (PRInt64) -1;
}

static PRInt64 GetHeapUnallocated()
{
  return (PRInt64) -1;
}

#endif

NS_MEMORY_REPORTER_IMPLEMENT(HeapUnallocated,
    "heap-unallocated",
    KIND_OTHER,
    UNITS_BYTES,
    GetHeapUnallocated,
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

/**
 ** nsMemoryReporterManager implementation
 **/

NS_IMPL_THREADSAFE_ISUPPORTS1(nsMemoryReporterManager, nsIMemoryReporterManager)

NS_IMETHODIMP
nsMemoryReporterManager::Init()
{
#if HAVE_JEMALLOC_STATS && defined(XP_LINUX)
    if (!jemalloc_stats)
        return NS_ERROR_FAILURE;
#endif

#define REGISTER(_x)  RegisterReporter(new NS_MEMORY_REPORTER_NAME(_x))

    REGISTER(HeapAllocated);
    REGISTER(HeapUnallocated);
    REGISTER(Resident);

#if defined(XP_LINUX) || defined(XP_MACOSX) || defined(XP_WIN)
    REGISTER(Vsize);
#endif

#if defined(XP_LINUX) || defined(XP_MACOSX)
    REGISTER(PageFaultsSoft);
    REGISTER(PageFaultsHard);
#endif

#if defined(XP_WIN) && MOZ_WINSDK_TARGETVER >= MOZ_NTDDI_LONGHORN
    REGISTER(Private);
#endif

#if defined(HAVE_JEMALLOC_STATS) && defined(XP_WIN)
    // heap-committed is only meaningful where we have MALLOC_DECOMMIT defined
    // (currently, just on Windows).  Elsewhere, it's the same as
    // stats->mapped, which is heap-allocated + heap-unallocated.
    //
    // Ideally, we'd check for MALLOC_DECOMMIT in the #if defined above, but
    // MALLOC_DECOMMIT is defined in jemalloc.c, not a header, so we'll just
    // have to settle for the OS check for now.

    REGISTER(HeapCommitted);
#endif

#if defined(HAVE_JEMALLOC_STATS)
    REGISTER(HeapDirty);
#elif defined(XP_MACOSX) && !defined(MOZ_MEMORY)
    REGISTER(HeapZone0Committed);
    REGISTER(HeapZone0Used);
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
nsMemoryReporterManager::GetResident(PRInt64 *aResident)
{
    *aResident = ::GetResident();
    return NS_OK;
}

struct MemoryReport {
    MemoryReport(const nsACString &path, PRInt64 amount) 
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
    PRInt64 amount;
};

// This is just a wrapper for InfallibleTArray<MemoryReport> that implements
// nsISupports, so it can be passed to nsIMemoryMultiReporter::CollectReports.
class MemoryReportsWrapper : public nsISupports {
public:
    NS_DECL_ISUPPORTS
    MemoryReportsWrapper(InfallibleTArray<MemoryReport> *r) : mReports(r) { }
    InfallibleTArray<MemoryReport> *mReports;
};
NS_IMPL_ISUPPORTS0(MemoryReportsWrapper)

class MemoryReportCallback : public nsIMemoryMultiReporterCallback
{
public:
    NS_DECL_ISUPPORTS

    NS_IMETHOD Callback(const nsACString &aProcess, const nsACString &aPath,
                        PRInt32 aKind, PRInt32 aUnits, PRInt64 aAmount,
                        const nsACString &aDescription,
                        nsISupports *aWrappedMRs)
    {
        if (aKind == nsIMemoryReporter::KIND_NONHEAP &&
            PromiseFlatCString(aPath).Find("explicit") == 0 &&
            aAmount != PRInt64(-1)) {

            MemoryReportsWrapper *wrappedMRs =
                static_cast<MemoryReportsWrapper *>(aWrappedMRs);
            MemoryReport mr(aPath, aAmount);
            wrappedMRs->mReports->AppendElement(mr);
        }
        return NS_OK;
    }
};
NS_IMPL_ISUPPORTS1(
  MemoryReportCallback
, nsIMemoryMultiReporterCallback
)

// Is path1 a prefix, and thus a parent, of path2?  Eg. "a/b" is a parent of
// "a/b/c", but "a/bb" is not.
static bool
isParent(const nsACString &path1, const nsACString &path2)
{
    if (path1.Length() >= path2.Length())
        return false;

    const nsACString& subStr = Substring(path2, 0, path1.Length());
    return subStr.Equals(path1) && path2[path1.Length()] == '/';
}

NS_IMETHODIMP
nsMemoryReporterManager::GetExplicit(PRInt64 *aExplicit)
{
    InfallibleTArray<MemoryReport> nonheap;
    PRInt64 heapUsed = PRInt64(-1);

    // Get "heap-allocated" and all the KIND_NONHEAP measurements from vanilla
    // "explicit" reporters.
    nsCOMPtr<nsISimpleEnumerator> e;
    EnumerateReporters(getter_AddRefs(e));

    bool more;
    while (NS_SUCCEEDED(e->HasMoreElements(&more)) && more) {
        nsCOMPtr<nsIMemoryReporter> r;
        e->GetNext(getter_AddRefs(r));

        PRInt32 kind;
        nsresult rv = r->GetKind(&kind);
        NS_ENSURE_SUCCESS(rv, rv);

        nsCString path;
        rv = r->GetPath(path);
        NS_ENSURE_SUCCESS(rv, rv);

        // We're only interested in NONHEAP explicit reporters and
        // the 'heap-allocated' reporter.
        if (kind == nsIMemoryReporter::KIND_NONHEAP &&
            path.Find("explicit") == 0) {

            PRInt64 amount;
            rv = r->GetAmount(&amount);
            NS_ENSURE_SUCCESS(rv, rv);

            // Just skip any NONHEAP reporters that fail, because
            // "heap-allocated" is the most important one.
            if (amount != PRInt64(-1)) {
                MemoryReport mr(path, amount);
                nonheap.AppendElement(mr);
            }
        } else if (path.Equals("heap-allocated")) {
            rv = r->GetAmount(&heapUsed);
            NS_ENSURE_SUCCESS(rv, rv);
            // If "heap-allocated" fails, we give up, because the result
            // would be horribly inaccurate.
            if (heapUsed == PRInt64(-1)) {
                *aExplicit = PRInt64(-1);
                return NS_OK;
            }
        }
    }

    // Get KIND_NONHEAP measurements from multi-reporters, too.
    nsCOMPtr<nsISimpleEnumerator> e2;
    EnumerateMultiReporters(getter_AddRefs(e2));
    nsRefPtr<MemoryReportsWrapper> wrappedMRs =
        new MemoryReportsWrapper(&nonheap);

    // This callback adds only NONHEAP explicit reporters.
    nsRefPtr<MemoryReportCallback> cb = new MemoryReportCallback();

    while (NS_SUCCEEDED(e2->HasMoreElements(&more)) && more) {
      nsCOMPtr<nsIMemoryMultiReporter> r;
      e2->GetNext(getter_AddRefs(r));
      r->CollectReports(cb, wrappedMRs);
    }

    // Ignore (by zeroing its amount) any reporter that is a child of another
    // reporter.  Eg. if we have "explicit/a" and "explicit/a/b", zero the
    // latter.  This is quadratic in the number of explicit NONHEAP reporters,
    // but there shouldn't be many.
    for (PRUint32 i = 0; i < nonheap.Length(); i++) {
        const nsCString &iPath = nonheap[i].path;
        for (PRUint32 j = i + 1; j < nonheap.Length(); j++) {
            const nsCString &jPath = nonheap[j].path;
            if (isParent(iPath, jPath)) {
                nonheap[j].amount = 0;
            } else if (isParent(jPath, iPath)) {
                nonheap[i].amount = 0;
            }
        }
    }

    // Sum all the nonheap reporters and heapUsed.
    *aExplicit = heapUsed;
    for (PRUint32 i = 0; i < nonheap.Length(); i++) {
        *aExplicit += nonheap[i].amount;
    }

    return NS_OK;
}

NS_IMPL_ISUPPORTS1(nsMemoryReporter, nsIMemoryReporter)

nsMemoryReporter::nsMemoryReporter(nsACString& process,
                                   nsACString& path,
                                   PRInt32 kind,
                                   PRInt32 units,
                                   PRInt64 amount,
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

NS_IMETHODIMP nsMemoryReporter::GetKind(PRInt32 *aKind)
{
    *aKind = mKind;
    return NS_OK;
}

NS_IMETHODIMP nsMemoryReporter::GetUnits(PRInt32 *aUnits)
{
  *aUnits = mUnits;
  return NS_OK;
}

NS_IMETHODIMP nsMemoryReporter::GetAmount(PRInt64 *aAmount)
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
    if (mgr == nsnull)
        return NS_ERROR_FAILURE;
    return mgr->RegisterReporter(reporter);
}

nsresult
NS_RegisterMemoryMultiReporter (nsIMemoryMultiReporter *reporter)
{
    nsCOMPtr<nsIMemoryReporterManager> mgr = do_GetService("@mozilla.org/memory-reporter-manager;1");
    if (mgr == nsnull)
        return NS_ERROR_FAILURE;
    return mgr->RegisterMultiReporter(reporter);
}

nsresult
NS_UnregisterMemoryReporter (nsIMemoryReporter *reporter)
{
    nsCOMPtr<nsIMemoryReporterManager> mgr = do_GetService("@mozilla.org/memory-reporter-manager;1");
    if (mgr == nsnull)
        return NS_ERROR_FAILURE;
    return mgr->UnregisterReporter(reporter);
}

nsresult
NS_UnregisterMemoryMultiReporter (nsIMemoryMultiReporter *reporter)
{
    nsCOMPtr<nsIMemoryReporterManager> mgr = do_GetService("@mozilla.org/memory-reporter-manager;1");
    if (mgr == nsnull)
        return NS_ERROR_FAILURE;
    return mgr->UnregisterMultiReporter(reporter);
}

